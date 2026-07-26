#include "generator.h"
#include <iostream>
#include <cassert>

int main() {
  hco::Generator<int> gen([](hco::Generator<int>* g) {
    for (int i = 0; i < 5; ++i) {
      g->yield_value(i * 10);
    }
  });

  int expected[] = {0, 10, 20, 30, 40};
  int idx = 0;
  for (int v : gen) {
    std::cout << v << " ";
    assert(v == expected[idx++]);
  }
  std::cout << "\ndone" << std::endl;
}
