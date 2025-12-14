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

struct ThreadResources;

class Coroutine : public std::enable_shared_from_this<Coroutine> {
 public:
  using task_t = std::function<void()>;

  ~Coroutine() = default;

  // 开始运行所有任务
  static std::vector<std::future<void>> start(const std::vector<std::shared_ptr<Coroutine>>& task_list);

  template<typename F, typename... Args>
  static std::shared_ptr<Coroutine> create_task(F&& task, Args&&... args) {
    auto wrapper = [task_func = std::decay_t<F>(std::forward<F>(task)),
                    ...args_copy = std::decay_t<Args>(std::forward<Args>(args))]() mutable {
      task_func(args_copy...);
    };
    // 不使用make_shared，因为其不能访问private的构造函数
    auto raw_ptr = new Coroutine(wrapper);
    return std::shared_ptr<Coroutine>(raw_ptr);
  }

  // 暂停当前协程，切换为另一个协程
  void yield();
  // 恢复当前协程
  void resume();

  size_t get_id() const { return id_.get_id(); }
  
 private:
  friend struct ThreadResources;
  // 用于创建非main的普通协程
  Coroutine(task_t task, std::shared_ptr<ThreadResources> thread_resources = nullptr);
  // 调度器协程的构造函数：task为main
  Coroutine(std::shared_ptr<ThreadResources> thread_resources = nullptr);

  void set_thread_resources(std::shared_ptr<ThreadResources> thread_resources) {
    thread_resources_ = thread_resources;
  }
  void make_context();
  static constexpr std::size_t STACK_SIZE = 1024 * 1024;
  
  static void task_wrapper(void* coroutine);
  void main_func();

  void init_main_task();

  
  Id id_;
  Status status_;
  std::packaged_task<void()> task_;
  ucontext_t context_;
  std::aligned_storage_t<STACK_SIZE> stack_;

  std::shared_ptr<ThreadResources> thread_resources_;
};

struct ThreadResources {
  Id running_coroutine_id_{Id::get_invalid_id()};
  std::unordered_map<Id, std::shared_ptr<Coroutine>> co_map_;
  using CoroutineMap = std::unordered_map<Id, std::shared_ptr<Coroutine>>;
  ThreadResources() = default;
};

}

#endif // COROUTINE_H_ 