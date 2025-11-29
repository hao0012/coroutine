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
  auto c1 = hco::Coroutine::create(add, 1, 2);
  auto c2 = hco::Coroutine::create(minus, 0, 1);
  auto f = hco::Coroutine::start();
  
  f.get();
}