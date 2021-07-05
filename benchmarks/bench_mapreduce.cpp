#include <numeric>
#include <random>
#include <set>
#include <vector>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/mapreduce.hpp>

const auto max_n = 10000000;
#include "common.hpp"

// ============================ Vanilla Map ======================================

// Sequential baseline for map
static void map_compute_seq(benchmark::State& state) {
  std::vector<int> A(state.range(0)), B(state.range(0));
  for (auto _ : state) {
    std::transform(std::begin(A), std::end(A), std::begin(B), [](int x) { return 2 * x; });
  }
}

// Parallel static baseline for map
static void map_compute_par(benchmark::State& state) { 
  parlay::set_num_workers(state.range(0));
  std::vector<int> A(state.range(1)), B(state.range(1));
  for (int i = 0; i < state.range(1); i++) A[i] = i;

  for (auto _ : state) {
    auto f = [](int x) { return 2 * x; };
    parlay::parallel_for(0, state.range(1), [&](auto i) { B[i] = f(A[i]); });
  }
}

// Parallel self-adjusting benchmark for static map
PSAC_STATIC_BENCHMARK(map_compute)(benchmark::State& state) {
  size_t n = state.range(1);
  std::vector<psac::Mod<int>> A(n), B(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&A[i], i);
  }

  // Benchmarks
  for (auto _ : state) {
    comp = psac_run(map, std::begin(A), std::end(A), std::begin(B), [](int x) { return 2*x; });
    record_stats(state);
  }
}

// Parallel self-adjusting benchmark for dynamic map
PSAC_DYNAMIC_BENCHMARK(map_update)(benchmark::State& state) { 
  
  size_t n = state.range(1);
  size_t k = state.range(2);
  std::vector<psac::Mod<int>> A(n), B(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&A[i], i);
  }

  std::mt19937 gen(1);
  std::uniform_int_distribution<int> dis_j(0, n - 1);
  std::uniform_int_distribution<int> dis_v(0, 1000000000);

  comp = psac_run(map, std::begin(A), std::end(A), std::begin(B), [](int x) { return 2*x; });

  for (auto _ : state) {
    // Make random changes
    state.PauseTiming();
    std::set<int> changes;
    while (changes.size() < k) {
      changes.insert(dis_j(gen));
    }
    std::vector<std::pair<int,int>> updates;
    for (auto i : changes) {
      updates.emplace_back(i, dis_v(gen));
    }
    state.ResumeTiming();

    // Write updates
    parlay::parallel_for(0, k, [&](auto i) {
      psac_write(&A[updates[i].first], updates[i].second);
    });

    // Propagate changes
    psac_propagate(comp);
    record_stats(state);
  }

  finalize(state);
}

// ============================ Vanilla Reduce ======================================

// Sequential baseline for reduce
static void reduce_compute_seq(benchmark::State& state) {
  std::vector<int> A(state.range(0)), B(state.range(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(std::accumulate(std::begin(A), std::end(A), 0));
  }
}

template<typename RandomAccessIterator>
int parallel_sum(RandomAccessIterator begin, RandomAccessIterator end) {
  if (begin + 1 == end) {
    return *begin;
  }
  else {
    int left, right;
    auto mid = begin + (end - begin) / 2;
    parlay::par_do(
      [&]() { left = parallel_sum(begin, mid); },
      [&]() { right = parallel_sum(mid, end); }
    );
    return left + right;
  }
}

// Parallel static baseline for reduce
static void reduce_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  std::vector<int> A(state.range(1));
  for (int i = 0; i < state.range(1); i++) A[i] = i;

  for (auto _ : state) {
    benchmark::DoNotOptimize(parallel_sum(A.begin(), A.end()));
  }
}

// Parallel self-adjusting benchmark for static reduce
PSAC_STATIC_BENCHMARK(reduce_compute)(benchmark::State& state) {
  size_t n = state.range(1);
  std::vector<psac::Mod<int>> A(n), B(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&A[i], i);
  }
  psac::Mod<int> result;

  // Benchmarks
  for (auto _ : state) {
    comp = psac_run(sum, std::begin(A), std::end(A), &result);
    record_stats(state);
  }
}

// =========================== Chunky Map ======================================

// Parallel self-adjusting benchmark for static chunky map
PSAC_STATIC_BENCHMARK(map_chunks_compute)(benchmark::State& state) {
  size_t n = (state.range(1) + chunk_size - 1) / chunk_size;
  std::vector<psac::Mod<int_chunk>> A(n), B(n);
  for (size_t i = 0; i < n; i++) {
    int_chunk a;
    for (size_t j = 0; j < chunk_size; j++) {
      a[j] = i;
    }
    psac_write(&A[i], a);
  }

  // Benchmarks
  for (auto _ : state) {
    comp = psac_run(map_chunks, std::begin(A), std::end(A), std::begin(B), [](int x) { return 2*x; });
    record_stats(state);
  }
}

