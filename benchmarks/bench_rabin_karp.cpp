
#include <algorithm>
#include <numeric>
#include <random>
#include <set>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/rabin-karp.hpp>

const auto max_n = 100000000;
const auto seed = 196883;
#include "common.hpp"

const auto test_chunk_size = 64;

// Return a random string of length l
std::string random_string(size_t l, size_t seed) {
  std::string s;
  parlay::random rng(seed);
  for (size_t j = 0; j < l; j++) {
    s.push_back('a' + rng.ith_rand(j) % 26);
  }
  return s;
}

// Return n random strings of length l
std::vector<std::string> random_strings(size_t n, size_t l) {
  std::vector<std::string> S(n);
  parlay::parallel_for(0, n, [&](auto i) {
    S[i] = random_string(l, i);
  });
  return S;
}

std::vector<std::string> random_chunks(size_t n, size_t chunk_size) {
  std::vector<std::string> S = random_strings(n / chunk_size, chunk_size);
  if (n % chunk_size != 0) {
    S.push_back(random_string(n % chunk_size, n / chunk_size));
  }
  return S;
}

// Pre-generate chunks and corresponding mods
std::vector<std::string> S = random_chunks(max_n, test_chunk_size);
std::vector<psac::Mod<std::string>> Sm = []() {
  std::vector<psac::Mod<std::string>> modz(S.size());
  parlay::parallel_for(0, S.size(), [&](auto i) {
    psac_write(&modz[i], S[i]);
  });
  return modz;
}();
std::vector<int> perm = []() {
  std::vector<int> p(max_n);
  std::iota(std::begin(p), std::end(p), 0);
  std::shuffle(std::begin(p), std::end(p), std::default_random_engine(seed));
  return p;
}();


template<typename It>
hash_pair hash_seq(It begin, It end) {
  if (begin + 1 == end) {
    return hash_chunk(*begin);
  }
  else {
    auto mid = begin + std::distance(begin, end) / 2;
    hash_pair left_ans = hash_seq(begin, mid);
    hash_pair right_ans = hash_seq(mid, end);
    return merge(left_ans, right_ans);
  }
}

static void rabin_karp_compute_seq(benchmark::State& state) {
  size_t n = state.range(0);

  hash_t ans; 
  for (auto _ : state) {
    hash_t hash = hash_seq(std::begin(S), std::end(S)).first;
    benchmark::DoNotOptimize(ans = hash);
  }
}

template<typename It>
hash_pair hash_par(It begin, It end) {
  if (begin + 1 == end) {
    return hash_chunk(*begin);
  }
  else {
    auto mid = begin + std::distance(begin, end) / 2;
    hash_pair left_ans, right_ans;
    parlay::par_do(
      [&]() { left_ans = hash_par(begin, mid); },
      [&]() { right_ans = hash_par(mid, end); }
    );
    return merge(left_ans, right_ans);
  }
}

static void rabin_karp_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  size_t n = state.range(1);

  hash_t ans; 
  for (auto _ : state) {
    auto hash = hash_par(std::begin(S), std::end(S)).first;
    benchmark::DoNotOptimize(ans = hash);
  }
}

PSAC_STATIC_BENCHMARK(rabin_karp_compute)(benchmark::State& state) {
  size_t n = state.range(1);
  size_t n_chunks = n / test_chunk_size + (n % test_chunk_size != 0);
  
  psac::Mod<hash_pair> res;
  for (auto _ : state) {
    comp = psac_run(rabin_karp, std::begin(Sm), std::end(Sm), &res);
    record_stats(state);
  }
}

PSAC_DYNAMIC_BENCHMARK(rabin_karp_update)(benchmark::State& state) {
  size_t n = state.range(1);
  size_t k = state.range(2);
  size_t n_chunks = n / test_chunk_size + (n % test_chunk_size != 0);

  psac::Mod<hash_pair> res;
  comp = psac_run(rabin_karp, std::begin(Sm), std::end(Sm), &res);

  size_t rand_seed = 0;
  for (auto _ : state) {
    // Make random changes
    state.PauseTiming();
    std::vector<int> changes;

    // Pick k elements to change
    parlay::random rng(rand_seed++);
    for (int i = 0; i < k; i++) {
      int j = i + (rng.ith_rand(i) % (n - i));
      std::swap(perm[i], perm[j]);
    }

    std::vector<int> to_mark;
    for (size_t i = 0; i < k; i++) {
      int j = perm[i];
      int chunk_id = j / test_chunk_size;
      int chunk_pos = j % test_chunk_size;
      Sm[chunk_id].value[chunk_pos] = 'a' + rng.ith_rand(k + i) % 26;
      to_mark.push_back(chunk_id);
    }
    state.ResumeTiming();

    // Write all updates
    parlay::parallel_for(0, to_mark.size(), [&](auto i) {
      int j = to_mark[i];
      Sm[j].notify_readers();
    });

    // Propagate changes
    psac_propagate(comp);
    record_stats(state);
  }

  finalize(state);
}

REGISTER_STATIC_BENCHMARK(rabin_karp_compute);
REGISTER_DYNAMIC_BENCHMARK(rabin_karp_update);

