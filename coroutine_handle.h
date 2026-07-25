#ifndef COROUTINE_HANDLE_H_
#define COROUTINE_HANDLE_H_

#include "context_policy.h"

namespace hco {

template<typename ContextPolicy> class Coroutine;

// 非拥有型协程句柄，提供和 C++20 std::coroutine_handle 类似的 API
template<typename ContextPolicy = UContext>
class coroutine_handle {
 public:
  coroutine_handle() : co_(nullptr) {}

  void resume() const { if (co_) co_->resume(); }
  bool done() const { return co_ == nullptr || co_->is_done(); }
  explicit operator bool() const { return co_ != nullptr; }

  void* address() const { return co_; }
  static coroutine_handle from_address(void* addr) {
    return coroutine_handle(static_cast<Coroutine<ContextPolicy>*>(addr));
  }

  bool operator==(const coroutine_handle& o) const { return co_ == o.co_; }
  bool operator!=(const coroutine_handle& o) const { return !(*this == o); }

  explicit coroutine_handle(Coroutine<ContextPolicy>* co) : co_(co) {}

 private:
  Coroutine<ContextPolicy>* co_;
};

}  // namespace hco

#endif  // COROUTINE_HANDLE_H_
