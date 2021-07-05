#include <algorithm>
#include <numeric>
#include <random>
#include <set>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/bst.hpp>

const auto max_n = 1000000;

#include "psac/examples/bst-primitives.hpp"
#include "common.hpp"

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

const size_t GRAN_LIMIT = 128;
const size_t MOD = 1e9 + 7;
auto f = ([](int x) {
    if (x % 2 == 1) return true;
    else return false;
});

void insert_random_nums(StaticBst<int, int, int>& bst, size_t n, size_t M) {
    for (size_t i = 0; i < n; i++) {
        int num = rand_int() % M;
        bst.insert(num, num);
    }
}

static void filter_compute_seq(benchmark::State& state) {
    parlay::set_num_workers(1);
    size_t n = state.range(0);
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    insert_random_nums(bst, n, MOD);

    StaticNodePtr<int, int> ans;
    for (auto _ : state) {
        StaticNodePtr<int, int> filtered = bst.filter_seq_helper(bst.root, f);
        benchmark::DoNotOptimize(ans = filtered);
    }
//    Allocator::qterminate();
}

static void filter_compute_par(benchmark::State& state) {
    parlay::set_num_workers(state.range(0));
    size_t n = state.range(1);
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    insert_random_nums(bst, n, MOD);

    StaticNodePtr<int, int> ans;
    for (auto _ : state) {
        StaticNodePtr<int, int> filtered = bst.filter_par_helper(bst.root, f);
        benchmark::DoNotOptimize(ans = filtered);
    }
//    Allocator::qterminate();
}

static void batch_insert (benchmark::State& state) {
    parlay::set_num_workers(state.range(0));
    size_t n = state.range(1);

    std::vector <std::pair<int,int>> nums(n);
    for (size_t i = 0; i < n; i++) {
        int num = rand_int() % MOD;
        nums[i] = std::make_pair(num, num);
    }
    std::sort(nums.begin(), nums.end());
    nums.erase(unique(nums.begin(), nums.end()), nums.end());
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(nums);
    psac::Mod<NodePtr<int, int>> res;

    Bst<int, int, int>* ans;
    for (auto _ : state) {
        // Insert random elements
        state.PauseTiming();
        std::vector <std::pair<int,int>> updates(100);
        for (size_t i = 0; i < 100; i++) {
            int update = rand_int() % MOD;
            updates[i] = std::make_pair(update, update);
        }
        std::sort(updates.begin(), updates.end());
        updates.erase(unique(updates.begin(), updates.end()), updates.end());
        state.ResumeTiming();

        bst.batch_insert(updates);
        benchmark::DoNotOptimize(ans = &bst);
    }
}

PSAC_STATIC_BENCHMARK(filter_compute)(benchmark::State& state) {
    size_t n = state.range(1);

    std::vector <std::pair<int, int>> nums(n);
    for (size_t i = 0; i < n; i++) {
        int num = rand_int() % MOD;
        nums[i] = std::make_pair(num, num);
    }
    std::sort(nums.begin(), nums.end());
    nums.erase(unique(nums.begin(), nums.end()), nums.end());
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(nums);
    psac::Mod<NodePtr<int, int>> res;

    for (auto _ : state) {
        comp = psac_run(bst.filter, bst.root.value, &res, f);
        record_stats(state);
    }
}

PSAC_DYNAMIC_BENCHMARK(filter_update)(benchmark::State& state) {
    size_t n = state.range(1);
    size_t k = state.range(2);

    std::vector <std::pair<int, int>> nums(n);
    for (size_t i = 0; i < n; i++) {
        int num = rand_int() % MOD;
        nums[i] = std::make_pair(num, num);
    }
    std::sort(nums.begin(), nums.end());
    nums.erase(unique(nums.begin(), nums.end()), nums.end());
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(nums);

    psac::Mod<NodePtr<int, int>> res;
    comp = psac_run(bst.filter, bst.root.value, &res, f);

    for (auto _ : state) {
        // Insert random elements
        state.PauseTiming();
        std::vector <std::pair<int,int>> updates(k);
        for (size_t i = 0; i < k; i++) {
            int update = rand_int() % MOD;
            updates[i] = std::make_pair(update, update);
        }
        std::sort(updates.begin(), updates.end());
        updates.erase(unique(updates.begin(), updates.end()), updates.end());
        state.ResumeTiming();
        bst.batch_insert(updates);

        psac_propagate(comp);
        record_stats(state);
    }
    finalize(state);
}

