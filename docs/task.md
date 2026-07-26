# Task\<T, CP\>

带返回值的协程包装，对标 C++20 `co_return` 的结果载体。

## 用法

```cpp
#include "task.h"

// 创建
auto task = hco::Task<int>::from([]() -> int {
    int a = hco::await<CACSContext>(some_op());
    return a * 2;
});

// 调度运行
hco::Coroutine<CACSContext>::start({task.coroutine()});

// 取结果（阻塞）
int result = task.get();
```

## API

| 方法 | 说明 |
|------|------|
| `Task::from(F&& f)` | 静态工厂，从可调用对象创建 Task。`f` 的返回类型推导为 `T`，支持 `void` |
| `T get()` | 阻塞等待协程结束，返回结果。有异常则 `rethrow`。可多次调用（内部用 `shared_future`） |
| `coroutine()` | 返回 `shared_ptr<Coroutine<CP>>`，交给 `start()` 调度 |
| `handle()` | 返回 `coroutine_handle<CP>` |

## 模板参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `T` | —— | 返回值类型，`void` 表示无返回值 |
| `CP` | `CACSContext` | 上下文切换策略，对应 `Coroutine` 的模板参数 |

## 实现

内部三个成员：

- `shared_ptr<Coroutine<CP>>` — 包装用户函数为 `packaged_task<void()>`，交给调度器执行
- `shared_ptr<promise<T>>` — 用户函数结束后设置返回值或异常
- `shared_future<T>` — 从 `promise` 获取，供 `get()` 读取

用户函数正常 `return` 即可，`Task::from` 自动生成 wrapper lambda：`try { promise->set_value(f()); } catch { promise->set_exception(...); }`。
