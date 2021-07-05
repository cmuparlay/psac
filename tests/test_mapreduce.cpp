#include <limits>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>

#include <psac/examples/mapreduce.hpp>

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());

// Makes k random changes to the given input of size n and reruns the given computation
void random_update(psac::Computation& comp, size_t n, size_t k, std::vector<psac::Mod<int>>& a) {
  for (size_t j = 0; j < k; j++) {
    size_t i = dis(gen) % n;
    int v = dis(gen) % 1000;    // Keep numbers small to not overflow sums
    psac_write(&a[i], v);
  }
  psac_propagate(comp);
}

// ----------------------------------------------------------------------------
//                            Map tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestMap);

// Run a map computation on the given input of size n and returns the computation object
psac::Computation do_map(size_t n, std::vector<psac::Mod<int>>& a, std::vector<psac::Mod<int>>& b) {
  for (size_t i = 0; i < n; i++) {
    psac_write(&a[i], i);
  }
  auto comp = psac_run(map, std::begin(a), std::end(a), std::begin(b), [](int x) { return 2 * x; });
  return comp;
}

bool valid_map(size_t n, std::vector<psac::Mod<int>>& a, std::vector<psac::Mod<int>>& b) {
  for (size_t i = 0; i < n; i++) {
    if (b[i].value != 2 * a[i].value) return false;
  }
  return true;
}

TEST_P(TestMap, TestMapComputeSmall) {
  size_t n = 100;
  std::vector<psac::Mod<int>> a(n), b(n);
  auto comp = do_map(n, a, b);
  ASSERT_TRUE(valid_map(n, a, b));
}

TEST_P(TestMap, TestMapUpdateSmall) {
  size_t n = 100;
  std::vector<psac::Mod<int>> a(n), b(n);
  auto comp = do_map(n, a, b);
  ASSERT_TRUE(valid_map(n, a, b));
  for (size_t k = 1; k <= n; k *= n) {
    random_update(comp, n, k, a);
    ASSERT_TRUE(valid_map(n, a, b));
  }
  psac::GarbageCollector::run();
}

TEST_P(TestMap, TestMapComputeLarge) {
  size_t n = 100000;
  std::vector<psac::Mod<int>> a(n), b(n);
  auto comp = do_map(n, a, b);
  ASSERT_TRUE(valid_map(n, a, b));
}

TEST_P(TestMap, TestMapUpdateLarge) {
  size_t n = 100000;
  std::vector<psac::Mod<int>> a(n), b(n);
  auto comp = do_map(n, a, b);
  ASSERT_TRUE(valid_map(n, a, b));
  for (size_t k = 1; k <= n; k *= 10) {
    random_update(comp, n, k, a);
    ASSERT_TRUE(valid_map(n, a, b));
  }
  psac::GarbageCollector::run();
}

// ----------------------------------------------------------------------------
//                            Sum (reduce) tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestSum);

psac::Computation do_sum(size_t n, std::vector<psac::Mod<int>>& a, psac::Mod<int>& res) {
  for (size_t i = 0; i < n; i++) {
    psac_write(&a[i], i % 1000);
  }
  auto comp = psac_run(sum, std::begin(a), std::end(a), &res);
  return comp;
}


bool valid_sum(size_t n, std::vector<psac::Mod<int>>& a, psac::Mod<int>& res) {
  int total = 0;
  for (size_t i = 0; i < n; i++) {
    total += a[i].value;
  }
  if (total != res.value) {
    std::cerr << "Expected sum = " << total << ", but found result = " << res.value << " instead" << std::endl;
    for (size_t i = 0; i < n; i++) {
      std::cerr << a[i].value << " ";
    }
    std::cerr << std::endl;
  }
  return (total == res.value);
}

TEST_P(TestSum, TestSumComputeSmall) {
  size_t n = 100;
  std::vector<psac::Mod<int>> a(n);
  psac::Mod<int> res;
  auto comp = do_sum(n, a, res);
  ASSERT_TRUE(valid_sum(n, a, res));
}

TEST_P(TestSum, TestSumUpdateSmall) {
  size_t n = 100;
  std::vector<psac::Mod<int>> a(n);
  psac::Mod<int> res;
  auto comp = do_sum(n, a, res);
  ASSERT_TRUE(valid_sum(n, a, res));
  for (size_t k = 1; k <= n; k *= 10) {
    random_update(comp, n, k, a);
    ASSERT_TRUE(valid_sum(n, a, res));
  }
  psac::GarbageCollector::run();
}

TEST_P(TestSum, TestSumComputeLarge) {
  size_t n = 100000;
  std::vector<psac::Mod<int>> a(n);
  psac::Mod<int> res;
  auto comp = do_sum(n, a, res);
  ASSERT_TRUE(valid_sum(n, a, res));
}

TEST_P(TestSum, TestSumUpdateLarge) {
  size_t n = 100000;
  std::vector<psac::Mod<int>> a(n);
  psac::Mod<int> res;
  auto comp = do_sum(n, a, res);
  ASSERT_TRUE(valid_sum(n, a, res));
  for (size_t k = 1; k <= n; k *= 10) {
    random_update(comp, n, k, a);
    ASSERT_TRUE(valid_sum(n, a, res));
  }
  psac::GarbageCollector::run();
}

