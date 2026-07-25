#ifndef AWAITABLE_H_
#define AWAITABLE_H_

#include "coroutine_handle.h"

namespace hco {

// ========== 标准 Awaitable ==========

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

// ========== await 函数模板 ==========
template<typename CP, typename Awaitable>
decltype(auto) await(Awaitable&& a) {
  if (!a.await_ready()) {
    auto h = this_coroutine<CP>();
    a.await_suspend(h);
    h.suspend();
  }
  return a.await_resume();
}

}  // namespace hco

#endif  // AWAITABLE_H_
