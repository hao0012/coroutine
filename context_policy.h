#ifndef CONTEXT_POLICY_H_
#define CONTEXT_POLICY_H_

#include <cstddef>
#include <type_traits>
#include <ucontext.h>
#include <functional>

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
  static constexpr std::size_t STACK_SIZE = 1024 * 1024;

  std::aligned_storage_t<STACK_SIZE> stack_;  
  ucontext_t context_;
};

#endif // CONTEXT_POLICY_H_