
#ifndef PSAC_EXAMPLES_RABIN_KARP_HPP_
#define PSAC_EXAMPLES_RABIN_KARP_HPP_

#include <string>

#include <psac/psac.hpp>

// Convert an iterator to a pointer to the value that the iterator points to
template<typename T>
auto to_ptr(T x) {
  return &(*x);
}

// ----------------------------------------------------------------------------
//                            HASHING OPERATIONS
// ----------------------------------------------------------------------------

constexpr unsigned long long int MOD = 100055128505716009ULL;
constexpr unsigned long long int b = 26;

struct hash_t {

  hash_t() : x(0) { }
  hash_t(unsigned long long int _x) : x(_x % MOD) { }

  hash_t operator+(hash_t other) const {
    return hash_t(x + other.x);
  }

// UBSAN doesn't like int128 so we have to turn it off here
#if defined(__clang__)
  __attribute__((no_sanitize("undefined")))
#endif
  hash_t operator*(hash_t other) const {
    __int128_t tmp = (static_cast<__int128_t>(x) * other.x) % MOD;
    return hash_t(static_cast<unsigned long long int>(tmp));
  }

  bool operator==(const hash_t& other) const { return x == other.x; }
  bool operator!=(const hash_t& other) const { return x != other.x; }

  unsigned long long int x;
};

using hash_pair = std::pair<hash_t, hash_t>;

// Given a string S, returns a pair consisting of the hash
// of S and the value of b^n, where n is the length of S
hash_pair hash_chunk(const std::string& chunk) {
  hash_t pre = 1;
  hash_t res = 0;
  for (size_t i = 0; i < chunk.length(); i++) {
    res = res * b + static_cast<unsigned long long int>(chunk[i]);
    pre = pre * b;
  }
  return std::make_pair(res, pre);
}

// Merge two adjacent hash values (computes the hash of the
// concatenation of the strings that they represent)
hash_pair merge(const hash_pair& left, const hash_pair& right) {
  return std::make_pair(left.first * right.second + right.first, left.second * right.second);
}

// ----------------------------------------------------------------------------
//                                  ALGORITHM
// ----------------------------------------------------------------------------

template<typename It>
psac_function(rabin_karp, It in_begin, It in_end, psac::Mod<hash_pair>* result) {
  if (in_begin + 1 == in_end) {
    psac_read((const std::string& s), (to_ptr(in_begin)), {
      psac_write(result, hash_chunk(s));
    });
  }
  else {
    auto in_mid = in_begin + std::distance(in_begin, in_end) / 2;
    psac::Mod<hash_pair>* left_result = psac_alloc(hash_pair);
    psac::Mod<hash_pair>* right_result = psac_alloc(hash_pair);
    psac_par(
      psac_call(rabin_karp, in_begin, in_mid, left_result),
      psac_call(rabin_karp, in_mid, in_end, right_result)
    );
    psac_read((auto left, auto right), (left_result, right_result), {
      psac_write(result, merge(left, right));
    });
  }
}

#endif  // PSAC_EXAMPLES_RABIN_KARP_HPP_

