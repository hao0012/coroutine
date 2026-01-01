#include "coroutine.h"
#include <cassert>
#include <iostream>

int add(int a, int b) {
  std::cout << "add: " << a + b << std::endl;
  return a + b;
}

int minus(int a, int b) {
  std::cout << "minus: " << a - b << std::endl;
  return a - b;
}

int main() {
  std::cout << hco::Id::get_global_id() << std::endl;
  auto task1 = hco::Coroutine<CACSContext>::create_task(add, 1, 2);
  auto task2 = hco::Coroutine<CACSContext>::create_task(minus, 0, 1);
  auto future_list = hco::Coroutine<CACSContext>::start({task1, task2});
  for (auto& future: future_list) {
    future.get();
  }
}