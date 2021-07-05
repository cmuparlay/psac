#pragma once

#include <iostream>
#include <cassert>
#include <ctype.h>
#include <memory>
#include <stdlib.h>
#include <type_traits>
#include <math.h>
#include <atomic>
#include <cstring>

#include "parallel.h"

typedef unsigned __int128 uint128_t;

namespace parlay {
  template <class T>
  size_t log2_up(T);
}


namespace parlay {



  template<typename T>
  inline void assign_uninitialized(T& a, const T& b) {
    new (static_cast<void*>(std::addressof(a))) T(b);
  }

  template<typename T>
  inline void assign_uninitialized(T& a, T&& b) { 
    new (static_cast<void*>(std::addressof(a))) T(std::move(b));
  }

  template<typename T>
  inline void move_uninitialized(T& a, const T b) {
    new (static_cast<void*>(std::addressof(a))) T(std::move(b));
  }
  
  // a 32-bit hash function
  // Ignore sanitizer warnings about overflow
  __attribute__((no_sanitize("integer")))
  inline uint32_t hash32(uint32_t a) {
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
  }

  inline uint32_t hash32_2(uint32_t a) {
    uint32_t z = (a + 0x6D2B79F5UL);
    z = (z ^ (z >> 15)) * (z | 1UL);
    z ^= z + (z ^ (z >> 7)) * (z | 61UL);
    return z ^ (z >> 14);
  }

  inline uint32_t hash32_3(uint32_t a) {
      uint32_t z = a + 0x9e3779b9;
      z ^= z >> 15; // 16 for murmur3
      z *= 0x85ebca6b;
      z ^= z >> 13;
      z *= 0xc2b2ae3d; // 0xc2b2ae35 for murmur3
      return z ^= z >> 16;
  }


  // from numerical recipes
  inline uint64_t hash64(uint64_t u )
  {
    uint64_t v = u * 3935559000370003845ul + 2691343689449507681ul;
    v ^= v >> 21;
    v ^= v << 37;
    v ^= v >>  4;
    v *= 4768777513237032717ul;
    v ^= v << 20;
    v ^= v >> 41;
    v ^= v <<  5;
    return v;
  }

  // a slightly cheaper, but possibly not as good version
  // based on splitmix64
  inline uint64_t hash64_2(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
  }

  template <typename ET>
  inline bool atomic_compare_and_swap(ET* a, ET oldval, ET newval) {
#if defined(MCX16)
    static_assert(sizeof(ET) <= 16, "Bad CAS length");
#else
    static_assert(sizeof(ET) <= 8, "Bad CAS length");
#endif
    if (sizeof(ET) == 1) {
      uint8_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint8_t*>(a), r_oval, r_nval);
    } else if (sizeof(ET) == 4) {
      uint32_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint32_t*>(a), r_oval, r_nval);
    } else if (sizeof(ET) == 8) {
      uint64_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint64_t*>(a), r_oval, r_nval);
#if defined(MCX16)
    } else if (sizeof(ET) == 16) {
      uint128_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint128_t*>(a), r_oval, r_nval);
#endif
    }
    else {
      assert(false && "Bad CAS length at runtime");
      return false;
    }
  }

  // returns the log base 2 rounded up (works on ints or longs or unsigned versions)
  template <class T>
  size_t log2_up(T i) {
    size_t a=0;
    T b=i-1;
    while (b > 0) {b = b >> 1; a++;}
    return a;
  }

  // A cheap version of an inteface that should be improved
  // Allows forking a state into multiple states
  struct random {
   public:
    random(size_t seed) : state(seed) {};
    random() : state(0) {};
    random fork(uint64_t i) const {
      return random(hash64(hash64(i+state))); }
    random next() const { return fork(0);}
    size_t ith_rand(uint64_t i) const {
      return hash64_2(i+state);}
    size_t operator[] (size_t i) const {return ith_rand(i);}
    size_t rand() { return ith_rand(0);}
   private:
    uint64_t state = 0;
  };

}
