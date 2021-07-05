
#include <algorithm>
#include <numeric>
#include <random>
#include <set>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/editdistance.hpp>

const auto max_n = 1000000;
#include "common.hpp"

// Length of the strings to use for benchmarking
const auto string_length = 80;

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

// Return n random strings of length l
std::vector<std::string> random_strings(size_t n, size_t l) {
  std::vector<std::string> S;
  for (size_t i = 0; i < n; i++) {
    S.push_back(random_string(l));
  }
  return S;
}

static void edit_distance_compute_seq(benchmark::State& state) {
  size_t n = state.range(0);
  auto S = random_strings(n, string_length);
  auto s = random_string(string_length);
  
  int ans;
  for (auto _ : state) {
    int min = edit_distance(S[0], s);
    for (size_t i = 1; i < n; i++) {
      min = std::min(min, edit_distance(S[i], s));
    }
    benchmark::DoNotOptimize(ans = min);
  }
}

template<typename It>
int min_edit_distance_par(It begin, It end, const std::string& s) {
  if (begin + 1 == end) {
    return edit_distance(*begin, s);
  }
  else {
    auto mid = begin + std::distance(begin, end) / 2;
    int left_ans, right_ans;
    parlay::par_do(
      [&]() { left_ans = min_edit_distance_par(begin, mid, s); },
      [&]() { right_ans = min_edit_distance_par(mid, end, s); }
    );
    return std::min(left_ans, right_ans);
  }
}

static void edit_distance_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  size_t n = state.range(1);
  auto S = random_strings(n, string_length);
  auto s = random_string(string_length);

  int ans;
  for (auto _ : state) {
    benchmark::DoNotOptimize(ans = min_edit_distance_par(std::begin(S), std::end(S), s));
  }
}

PSAC_STATIC_BENCHMARK(edit_distance_compute)(benchmark::State& state) {
  size_t n = state.range(1);
 
  std::vector<psac::Mod<std::string>> Sm(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&Sm[i], random_string(string_length));
  }  
  auto s = random_string(string_length);
  psac::Mod<int> res;

  for (auto _ : state) {
    comp = psac_run(reduce_edit_distance, std::begin(Sm), std::end(Sm), &s, &res);
    record_stats(state);
  }
}

PSAC_DYNAMIC_BENCHMARK(edit_distance_update)(benchmark::State& state) {
  size_t n = state.range(1);
  size_t k = state.range(2);

  std::vector<psac::Mod<std::string>> Sm(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&Sm[i], random_string(string_length));
  }  
  auto s = random_string(string_length);
  psac::Mod<int> res;

  comp = psac_run(reduce_edit_distance, std::begin(Sm), std::end(Sm), &s, &res);

  for (auto _ : state) {
    // Make random changes
    state.PauseTiming();
    std::set<int> changes;
    while (changes.size() < k) {
      changes.insert(rand_int() % n);
    }
    std::vector<std::pair<int,std::string>> updates;
    for (int i : changes) {
      updates.emplace_back(i, random_string(string_length));
    }
    state.ResumeTiming();

    // Write all updates
    parlay::parallel_for(0, k, [&](auto i) {
      psac_write(&Sm[updates[i].first], std::move(updates[i].second));
    });

    // Propagate changes
    psac_propagate(comp);
    record_stats(state);
  }

  finalize(state);
}

REGISTER_STATIC_BENCHMARK(edit_distance_compute);
REGISTER_DYNAMIC_BENCHMARK(edit_distance_update);

