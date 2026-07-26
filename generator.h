#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <functional>
#include <optional>
#include "context_policy.h"

namespace hco {

// Generator<T> — 惰性序列，支持 range-for 遍历。
// 用法:
//   Generator<int> gen([](Generator<int>* g) {
//       for (int i = 0; i < 10; ++i) g->yield_value(i);
//   });
//   for (int v : gen) { ... }
template<typename T>
class Generator {
  struct State {
    std::function<void(Generator*)> func;
    std::optional<T> value;
    bool done = false;
    Generator* gen = nullptr;
  };

  CACSContext caller_ctx_;
  CACSContext gen_ctx_;
  State state_;

  static void gen_entry(void* raw) {
    auto* state = static_cast<State*>(raw);
    state->func(state->gen);
    state->done = true;
    state->gen->gen_ctx_.swap(state->gen->caller_ctx_);
  }

 public:
  Generator() = default;

  template<typename F>
  Generator(F&& f) {
    state_.gen = this;
    state_.func = [f = std::move(f)](Generator* g) { f(g); };
    gen_ctx_.init(gen_entry, &state_);
  }

  void yield_value(T val) {
    state_.value = std::move(val);
    gen_ctx_.swap(caller_ctx_);
  }

  class iterator {
   public:
    explicit iterator(Generator* g) : gen_(g) {}
    Generator* gen_;
   public:
    T operator*() const { return *gen_->state_.value; }
    iterator& operator++() {
      gen_->caller_ctx_.swap(gen_->gen_ctx_);
      return *this;
    }
    bool operator!=(std::default_sentinel_t) const {
      return !gen_->state_.done;
    }
  };

  auto begin() {
    if (!state_.done) {
      caller_ctx_.swap(gen_ctx_);
    }
    return iterator{this};
  }

  std::default_sentinel_t end() { return {}; }
};

}  // namespace hco

#endif  // GENERATOR_H_
