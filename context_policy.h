#ifndef CONTEXT_POLICY_H_
#define CONTEXT_POLICY_H_

#include <cstddef>
#include <ucontext.h>
#include <cassert>

#include "cacs.h"

class ContextPolicy {
 public:
  virtual void init(void(*entry)(void*), void* arg) = 0;
  virtual void swap(ContextPolicy& target) = 0;
  virtual ~ContextPolicy() = default;
 protected:
  static constexpr std::size_t STACK_SIZE = 1024 * 1024;
  static constexpr std::size_t STACK_ALIGNMENT = 16;
  std::aligned_storage_t<STACK_SIZE, STACK_ALIGNMENT> stack_;
};

class UContext : public ContextPolicy {
 public: 
  void init(void(*entry)(void*), void* arg) override {
    getcontext(&context_);
    context_.uc_link = nullptr;
    context_.uc_stack.ss_size = STACK_SIZE;
    context_.uc_stack.ss_sp = &stack_;
    makecontext(&context_, reinterpret_cast<void(*)()>(entry), 1, arg);
  }

  void swap(ContextPolicy& target) override {
    auto& target_context = static_cast<UContext&>(target);
    swapcontext(&context_, &target_context.context_);
  }

  ~UContext() override = default;
 private:
  ucontext_t context_;
};

class CACSContext : public ContextPolicy {
 public:
  void init(void(*entry)(void*), void* arg) override {
    auto stack_start = reinterpret_cast<char*>(&stack_);
    auto stack_end = stack_start + STACK_SIZE;

    rsp_ = reinterpret_cast<void*>(stack_end);
    CACS_init(&rsp_, entry, arg);

    assert(*reinterpret_cast<void**>(rsp_) == reinterpret_cast<void*>(entry));
    assert(*reinterpret_cast<void**>(reinterpret_cast<char*>(rsp_) + 8) == arg);
  }

  void swap(ContextPolicy& target) override {
    auto& target_context = static_cast<CACSContext&>(target);
    CACS(&rsp_, &target_context.rsp_);
  }

  ~CACSContext() override = default;
 private:
  void* rsp_;
};

#endif // CONTEXT_POLICY_H_
