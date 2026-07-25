#include "coroutine.h"
#include <iostream>
#include <cassert>

int add(int a, int b) {
  auto h = hco::this_coroutine<CACSContext>();
  assert(h);
  assert(!h.done());
  std::cout << "add: " << a + b << ", handle valid: " << static_cast<bool>(h) << std::endl;
  return a + b;
}

int minus(int a, int b) {
  auto h = hco::this_coroutine<CACSContext>();
  assert(h);
  std::cout << "minus: " << a - b << ", handle valid: " << static_cast<bool>(h) << std::endl;
  return a - b;
}

int main() {
  std::cout << "global_id: " << hco::Id::get_global_id() << std::endl;
  auto task1 = hco::Coroutine<CACSContext>::create_task(add, 1, 2);
  auto task2 = hco::Coroutine<CACSContext>::create_task(minus, 0, 1);
  std::cout << "task1 id: " << task1->get_id() << std::endl;
  std::cout << "task2 id: " << task2->get_id() << std::endl;

  auto h1 = task1->handle();
  auto h2 = task2->handle();
  assert(h1);
  assert(h2);
  assert(!h1.done());
  assert(h1 != h2);
  assert(h1 == h1);
  std::cout << "handle compare OK" << std::endl;

  auto future_list = hco::Coroutine<CACSContext>::start({task1, task2});
  for (auto& f : future_list) {
    f.get();
  }
  std::cout << "done" << std::endl;
}
