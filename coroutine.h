#ifndef COROUTINE_H_
#define COROUTINE_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <ucontext.h>
#include <future>

#include "id.h"

namespace hco {

enum class Status {
  DEAD,
  WAITING,
  READY,
  RUNNING
};

class Coroutine : public std::enable_shared_from_this<Coroutine> {
 public:
  using task_t = std::function<void()>;

  ~Coroutine() = default;

  // 开始运行所有任务
  static std::future<void> start();
  static void main_func();

  template<typename F, typename... Args>
  static std::shared_ptr<Coroutine> create(F&& task, Args&&... args) {
    auto wrapper = [task_func = std::decay_t<F>(std::forward<F>(task)),
                    ...args_copy = std::decay_t<Args>(std::forward<Args>(args))]() mutable {
      task_func(args_copy...);
    };
    // 不使用make_shared，因为其不能访问private的构造函数
    auto raw_ptr = new Coroutine(wrapper);
    auto co = std::shared_ptr<Coroutine>(raw_ptr);
    coroutine_map.emplace(co->id_, co);
    return co;
  }

  // 暂停当前协程，切换为另一个协程
  void yield();
  // 恢复当前协程
  void resume();

  size_t get_id() const { return id_.get_id(); }
  
 private:
  Coroutine(task_t task);
  Coroutine(task_t task, Id id);

  void make_context();
  static constexpr std::size_t STACK_SIZE = 1024 * 1024;
  
  static void task_wrapper(void* coroutine);

  // 正在运行协程的Id
  static Id running_coroutine_id;

  // <id, co> 所有的协程
  static std::unordered_map<Id, std::shared_ptr<Coroutine>> coroutine_map;

  Id id_;
  Status status_;
  task_t task_;
  ucontext_t context_;
  std::aligned_storage_t<STACK_SIZE> stack_;
};

}

#endif // COROUTINE_H_ 