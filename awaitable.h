#ifndef AWAITABLE_H_
#define AWAITABLE_H_

#include "coroutine_handle.h"

namespace hco {

template<typename CP>
struct suspend_always_t {
  bool await_ready() const { return false; }
  void await_suspend(coroutine_handle<CP> h) { h_ = h; }
  void await_resume() const {}
  void resume() { h_.resume(); }
 private:
  coroutine_handle<CP> h_;
};

struct suspend_never_t {
  bool await_ready() const { return true; }
  template<typename CP>
  void await_suspend(coroutine_handle<CP>) {}
  void await_resume() const {}
};

template<typename CP, typename Awaitable>
decltype(auto) await(Awaitable&& a) {
  if (!a.await_ready()) {
    auto h = this_coroutine<CP>();
    using suspend_result = decltype(a.await_suspend(h));
    if constexpr (std::is_void_v<suspend_result>) {
      a.await_suspend(h);
      h.suspend();
    } else {
      auto next = a.await_suspend(h);
      h.swap_to(next);
    }
  }
  return a.await_resume();
}

// yield_to: 对称转移——await_suspend 返回目标 handle，直接 swap 过去
template<typename CP>
struct yield_to_t {
  coroutine_handle<CP> target;
  bool await_ready() const { return false; }
  coroutine_handle<CP> await_suspend(coroutine_handle<CP>) { return target; }
  void await_resume() const {}
};

}  // namespace hco

#endif  // AWAITABLE_H_