// Parallel self-adjusting benchmark for dynamic chunky map
PSAC_DYNAMIC_BENCHMARK(map_chunks_update)(benchmark::State& state) { 
  
  size_t n = (state.range(1) + chunk_size - 1) / chunk_size;
  size_t k = state.range(2);
  std::vector<psac::Mod<int_chunk>> A(n), B(n);
  for (size_t i = 0; i < n; i++) {
    int_chunk a;
    for (size_t j = 0; j < chunk_size; j++) {
      a[j] = i;
    }
    psac_write(&A[i], a);
  }

  std::mt19937 gen(1);
  std::uniform_int_distribution<int> dis_j(0, n * chunk_size - 1);
  std::uniform_int_distribution<int> dis_v(0, 1000000000);

  comp = psac_run(map_chunks, std::begin(A), std::end(A), std::begin(B), [](int x) { return 2*x; });

  for (auto _ : state) {
    // Make random changes
    state.PauseTiming();
    for (size_t i = 0; i < k; i++) {
      int j = dis_j(gen);
      int v = dis_v(gen);

      A[j/chunk_size].value[j%chunk_size] = v;
      A[j/chunk_size].write(A[j%chunk_size].value);
    }
    state.ResumeTiming();

    // Propagate changes
    psac_propagate(comp);
    record_stats(state);
  }

  finalize(state);
}

// =========================== Shuffle Map ======================================

static void shuffle_map_compute_seq(benchmark::State& state) {
  std::vector<int> A(state.range(0)), B(state.range(0)), P(state.range(0));
  std::iota(std::begin(A), std::end(A), 0);
  std::iota(std::begin(P), std::end(P), 0);
  std::shuffle(std::begin(A), std::end(A), std::default_random_engine(1));
  std::shuffle(std::begin(P), std::end(P), std::default_random_engine(0));
  std::function<int(int)> f = [](int x) { return 2 * x; };
  for (auto _ : state) {
    for (int i = 0; i < state.range(0); i++) {
      B[i] = f(A[P[i]]);
    }
  }
}

static void shuffle_map_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  std::vector<int> A(state.range(1)), B(state.range(1)), P(state.range(1));
  std::iota(std::begin(A), std::end(A), 0);
  std::iota(std::begin(P), std::end(P), 0);
  std::shuffle(std::begin(A), std::end(A), std::default_random_engine(1));
  std::shuffle(std::begin(P), std::end(P), std::default_random_engine(0));
  std::function<int(int)> f = [](int x) { return 2 * x; };
  for (auto _ : state) {
    parlay::parallel_for(0, state.range(1), [&](auto i) {
      B[i] = f(A[P[i]]);
    });
  }
}

PSAC_STATIC_BENCHMARK(shuffle_map_compute)(benchmark::State& state) {
  size_t n_chunks = (state.range(1) + chunk_size - 1) / chunk_size;
  std::vector<psac::Mod<int>> A(state.range(1));
  std::vector<psac::Mod<int_chunk>> B(n_chunks);
  std::vector<int> P(state.range(1));
  std::iota(std::begin(P), std::end(P), 0);
  std::shuffle(std::begin(P), std::end(P), std::default_random_engine(0));
  
  for (auto _ : state) {
    comp = psac_run(shuffle_map, std::begin(A), std::end(A), std::begin(P), std::begin(B), [](int x) { return 2 * x; });
    record_stats(state);
  }
}

PSAC_DYNAMIC_BENCHMARK(shuffle_map_update)(benchmark::State& state) { 

  size_t n = state.range(1);
  size_t n_chunks = (state.range(1) + chunk_size - 1) / chunk_size;
  size_t k = state.range(2);

  std::vector<psac::Mod<int>> A(n);
  for (size_t i = 0; i < n; i++) {
    A[i].write(i);
  }

  std::vector<psac::Mod<int_chunk>> B(n_chunks);

  std::vector<int> P(state.range(1));
  std::iota(std::begin(P), std::end(P), 0);
  std::shuffle(std::begin(P), std::end(P), std::default_random_engine(0));

  std::mt19937 gen(1);
  std::uniform_int_distribution<int> dis_j(0, n);
  std::uniform_int_distribution<int> dis_v(0, 1000000000);

  comp = psac_run(shuffle_map, std::begin(A), std::end(A), std::begin(P), std::begin(B), [](int x) { return 2*x; });

  for (auto _ : state) {
    // Make random changes
    state.PauseTiming();
    for (size_t i = 0; i < k; i++) {
      int j = dis_j(gen);
      int v = dis_v(gen);
      A[j].write(v);
    }
    state.ResumeTiming();

    // Propagate changes
    psac_propagate(comp);
    record_stats(state);
  }

  finalize(state);
}


REGISTER_STATIC_BENCHMARK(map_compute);
REGISTER_DYNAMIC_BENCHMARK(map_update);

REGISTER_STATIC_BENCHMARK(reduce_compute);

//REGISTER_STATIC_BENCHMARK(shuffle_map_compute);
//REGISTER_DYNAMIC_BENCHMARK(shuffle_map_update);

