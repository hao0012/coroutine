#ifndef TASK_H_
#define TASK_H_

#include <future>
#include <memory>
#include <exception>
#include "coroutine.h"

namespace hco {

// Task<T, CP> — 带返回值的协程。
// 用法:
//   auto task = Task<int>::from([]() -> int { return 42; });
//   Coroutine<CP>::start({task.coroutine()});
//   int result = task.get();
template<typename T, typename CP = CACSContext>
class Task {
 public:
  Task() = default;

  template<typename F>
  static Task from(F&& f) {
    Task task;
    task.promise_ = std::make_shared<std::promise<T>>();
    task.future_ = task.promise_->get_future().share();

    task.coro_ = Coroutine<CP>::create_task(
        [f = std::decay_t<F>(std::forward<F>(f)), p = task.promise_]() mutable {
          try {
            if constexpr (std::is_void_v<T>) {
              f();
              p->set_value();
            } else {
              p->set_value(f());
            }
          } catch (...) {
            p->set_exception(std::current_exception());
          }
        });

    return task;
  }

  T get() { return future_.get(); }
  std::shared_ptr<Coroutine<CP>> coroutine() { return coro_; }
  coroutine_handle<CP> handle() { return coro_->handle(); }

 private:
  std::shared_ptr<Coroutine<CP>> coro_;
  std::shared_ptr<std::promise<T>> promise_;
  std::shared_future<T> future_;
};

}  // namespace hco

#endif  // TASK_H_
