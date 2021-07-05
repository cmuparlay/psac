
#include <array>

#include "gtest/gtest.h"

#include "common.hpp"

#include <psac/psac.hpp>

// Test that write writes the correct value
TEST(TestMod, TestWrite) {
  psac::Mod<int> m;
  psac_write(&m, 5);
  ASSERT_EQ(m.value, 5);
}

// Read the given mod and assert that its value is 
// equal to the given expected value
psac_function(test_read, psac::Mod<int>* m, int expected) {
  psac_read((auto x), (m), {
    ASSERT_EQ(x, expected);
  });
}

// Test that read reads the correct value
TEST(TestMod, TestRead) {
  psac::Mod<int> m;
  psac_write(&m, 5);
  psac_run(test_read, &m, 5);
}

// Test that reading a mod registers a reader
TEST(TestMod, TestReader) {
  psac::Mod<int> m;
  psac_write(&m, 5);
  auto comp = psac_run(test_read, &m, 5);
  ASSERT_NE(m.readers.ptr_value, 0);
}

psac_function(test_read_tuple, psac::Mod<int>* a, int expected_a, psac::Mod<int>* b, int expected_b) {
  psac_read((auto x, auto y), (a, b), {
    ASSERT_EQ(x, expected_a);
    ASSERT_EQ(y, expected_b);
  });
}

// Test that reading multiple mods reads the correct
// values
TEST(TestMod, TestTupleRead) {
  psac::Mod<int> a, b;
  psac_write(&a, 1);
  psac_write(&b, 2);
  auto comp = psac_run(test_read_tuple, &a, 1, &b, 2);
}

template<typename It>
psac_function(test_read_array, It begin, It end) {
  psac_read_array(auto a, std::make_pair(begin, end), {
    for (size_t i = 0; i < a.size(); i++) {
      ASSERT_EQ(a[i], i);
    }
  });
}

// Test that reading an array works correctly
TEST(TestMod, TestArrayRead) {
  std::array<psac::Mod<int>, 10> ms;
  for (size_t i = 0; i < 10; i++) {
    psac_write(&ms[i], i);
  }
  auto comp = psac_run(test_read_array, std::begin(ms), std::end(ms));
}

psac_function(test_dynamic_read, psac::Mod<int>* ms) {
  psac_dynamic_context({
    for (int i = 0; i < 10; i++) {
      auto val = psac_dynamic_read(ms + i);
      ASSERT_EQ(val, i);
    }
  });
}

// Test that dynamic read contexts work correctly
TEST(TestMod, TestDynamicRead) {
  std::array<psac::Mod<int>, 10> ms;
  for (size_t i = 0; i < 10; i++) {
    psac_write(&ms[i], i);
  }
  auto comp = psac_run(test_dynamic_read, ms.data());
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
TEST(TestMod, TestMultipleReaders) {
  psac::Mod<int> in, out1, out2;
  psac_write(&in, 1);
  auto comp = psac_run(test_multiple_readers, &in, &out1, &out2);
  ASSERT_EQ(out1.value, 1);
  ASSERT_EQ(out2.value, 1);
}

psac_function(test_dynamic_alloc, psac::Mod<bool>* success) {
  psac::Mod<int>* m = psac_alloc(int);
  psac_write(m, 5);
  psac_read((auto x), (m), {
    if (x == 5) {
      psac_write(success, true);
    }
    else {
      psac_write(success, false);
    }
  });
}

// Test that dynamic allocation works
TEST(TestMod, TestDynamicAlloc) {
  psac::Mod<bool> success;
  auto comp = psac_run(test_dynamic_alloc, &success);
  ASSERT_TRUE(success.value);
}

psac_function(test_dynamic_alloc_nontrivial, psac::Mod<bool>* success) {
  psac::Mod<std::string>* m = psac_alloc(std::string);
  psac_write(m, std::string("Hello, friends"));
  psac_read((const auto& x), (m), {
    if (x == std::string("Hello, friends")) {
      psac_write(success, true);
    }
    else {
      psac_write(success, false);
    }
  });
}

// Test that dynamic allocation of a big nontrivial type works (small types are SBO'd)
TEST(TestMod, TestDynamicAllocNontrivial) {
  psac::Mod<bool> success;
  auto comp = psac_run(test_dynamic_alloc_nontrivial, &success);
  ASSERT_TRUE(success.value);
}

psac_function(test_dynamic_alloc_nontrivial_empty, psac::Mod<bool>* success) {
  psac::Mod<std::string>* m = psac_alloc(std::string);
  psac_write(m, std::string());
  psac_read((const auto& x), (m), {
    if (x == std::string()) {
      psac_write(success, true);
    }
    else {
      psac_write(success, false);
    }
  });
}

// Test that dynamic allocation of a big empty nontrivial type works (small types are SBO'd)
TEST(TestMod, TestDynamicAllocNontrivialEmpty) {
  psac::Mod<bool> success;
  auto comp = psac_run(test_dynamic_alloc_nontrivial_empty, &success);
  ASSERT_TRUE(success.value);
}

psac_function(test_mod_array, psac::Mod<int>* a, size_t s) {
  for (size_t i = 0; i < s; i++) {
    psac_write(a + i, i);
  }
}

// Test that arrays of mods work correctly
TEST(TestMod, TestModArray) {
  psac::ModArray<int> A(10);
  auto comp = psac_run(test_mod_array, A.get_array(), 10);
  for (size_t i = 0; i < 10; i++) {
    ASSERT_EQ(A[i].value, i);
  }
}

psac_function(test_dynamic_alloc_array, psac::Mod<bool>* success) {
  psac::Mod<int>* ms = psac_alloc_array(int, 10);
  for (size_t i = 0; i < 10; i++) {
    psac_write(ms + i, i);
  }
  psac_read_array(auto a, std::make_pair(ms, ms + 10), {
    bool good = true;
    for (int i = 0; i < 10; i++) {
      if (a[i] != i) good = false;
    }
    psac_write(success, good);
  });
}

TEST(TestMod, TestDynamicAllocArray) {
  psac::Mod<bool> success;
  auto comp = psac_run(test_dynamic_alloc_array, &success);
  ASSERT_TRUE(success.value);
}

psac_function(test_alloc_inside_read, psac::Mod<int>* m) {
  psac_read((auto x), (m), {
    
    auto m2 = psac_alloc(int);
    psac_write(m2, x + 1); 
    
  });
}

TEST(TestMod, TestAllocInsideRead) {
  psac::Mod<int> m;
  m.write(5);
  psac_run(test_alloc_inside_read, &m);
}

