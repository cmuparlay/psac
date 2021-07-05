#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>

psac_function(test_prop, psac::Mod<int>* in, psac::Mod<int>* out) {
  psac_read((auto x), (in), {
    psac_write(out, x + 1);
  });
}

// Test that propagate correctly pushes a single change
TEST(TestPropagate, TestSingle) {
  psac::Mod<int> in, out;
  psac_write(&in, 5);
  auto comp = psac_run(test_prop, &in, &out);
  ASSERT_EQ(out.value, 6);
  psac_write(&in, 6);
  psac_propagate(comp);
  ASSERT_EQ(out.value, 7);
  psac::GarbageCollector::run();
}

psac_function(test_prop2, psac::Mod<int>* a, psac::Mod<int>* b, psac::Mod<int>* c,
    psac::Mod<int>* d) {

  psac_read((auto x), (a), {
    psac_write(b, x + 1);
  });

  psac_read((auto x), (b), {
    psac_write(c, x + 1);
  });

  psac_read((auto x), (c), {
    psac_write(d, x + 1);
  });

}

// Test that propgate correctly pushes a chain of
// dependent changes
TEST(TestPropagate, TestChain) {
  psac::Mod<int> a, b, c, d;
  psac_write(&a, 5);
  auto comp = psac_run(test_prop2, &a, &b, &c, &d);
  ASSERT_EQ(b.value, 6);
  ASSERT_EQ(c.value, 7);
  ASSERT_EQ(d.value, 8);
  psac_write(&a, 10);
  psac_propagate(comp);
  ASSERT_EQ(b.value, 11);
  ASSERT_EQ(c.value, 12);
  ASSERT_EQ(d.value, 13);
  psac::GarbageCollector::run();
}

psac_function(test_prop3, psac::Mod<int>* a, psac::Mod<int>* b, psac::Mod<int>* c) {
  psac_read((auto x), (a), {
    psac_write(b, x + 1);
  });
  psac_call(test_prop, b, c);
}

// Test that propgate correctly pushes a chain of
// dependent changes through multiple functions
TEST(TestPropagate, TestChainFunctions) {
  psac::Mod<int> a, b, c;
  psac_write(&a, 5);
  auto comp = psac_run(test_prop3, &a, &b, &c);
  ASSERT_EQ(b.value, 6);
  ASSERT_EQ(c.value, 7);
  psac_write(&a, 10);
  psac_propagate(comp);
  ASSERT_EQ(b.value, 11);
  ASSERT_EQ(c.value, 12);
  psac::GarbageCollector::run();
}

// Test that propagate correctly pushes a chain of
// dependent changes through multiple computations
TEST(TestPropagate, TestChainRuns) {
  psac::Mod<int> a, b, c;
  psac_write(&a, 5);
  auto comp = psac_run(test_prop, &a, &b);
  ASSERT_EQ(b.value, 6);
  auto comp2 = psac_run(test_prop, &b, &c);
  ASSERT_EQ(c.value, 7);
  psac_write(&a, 10);
  psac_propagate(comp);
  ASSERT_EQ(b.value, 11);
  psac_propagate(comp2);
  ASSERT_EQ(c.value, 12);
  psac::GarbageCollector::run();
}

psac_function(test_multiple_readers, psac::Mod<int>* in, psac::Mod<int>* out1, psac::Mod<int>* out2) {
  psac_read((auto x), (in), {
    psac_write(out1, x);
  });
  psac_read((auto x), (in), {
    psac_write(out2, x);
  });
}

// Test that multiple functions can read the same value
TEST(TestPropagate, TestMultipleReaders) {
  psac::Mod<int> in, out1, out2;
  psac_write(&in, 1);
  auto comp = psac_run(test_multiple_readers, &in, &out1, &out2);
  ASSERT_EQ(out1.value, 1);
  ASSERT_EQ(out2.value, 1);
  psac_write(&in, 2);
  psac_propagate(comp);
  ASSERT_EQ(out1.value, 2);
  ASSERT_EQ(out2.value, 2);
  psac::GarbageCollector::run();
}

psac_function(test_select, psac::Mod<int>* i, psac::Mod<int>* a, psac::Mod<int>* b, psac::Mod<int>* res) {
  psac_read((auto j), (i), {
    if (j == 1) {
      psac_read((int x), (a), {
        psac_write(res, x);
      });
    }
    else {
      psac_read((int x), (b), {
        psac_write(res, x);
      });
    }
  });
}

// Test that propgation works when the computation
// results in a different structure
TEST(TestPropagate, TestStructureChange) {
  psac::Mod<int> i, a, b, res;
  i.write(1);
  a.write(10);
  b.write(20);
  auto comp = psac_run(test_select, &i, &a, &b, &res);
  ASSERT_EQ(res.value, 10);
  i.write(2);
  psac_propagate(comp);
  ASSERT_EQ(res.value, 20);
  psac::GarbageCollector::run();
}


