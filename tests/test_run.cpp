#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>

// Dummy function that does nothing
psac_function(test) {
  
}

// Test that the computation returns a non-empty SP tree
TEST(TestRun, TestNonEmpty) {
  auto comp = psac_run(test);
  ASSERT_NE(comp.root, nullptr);
}

bool called = false;
psac_function(test3) {
  called = true;
}

psac_function(test2) {
  psac_call(test3);
}

// Test that a computation can call a function
TEST(TestRun, TestFnCall) {
  auto comp = psac_run(test2);
  ASSERT_EQ(called, true);
}

