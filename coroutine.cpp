#include "coroutine.h"
#include <iostream>
#include <thread>
#include <ucontext.h>
#include <utility>
#include <cassert>
#include <vector>

namespace hco {

std::atomic<size_t> Id::global_increment_id{2};

Id Coroutine::running_coroutine_id{Id::get_invalid_id()};

// <id, co> 所有的协程
std::unordered_map<Id, std::shared_ptr<Coroutine>> Coroutine::coroutine_map;

std::future<void> Coroutine::start() {
  auto task = []() {
    // 不使用make_shared，因为其不能访问private的构造函数
    auto raw_ptr = new Coroutine(main_func, Id::get_main_co_id());
    auto co = std::shared_ptr<Coroutine>(raw_ptr);
    coroutine_map.emplace(co->id_, co);
    co->task_();
  };
  auto future = std::async(std::launch::async, task);
  return future;
}

void Coroutine::main_func() {
  auto current = coroutine_map[Id(Id::get_main_co_id())];
  while (coroutine_map.size() > 1) {
    Id next_id = Id::get_invalid_id();
    std::erase_if(coroutine_map, [](const auto& pair) {
      assert(pair.second != nullptr);
      return pair.second->status_ == Status::DEAD;
    });
    for (auto& [id, co] : coroutine_map) {
      if (id == current->id_ || co->status_ != Status::READY) {
        continue;
      }
      assert(co != nullptr);
      // 有状态为READY的协程可以进行切换
      next_id = id;
      current->status_ = Status::READY;
      running_coroutine_id = next_id;
      co->status_ = Status::RUNNING;
      swapcontext(&current->context_, &co->context_);
      assert(current->status_ == Status::RUNNING);
      assert(running_coroutine_id == current->id_);
      break;
    }

    if (!next_id.is_valid()) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(10ms);
      continue;
    }
  }
  current->running_coroutine_id = Id::get_invalid_id();
}

void Coroutine::task_wrapper(void* coroutine) {
  auto co = reinterpret_cast<Coroutine *>(coroutine);
  co->task_();
  co->status_ = Status::DEAD;
  co->yield();
}

void Coroutine::yield() {
  assert(Coroutine::running_coroutine_id == id_);
  // 在yield之前修改状态
  assert(status_ != Status::RUNNING);

  auto next_id = Id::get_invalid_id();
  for (auto& [id, co] : coroutine_map) {
    if (co->status_ == Status::READY) {
      next_id = id;
      break;
    }
  }
  assert(next_id.is_valid());
  
  const auto& next_co = coroutine_map[next_id];
  next_co->status_ = Status::RUNNING;
  running_coroutine_id = next_id;
  swapcontext(&context_, &next_co->context_);
}

void Coroutine::resume() {
  assert(Coroutine::running_coroutine_id != id_);
  const auto running_id = Coroutine::running_coroutine_id;
  auto running_co = coroutine_map[running_id];
  assert(running_co->status_ == Status::RUNNING);
  assert(status_ != Status::RUNNING);

  status_ = Status::RUNNING;
  running_coroutine_id = id_;
  swapcontext(&context_, &running_co->context_);
}

void Coroutine::make_context() {
  getcontext(&context_);
  context_.uc_link = nullptr;
  context_.uc_stack.ss_size = STACK_SIZE;
  context_.uc_stack.ss_sp = &stack_;
  makecontext(&context_, reinterpret_cast<void(*)()>(task_wrapper), 1, this);
}

Coroutine::Coroutine(task_t task): status_(Status::READY), task_(task) {
  make_context();
}

Coroutine::Coroutine(task_t task, Id id): id_(id), status_(Status::READY), task_(task) {
  make_context();
}

}