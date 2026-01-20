#ifndef CONTEXT_POLICY_H_
#define CONTEXT_POLICY_H_

#include <cstddef>
#include <iostream>
#include <type_traits>
#include <ucontext.h>
#include <functional>
#include <cassert>

#include "cacs.h"

class ContextPolicy {
 public:
  template<typename F, typename... Args> 
  void init_task(F&& f, Args&&... args) {
    task_ = [task_func = std::decay_t<F>(std::forward<F>(f)),
                    ...args_copy = std::decay_t<Args>(std::forward<Args>(args))]() mutable {
      task_func(args_copy...);
    };
  }

  virtual void init() = 0;
  virtual void swap(ContextPolicy& target) = 0;
  virtual ~ContextPolicy() = default;
 protected:
  static constexpr std::size_t STACK_SIZE = 1024 * 1024;
  static constexpr std::size_t STACK_ALIGNMENT = 16;
  std::aligned_storage_t<STACK_SIZE, STACK_ALIGNMENT> stack_;
  std::function<void()> task_;
};

class UContext : public ContextPolicy {
 public: 
  void init() override {
    getcontext(&context_);
    context_.uc_link = nullptr;
    context_.uc_stack.ss_size = STACK_SIZE;
    context_.uc_stack.ss_sp = &stack_;
    makecontext(&context_, reinterpret_cast<void(*)()>(task_wrapper), 1, this);
  }

  void swap(ContextPolicy& target) override {
    auto& target_context = static_cast<UContext&>(target);
    swapcontext(&context_, &target_context.context_);
  }

  ~UContext() override = default;
private:
  static void task_wrapper(UContext* context) { context->task_(); }
  ucontext_t context_;
};

class CACSContext : public ContextPolicy {
 public:
  void init() override {
    auto stack_start = reinterpret_cast<char *>(&stack_);
    auto stack_end = stack_start + STACK_SIZE;

    rsp_ = reinterpret_cast<void *>(stack_end);
    CACS_init(&rsp_, task_wrapper, this);

    assert(*reinterpret_cast<void**>(rsp_) == task_wrapper);
    assert(*reinterpret_cast<void**>(reinterpret_cast<char*>(rsp_) + 8) == this);
  }

  void swap(ContextPolicy& target) override {
    auto& target_context = static_cast<CACSContext&>(target);
    CACS(&rsp_, &target_context.rsp_);
  }

  ~CACSContext() override = default;
 private:
  static void task_wrapper(CACSContext *context) {
    void *real_rsp;
    
    asm volatile (
      R"(
        mov 40(%%rsp), %0 # 从栈中获取context 因为task_wrapper本身也会使用栈，context在rsp + 40处
        mov %%rsp, %1
      )"
      : "=r"(context), "=r"(real_rsp)  // 输出约束：context和临时rsp
      : 
      : "rax", "rsp", "memory"
    );
    assert(context != nullptr);

    auto ctx = static_cast<CACSContext *>(context);
    ctx->rsp_ = real_rsp;
    ctx->task_();
  }
  void* rsp_;
};

#endif // CONTEXT_POLICY_H_