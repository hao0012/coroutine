#include "coroutine.h"
#include "awaitable.h"
#include "task.h"
#include <iostream>
#include <cassert>

using CoCP = CACSContext;

int add(int a, int b) {
  hco::await<CoCP>(hco::suspend_never_t{});
  std::cout << "add: " << a + b << std::endl;
  return a + b;
}

int minus(int a, int b) {
  hco::await<CoCP>(hco::suspend_never_t{});
  std::cout << "minus: " << a - b << std::endl;
  return a - b;
}

int main() {
  auto task1 = hco::Task<int, CoCP>::from([]() { return add(1, 2); });
  auto task2 = hco::Task<int, CoCP>::from([]() { return minus(0, 1); });

  hco::Coroutine<CoCP>::start({task1.coroutine(), task2.coroutine()});

  assert(task1.get() == 3);
  assert(task2.get() == -1);
  std::cout << "done" << std::endl;
}
