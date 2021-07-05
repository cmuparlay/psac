
#include<array>
#include<iostream>
#include<memory>
#include<random>

#include <psac/psac.hpp>
#include <psac/examples/mapreduce.hpp>

constexpr int n = 1000;
std::array<psac::Mod<int>, n> A;
std::array<psac::Mod<int>, n> B;

int main() {
  // Random initial values
  int truesum = 0;
  srand(time(0));
  for (int i = 0; i < n; i++) {
    A[i].write(rand() % 10);
    truesum += A[i].value;
  }
  
  // Check the result
  psac::Mod<int> result;
  auto computation = psac_run(map_reduce, std::begin(A), std::end(A), std::begin(B), [](int x) { return 2*x; }, &result);
  
  std::cout << "Values = ";
  for (int i = 0; i < n; i++) std::cout << A[i].value << " \n"[i == n-1];
  std::cout << "True sum = " << 2*truesum << std::endl;
  std::cout << "Result = " << result.value << std::endl;
  assert(result.value == 2*truesum);
  
  // Random changes
  for (int i = 0; i < n; i++) {
    if ((rand() % 2) == 0) {
      int cur = A[i].value;
      A[i].write(rand() % 10);
      truesum += A[i].value - cur;
    }
  }
  psac_propagate(computation);

  std::cout << "Values = ";
  for (int i = 0; i < n; i++) std::cout << A[i].value << " \n"[i == n-1];
  std::cout << "True sum = " << 2*truesum << std::endl;
  std::cout << "Result = " << result.value << std::endl;
  assert(result.value == 2*truesum);

  psac::GarbageCollector::run();
}

