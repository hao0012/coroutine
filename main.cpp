#include "coroutine.h"
#include "awaitable.h"
#include "task.h"
#include <iostream>
#include <cassert>

using CoCP = CACSContext;

int main() {
  hco::coroutine_handle<CoCP> task1_handle;
  hco::coroutine_handle<CoCP> task2_handle;

  auto task1 = hco::Task<int, CoCP>::from([&task2_handle]() -> int {
    std::cout << "task1: transfer to task2" << std::endl;
    hco::await<CoCP>(hco::yield_to_t<CoCP>{task2_handle});
    std::cout << "task1: resumed" << std::endl;
    return 1;
  });

  auto task2 = hco::Task<int, CoCP>::from([]() -> int {
    std::cout << "task2: running" << std::endl;
    return 2;
  });

  // task3: 先启动 task1 → task2 链，等链跑完后 resume task1
  auto task3 = hco::Task<int, CoCP>::from([&task1_handle]() -> int {
    std::cout << "task3: starting chain" << std::endl;
    task1_handle.resume();
    std::cout << "task3: chain done, resuming task1" << std::endl;
    task1_handle.resume();
    return 3;
  });

  task1_handle = task1.handle();
  task2_handle = task2.handle();

  hco::Coroutine<CoCP>::start({task1.coroutine(), task2.coroutine(), task3.coroutine()});

  assert(task1.get() == 1);
  assert(task2.get() == 2);
  assert(task3.get() == 3);
  std::cout << "done" << std::endl;
}
