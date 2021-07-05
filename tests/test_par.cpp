#include <array>

#include "gtest/gtest.h"

#include "common.hpp"

#include <psac/psac.hpp>

psac_function(test_par_do, psac::Mod<int>* a, psac::Mod<int>* b) {
  psac_par(
    { psac_write(a, 1); },
    { psac_write(b, 2); }
  );
}

// Test that both branches of a par call are executed
TEST(TestPar, TestParDo) {
  psac::Mod<int> a, b;

  auto comp = psac_run(test_par_do, &a, &b);

  ASSERT_EQ(a.value, 1);
  ASSERT_EQ(b.value, 2);
}

psac_function(test_nested_par_do, psac::Mod<int>* a, psac::Mod<int>* b, psac::Mod<int>* c, psac::Mod<int>* d) {
  psac_par(
    {
      psac_par(
        {
          psac_write(a, 1);
        },
        {
          psac_write(b, 2);
        }
      );
    },
    {
      psac_par(
        {
          psac_write(c, 3);
        },
        {
          psac_write(d, 4);
        }
      );
    }
  );
}

// Test that nested pars work
TEST(TestPar, TestNestedParDo) {
  psac::Mod<int> a, b, c, d;
  auto comp = psac_run(test_nested_par_do, &a, &b, &c, &d);
  ASSERT_EQ(a.value, 1);
  ASSERT_EQ(b.value, 2);
  ASSERT_EQ(c.value, 3);
  ASSERT_EQ(d.value, 4);
}

template<typename It>
psac_function(test_par_for, It begin, It end) {
  int n = std::distance(begin, end);
  psac_parallel_for(int i, 0, n, 1, {
    psac_write(&(*(begin + i)), i);
  });
}

TEST(TestPar, TestParFor) {
  std::array<psac::Mod<int>, 10> ms;
  auto comp = psac_run(test_par_for, std::begin(ms), std::end(ms));
  for (size_t i = 0; i < 10; i++) {
    ASSERT_EQ(ms[i].value, i);
  }
}

