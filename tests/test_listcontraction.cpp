#include <limits>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>

#include <psac/examples/listcontraction.hpp>

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

// Print the given sequence internals for debugging
void debug_out(DynamicSequence&) {
  /*
  size_t n = S.n;
  size_t nr = S.n_rounds;
  for (size_t i = 0; i < n; i++) {
    std::cerr << "A[" << i << "] = " << S.A[i].value << ", ";
  }
  std::cerr << std::endl;
  for (size_t r = 0; r < nr; r++) {
    std::cerr << "Round " << r << " -------------" << std::endl;
    std::cerr << std::endl;
    for (size_t i = 0; i < n; i++) {
      if (S.alive[r][i].value) {
        std::cerr << "(" << S.P[r][i].value << "," << S.A[i].value << ") ";
      }
      else {
        std::cerr << "   *    "; 
      }
    }
    std::cerr << std::endl;
  }
  */
}

// Return a dynamic sequence of length n consisting of the elements 0...n-1
DynamicSequence make_sequence(size_t n) {
  std::vector<int> a(n);
  std::iota(std::begin(a), std::end(a), 0);
  DynamicSequence S(a);
  S.go();
  return S;
}

// Check that all queries return correct values
bool is_correct(DynamicSequence& S, size_t n) {
  for (size_t i = 0; i < n; i++) {
    int r = i;
    int expected = 0;
    while (r != -1) {
      expected += S.A[r].value;
      if (S.query(i, r) != expected) {
        std::cerr << "Expected reduction from " << i << " to " << r << " to be "
          << expected << ", but got " << S.query(i, r) << " instead" << std::endl;
        debug_out(S);
        return false;
      }
      r = S.getR(0, r);
    }
  }
  return true; 
}

// ----------------------------------------------------------------------------
//                              Small tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestListSmall);
constexpr int small_list_size = 16;

// Test that the list contraction algorithm produces a computation
TEST_P(TestListSmall, TestConstruction) {
  auto S = make_sequence(small_list_size);
  ASSERT_NE(S.computation.root, nullptr);
}

// Test that the reduction of a sequence is correct
TEST_P(TestListSmall, TestReduction) {
  auto S = make_sequence(small_list_size);
  for (int i = 0; i < small_list_size; i++) {
    for (int j = i; j < small_list_size; j++) {
      int expected = j*(j+1)/2 - (i!=0)*i*(i-1)/2;
      ASSERT_EQ(S.query(i, j), expected);
    }
  }
}

// Test that a single value update correctly propagates
TEST_P(TestListSmall, TestSingleValueUpdate) {
  auto S = make_sequence(small_list_size);
  ASSERT_TRUE(is_correct(S, small_list_size));
  for (size_t i = 0; i < small_list_size; i++) {
    int newval = rand_int() % 1000;
    S.batch_update({{i, newval}});
    S.update();
    psac::GarbageCollector::run();
    //S.collect_garbage();
    ASSERT_TRUE(is_correct(S, small_list_size));
  }
}

// Test that a batch value update correctly propagates
TEST_P(TestListSmall, TestBatchValueUpdate) {
  auto S = make_sequence(10);
  ASSERT_TRUE(is_correct(S, 10));
  for (size_t subset = 1; subset < (1 << 10); subset++) {
    std::vector<std::pair<int,int>> U;
    for (size_t i = 0; i < 10; i++) {
      if (subset & i) {
        int newval = rand_int() % 1000;
        U.emplace_back(i, newval);
      }
    }
    S.batch_update(U);
    S.update();
    ASSERT_TRUE(is_correct(S, 10));
  }
  psac::GarbageCollector::run();
  //S.collect_garbage();
}

// Test that splits and joins work
TEST_P(TestListSmall, TestStructureUpdate) {
  auto S = make_sequence(small_list_size);
  ASSERT_TRUE(is_correct(S, small_list_size));
  int start = 0, end = small_list_size - 1;
  for (int i = 0; i < small_list_size - 1; i++) {
    int r = S.getR(0, i);
    S.batch_split({i});
    S.batch_join({{end, start}});
    S.update();
    psac::GarbageCollector::run();
    //S.collect_garbage();
    ASSERT_TRUE(is_correct(S, small_list_size));
    start = r;
    end = i;
  }
}

// ----------------------------------------------------------------------------
//                          Medium generated tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestListMedium);
constexpr int medium_list_size = 64;

// Test that the list contraction algorithm produces a computation
TEST_P(TestListMedium, TestConstruction) {
  auto S = make_sequence(medium_list_size);
  ASSERT_NE(S.computation.root, nullptr);
}

// Test that the reduction of a sequence is correct
TEST_P(TestListMedium, TestReduction) {
  auto S = make_sequence(medium_list_size);
  for (int i = 0; i < medium_list_size; i++) {
    for (int j = i; j < medium_list_size; j += rand_int() % (medium_list_size / 10)) {
      int expected = j*(j+1)/2 - (i!=0)*i*(i-1)/2;
      ASSERT_EQ(S.query(i, j), expected);
    }
  }
}

// ----------------------------------------------------------------------------
//                          Large generated tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestListLarge);
constexpr int large_list_size = 200;

// Test that the list contraction algorithm produces a computation
TEST_P(TestListLarge, TestConstruction) {
  auto S = make_sequence(large_list_size);
  ASSERT_NE(S.computation.root, nullptr);
}

// Test that the reduction of a sequence is correct
TEST_P(TestListLarge, TestReduction) {
  auto S = make_sequence(large_list_size);
  for (int i = 0; i < large_list_size; i++) {
    for (int j = i; j < large_list_size; j += rand_int() % (large_list_size / 10)) {
      int expected = j*(j+1)/2 - (i!=0)*i*(i-1)/2;
      ASSERT_EQ(S.query(i, j), expected);
    }
  }
}

// Test that a single value update correctly propagates
TEST_P(TestListLarge, TestSingleValueUpdate) {
  auto S = make_sequence(large_list_size);
  ASSERT_TRUE(is_correct(S, large_list_size));
  for (size_t i = 0; i < large_list_size; i++) {
    int newval = rand_int() % 1000;
    S.batch_update({{i, newval}});
    S.update();
    psac::GarbageCollector::run();
    //S.collect_garbage();
    ASSERT_TRUE(is_correct(S, large_list_size));
  }
}

// Test that splits and joins work
TEST_P(TestListLarge, TestStructureUpdate) {
  auto S = make_sequence(large_list_size);
  ASSERT_TRUE(is_correct(S, large_list_size));
  int start = 0, end = large_list_size - 1;
  for (int i = 0; i < large_list_size; i++) {
    int r =  S.getR(0, i);
    S.batch_split({i});
    S.batch_join({{end, start}});
    S.update();
    psac::GarbageCollector::run();
    //S.collect_garbage();
    ASSERT_TRUE(is_correct(S, large_list_size));
    start = r;
    end = i;
  }
}