REGISTER_STATIC_BENCHMARK(filter_compute);
REGISTER_DYNAMIC_BENCHMARK(filter_update);

static void filter_reduce_compute_seq(benchmark::State& state) {
  parlay::set_num_workers(1);
  size_t n = state.range(0);
  StaticBst<int, int, int> bst(GRAN_LIMIT);
  insert_random_nums(bst, n, MOD);

  StaticNodePtr<int, int> ans;
  for (auto _ : state) {
    StaticNodePtr<int, int> filtered = bst.filter_seq_helper(bst.root, f);
    benchmark::DoNotOptimize(ans = filtered);
  }
}

static void filter_reduce_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  size_t n = state.range(1);
  StaticBst<int, int, int> bst(GRAN_LIMIT);
  insert_random_nums(bst, n, MOD);

  StaticNodePtr<int, int> ans;
  for (auto _ : state) {
    StaticNodePtr<int, int> filtered = bst.filter_par_helper(bst.root, f);
    benchmark::DoNotOptimize(ans = filtered);
  }
}

// Composes filter with reduce. Assumes values are integers that are reduced by max
psac_function(filter_reduce, Bst<int,int,int>* bst, psac::Mod<NodePtr<int,int>>* filter_result, psac::Mod<int>* result) {
  psac_read((auto r), (&(bst->root)), {
    psac_call((*bst).filter, r, filter_result, f);
    psac_read((auto node), (filter_result), {
      ReduceNode<int>* reduce_result = bst->make_reducenode();
      psac_call((*bst).mapreduce, node, reduce_result, 0,
                [](int a) { return 2 * a; },
                [](int a, int b) { return std::max(a, b); });
      psac_read((int val), (&(reduce_result->val)), {
        psac_write(result, val);
      });
    });
  });
}

PSAC_STATIC_BENCHMARK(filter_reduce_compute)(benchmark::State& state) {
  size_t n = state.range(1);

  std::vector <std::pair<int, int>> nums(n);
  for (size_t i = 0; i < n; i++) {
    int num = rand_int() % MOD;
    nums[i] = std::make_pair(num, num);
  }
  std::sort(nums.begin(), nums.end());
  nums.erase(unique(nums.begin(), nums.end()), nums.end());
  Bst<int, int, int> bst(GRAN_LIMIT);
  bst.batch_insert(nums);
  psac::Mod<NodePtr<int,int>> filter_res;
  psac::Mod<int> res;

  for (auto _ : state) {
    comp = psac_run(filter_reduce, &bst, &filter_res, &res);
    record_stats(state);
  }
}

PSAC_DYNAMIC_BENCHMARK(filter_reduce_update)(benchmark::State& state) {
  size_t n = state.range(1);
  size_t k = state.range(2);

  std::vector <std::pair<int, int>> nums(n);
  for (size_t i = 0; i < n; i++) {
    int num = rand_int() % MOD;
    nums[i] = std::make_pair(num, num);
  }
  std::sort(nums.begin(), nums.end());
  nums.erase(unique(nums.begin(), nums.end()), nums.end());
  Bst<int, int, int> bst(GRAN_LIMIT);
  bst.batch_insert(nums);

  psac::Mod<NodePtr<int,int>> filter_res;
  psac::Mod<int> res;
  comp = psac_run(filter_reduce, &bst, &filter_res, &res);

  for (auto _ : state) {
    // Insert random elements
    state.PauseTiming();
    std::vector <std::pair<int,int>> updates(k);
    for (size_t i = 0; i < k; i++) {
      int update = rand_int() % MOD;
      updates[i] = std::make_pair(update, update);
    }
    std::sort(updates.begin(), updates.end());
    updates.erase(unique(updates.begin(), updates.end()), updates.end());
    state.ResumeTiming();
    bst.batch_insert(updates);

    psac_propagate(comp);
    record_stats(state);
  }
  finalize(state);
}

REGISTER_STATIC_BENCHMARK(filter_reduce_compute);
REGISTER_DYNAMIC_BENCHMARK(filter_reduce_update);
