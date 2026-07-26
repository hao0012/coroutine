# Generator\<T\>

惰性序列生成器，支持 range-for 遍历。完全自包含，不依赖 `Coroutine` / `ThreadResources` / 调度器——直接用两个 `CACSContext` 做双向上下文切换。

## 用法

```cpp
#include "generator.h"

hco::Generator<int> gen([](hco::Generator<int>* g) {
    for (int i = 0; i < 5; ++i) {
        g->yield_value(i * 10);
    }
});

for (int v : gen) {          // 0 10 20 30 40
    std::cout << v << " ";
}
```

## API

| 方法 | 说明 |
|------|------|
| `Generator(F&& f)` | 构造函数，`f` 签名为 `void(Generator<T>*)` |
| `void yield_value(T val)` | 在生成器函数内调用，产出值并挂起 |
| `begin()` | 启动生成器并执行到第一个 `yield_value`，返回 iterator |
| `end()` | 返回 `std::default_sentinel_t` |

## 实现原理

```
begin() / operator++():
  caller_ctx_.swap(gen_ctx_) → 跳到生成器栈，执行到 yield 或结束

yield_value(val):
  存值 → gen_ctx_.swap(caller_ctx_) → 跳回调用者

gen_entry (用户函数结束):
  done = true → gen_ctx_.swap(caller_ctx_) → 跳回，循环检查 done 退出
```

两个 `CACSContext`：
- `gen_ctx_` — 生成器栈，`init()` 时铺设初始上下文
- `caller_ctx_` — 调用者上下文，不初始化，只在 swap 时写入 rsp

`caller_ctx_` 不需要 `init()`——首次 `begin()` 的 swap 会把当前 rsp 写入 `caller_ctx_.rsp_`，后续 `yield_value` 和 `operator++` 都通过读写这个 rsp 来切回。caller_ctx_ 底下的 1MB 栈 buffer 实际上从未被用到。

## 限制

- 空生成器（无 yield 直接返回）：`begin()` 执行后 `done = true`，`begin() == end()`，range-for 不进入循环体，安全
- `yield_value` 只能从生成器函数内调用，其他地方调用行为未定义
- 不支持值类型的 `co_yield` 语法（需要编译器支持），但语义等价

## 与 C++20 的差异

| | C++20 `std::generator<T>` | hco `Generator<T>` |
|---|---|---|
| 实现 | 编译器将函数体拆成状态机 | 有栈协程，直接上下文切换 |
| `co_yield` | 编译器关键字 | `g->yield_value(val)` 方法调用 |
| 范围 for | `for (auto& v : gen)` | `for (auto v : gen)` |
| 引用支持 | 支持 `co_yield` 表达式引用 | 当前只支持值语义 |
