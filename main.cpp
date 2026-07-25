#include "coroutine.h"
#include "awaitable.h"
#include <iostream>
#include <cassert>

using CoCP = CACSContext;

int add(int a, int b) {
  hco::await<CoCP>(hco::suspend_never_t{});
  std::cout << "add: " << a + b << " (after suspend_never)" << std::endl;
  return a + b;
}

int minus(int a, int b) {
  hco::await<CoCP>(hco::suspend_never_t{});
  std::cout << "minus: " << a - b << " (after suspend_never)" << std::endl;
  return a - b;
}

int main() {
  auto task1 = hco::Coroutine<CoCP>::create_task(add, 1, 2);
  auto task2 = hco::Coroutine<CoCP>::create_task(minus, 0, 1);
  auto futures = hco::Coroutine<CoCP>::start({task1, task2});
  for (auto& f : futures) f.get();
  std::cout << "done" << std::endl;
}
