# Awaitable 协议

对标 C++20 `co_await` 的库级实现。得益于有栈协程，`await()` 是普通函数模板，不需要编译器魔法。

## Awaitable 概念

一个 Awaitable 需提供三个方法，由 `await()` 按下列顺序调用：

```
await_ready()     → true?  跳过 suspend，直接 await_resume() 取结果
                 → false?  await_suspend(handle) → handle.suspend() → 切走
                            (被外部 resume 后) → await_resume() 取结果
```

## 用法

```cpp
#include "awaitable.h"

using CoCP = CACSContext;

// suspend_never — 条件已满足，立即通过
hco::await<CoCP>(hco::suspend_never_t{});

// suspend_always — 挂起，等外部 resume
auto sa = hco::suspend_always_t<CoCP>{};
// 协程 A: hco::await<CoCP>(sa);   → 挂起
// 协程 B: sa.resume();             → 唤醒 A
```

## 标准 Awaitable

| 类型 | `await_ready()` | `await_suspend` | `await_resume` | 用途 |
|------|-----------------|-----------------|----------------|------|
| `suspend_never_t` | `true` | 不调用 | 直接返回 | 条件已就绪，不挂起 |
| `suspend_always_t<CP>` | `false` | 存 `coroutine_handle` | 直接返回 | 挂起等待外部 `resume()` |

## 自定义 Awaitable

实现三个方法即可，例如 epoll IO：

```cpp
struct socket_read {
  int fd;
  char* buf;
  size_t n;
  int result = 0;

  bool await_ready() {
    result = try_read_nonblock(fd, buf, n);
    return result != -1;  // 有数据就跳过 suspend
  }

  void await_suspend(coroutine_handle h) {
    event_loop::register_fd(fd, EPOLLIN, [h]() { h.resume(); });
  }

  int await_resume() { return result; }
};
```

## `await()` 函数模板

```cpp
template<typename CP, typename Awaitable>
decltype(auto) await(Awaitable&& a) {
  if (!a.await_ready()) {
    auto h = this_coroutine<CP>();
    a.await_suspend(h);
    h.suspend();   // 协程 → WAITING → yield
  }
  return a.await_resume();
}
```

与宏不同，`await()` 是有栈协程下的普通函数调用——`h.suspend()` 内调 `yield()` 保存当前栈指针后切走，被 `resume` 时从 `suspend()` 返回，继续执行 `await_resume()`。调用栈自然停在 `await()` 内部，不需要编译器切开函数体。

## 限制

- `suspend_always::resume()` 必须在协程线程内调用（CACS 上下文切换假设单线程）。跨线程需要事件循环 marshaling。
- 暂不支持 `await_suspend` 返回 `coroutine_handle` 的对称转移。

## 对称转移

当 `await_suspend` 返回 `coroutine_handle`（而非 `void`）时，`await()` 不走调度器，直接从当前协程 swap 到目标协程。

```
普通路径:  A → suspend → scheduler → pick B → swap    (2 次上下文切换)
对称转移:  A → swap_to(B)                               (1 次上下文切换)
```

### 用法

```cpp
template<typename CP>
struct yield_to_t {
    coroutine_handle<CP> target;
    bool await_ready() const { return false; }
    coroutine_handle<CP> await_suspend(coroutine_handle<CP>) { return target; }
    void await_resume() const {}
};

// 协程 A 直接跳到 B：
hco::await<CoCP>(hco::yield_to_t<CoCP>{B_handle});
```

### 实现

`await()` 用 `if constexpr` 检测 `await_suspend` 返回类型：

```cpp
using suspend_result = decltype(a.await_suspend(h));
if constexpr (std::is_void_v<suspend_result>) {
    a.await_suspend(h);
    h.suspend();         // 走调度器
} else {
    auto next = a.await_suspend(h);
    h.swap_to(next);     // 对称转移
}
```

`swap_to()` 调用 `Coroutine::symmetric_swap_to(target)`：
- 当前协程 → `WAITING`
- 更新 `running_coroutine_id_` 为目标 id
- `context_->swap(*target.context_)`，直接跳转

### 限制

- 对称转移后源协程进入 `WAITING`，需要有人显式 `resume` 才能继续。`await_suspend` 通常应同时存储源协程的 handle
- 跨协程链时每步都是 WAITING，最终需要事件循环或另一个协程逐层唤醒
