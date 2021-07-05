#include <limits>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>

#include <psac/examples/rabin-karp.hpp>

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

// Return a random string of length l
std::string random_string(size_t l) {
  std::string s;
  for (size_t j = 0; j < l; j++) {
    s.push_back('a' + (rand_int() % 26));
  }
  return s;
}

// ----------------------------------------------------------------------------
//                                  Tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestRabinKarp);

TEST_P(TestRabinKarp, TestRabinKarp) {
  size_t n = 1000;
  std::vector<psac::Mod<std::string>> S(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&S[i], random_string(64));
  }
  psac::Mod<hash_pair> result;
  auto comp = psac_run(rabin_karp, std::begin(S), std::end(S), &result);
  ASSERT_NE(comp.root, nullptr);

  hash_t hash = 0;
  for (size_t j = 0; j < n; j++) {
    const std::string& s = S[j].value;
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      hash = hash * b + static_cast<unsigned long long int>(c);
    }
  }
  ASSERT_EQ(hash, result.value.first);
}


