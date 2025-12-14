#include "coroutine.h"
#include <future>
#include <iostream>
#include <thread>
#include <ucontext.h>
#include <utility>
#include <cassert>
#include <vector>

namespace hco {

std::atomic<size_t> Id::global_increment_id{2};

std::vector<std::future<void>> Coroutine::start(const std::vector<std::shared_ptr<Coroutine>>& co_list) {
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

void Coroutine::main_func() {
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
    swapcontext(&current_co->context_, &next_co->context_);
    // 回到当前协程
    assert(current_co->status_ == Status::RUNNING);
    assert(running_id == current_co->id_);
  }
  running_id = Id::get_invalid_id();
}

void Coroutine::task_wrapper(void* coroutine) {
  auto co = reinterpret_cast<Coroutine *>(coroutine);
  co->task_();
  co->status_ = Status::DEAD;
  co->yield();
}

void Coroutine::yield() {
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
  swapcontext(&context_, &next_co->context_);
}

void Coroutine::resume() {
  auto& running_id = thread_resources_->running_coroutine_id_;
  assert(running_id != id_);
  auto running_co = thread_resources_->co_map_[running_id];
  assert(running_co->status_ == Status::RUNNING);
  assert(status_ != Status::RUNNING);

  status_ = Status::RUNNING;
  running_id = id_;
  swapcontext(&context_, &running_co->context_);
}

void Coroutine::make_context() {
  getcontext(&context_);
  context_.uc_link = nullptr;
  context_.uc_stack.ss_size = STACK_SIZE;
  context_.uc_stack.ss_sp = &stack_;
  makecontext(&context_, reinterpret_cast<void(*)()>(task_wrapper), 1, this);
}

Coroutine::Coroutine(task_t task, std::shared_ptr<ThreadResources> thread_resources)
: status_(Status::READY), task_(std::packaged_task<void()>(task)), thread_resources_(thread_resources) {
  make_context();
}

Coroutine::Coroutine(std::shared_ptr<ThreadResources> thread_resources)
: id_(Id::get_main_co_id()), status_(Status::READY), task_(std::packaged_task<void()>([this]() { this->main_func(); })), thread_resources_(thread_resources) {
  make_context();
}

}