#ifndef COROUTINE_H_
#define COROUTINE_H_

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <ucontext.h>
#include <future>
#include <cassert>
#include "context_policy.h"

#include "id.h"

namespace hco {

enum class Status {
  DEAD,
  WAITING,
  READY,
  RUNNING
};

template<typename ContextPolicy = UContext>
class Coroutine : public std::enable_shared_from_this<Coroutine<ContextPolicy>> {
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
  struct ThreadResources {
    Id running_coroutine_id_{Id::get_invalid_id()};
    std::unordered_map<Id, std::shared_ptr<Coroutine<ContextPolicy>>> co_map_;
    using CoroutineMap = std::unordered_map<Id, std::shared_ptr<Coroutine<ContextPolicy>>>;
    ThreadResources() = default;
  };

  // 用于创建非main的普通协程
  Coroutine(task_t task, std::shared_ptr<ThreadResources> thread_resources = nullptr);
  // 调度器协程的构造函数：task为main
  Coroutine(std::shared_ptr<ThreadResources> thread_resources = nullptr);

  void set_thread_resources(std::shared_ptr<ThreadResources> thread_resources) {
    thread_resources_ = thread_resources;
  }
  void make_context();
  
  static void task_wrapper(void* coroutine);
  void main_func();

  void init_main_task();

  
  Id id_;
  Status status_;
  std::packaged_task<void()> task_;
  std::unique_ptr<ContextPolicy> context_;

  std::shared_ptr<ThreadResources> thread_resources_;
};

template<typename ContextPolicy>
std::vector<std::future<void>> Coroutine<ContextPolicy>::start(const std::vector<std::shared_ptr<Coroutine>>& co_list) {
  std::vector<std::future<void>> future_list;
  for (const auto& co : co_list) {
    future_list.emplace_back(co->task_.get_future());
  }

  auto thread_resources = std::make_shared<ThreadResources>();
  for (auto co : co_list) {
    co->set_thread_resources(thread_resources);
    thread_resources->co_map_.emplace(co->id_, co);
  }

  // 在std::make_shared内部无法访问private的构造函数，所以用下面的方式创建Coroutine
  auto raw_ptr = new Coroutine(thread_resources);
  auto co = std::shared_ptr<Coroutine>(raw_ptr);
  thread_resources->co_map_.emplace(co->id_, co);

  auto task = [=]() { co->task_(); };
  std::thread(task).detach();
  return future_list;
}

template<typename ContextPolicy>
void Coroutine<ContextPolicy>::main_func() {
  auto& co_map = thread_resources_->co_map_;
  auto& running_id = thread_resources_->running_coroutine_id_;
  auto current_co = co_map[Id(Id::get_main_co_id())];

  while (co_map.size() > 1) {
    // 删除DEAD协程
    std::erase_if(co_map, [](const auto& pair) {
      assert(pair.second != nullptr);
      return pair.second->status_ == Status::DEAD;
    });

    std::shared_ptr<Coroutine> next_co = nullptr;
    for (auto& [id, co] : co_map) {
      if (id == current_co->id_ || co->status_ != Status::READY) {
        continue;
      }
      assert(co != nullptr);
      next_co = co;
      break;
    }
    // 未找到任何READY协程
    if (next_co == nullptr) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(10ms);
      continue;
    }
    // 切换到READY协程
    current_co->status_ = Status::READY;
    running_id = next_co->id_;
    next_co->status_ = Status::RUNNING;
    
    current_co->context_->swap(*next_co->context_);
    
    // 回到当前协程
    assert(current_co->status_ == Status::RUNNING);
    assert(running_id == current_co->id_);
  }
  running_id = Id::get_invalid_id();
}

template<typename ContextPolicy>
void Coroutine<ContextPolicy>::task_wrapper(void* coroutine) {
  auto co = reinterpret_cast<Coroutine *>(coroutine);
  co->task_();
  co->status_ = Status::DEAD;
  co->yield();
}

template<typename ContextPolicy>
void Coroutine<ContextPolicy>::yield() {
  assert(thread_resources_->running_coroutine_id_ == id_);
  // 在yield之前修改状态
  assert(status_ != Status::RUNNING);

  auto next_id = Id::get_invalid_id();
  for (auto& [id, co] : thread_resources_->co_map_) {
    if (co->status_ == Status::READY) {
      next_id = id;
      break;
    }
  }
  assert(next_id.is_valid());
  
  const auto& next_co = thread_resources_->co_map_[next_id];
  next_co->status_ = Status::RUNNING;
  thread_resources_->running_coroutine_id_ = next_id;
  
  context_->swap(*next_co->context_);
}

template<typename ContextPolicy>
void Coroutine<ContextPolicy>::resume() {
  auto& running_id = thread_resources_->running_coroutine_id_;
  assert(running_id != id_);
  auto running_co = thread_resources_->co_map_[running_id];
  assert(running_co->status_ == Status::RUNNING);
  assert(status_ != Status::RUNNING);

  status_ = Status::RUNNING;
  running_id = id_;

  context_->swap(*running_co->context_);
}

template<typename ContextPolicy>
void Coroutine<ContextPolicy>::make_context() {
  context_ = std::make_unique<ContextPolicy>();
  context_->init_task(task_wrapper, this);
  context_->init();
}

template<typename ContextPolicy>
Coroutine<ContextPolicy>::Coroutine(task_t task, std::shared_ptr<ThreadResources> thread_resources)
: status_(Status::READY), task_(std::packaged_task<void()>(task)), thread_resources_(thread_resources) {
  make_context();
}

template<typename ContextPolicy>
Coroutine<ContextPolicy>::Coroutine(std::shared_ptr<ThreadResources> thread_resources)
: id_(Id::get_main_co_id()), status_(Status::READY), task_(std::packaged_task<void()>([this]() { this->main_func(); })), thread_resources_(thread_resources) {
  make_context();
}

}

#endif // COROUTINE_H_ 