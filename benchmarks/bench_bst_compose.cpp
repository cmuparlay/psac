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

std::vector<std::pair<int, int>> create_random_vector(size_t n) {
    std::vector<std::pair<int, int>> vec;
    for (size_t i = 0; i < n; i++) {
        int num1 = rand_int() % (100 * n);
        int num2 = rand_int() % (100 * n);
        vec.push_back(std::make_pair(num1, num2));
    }
    return vec;
}

static void filter_mapreduce_compute_seq(benchmark::State& state) {
    size_t n = state.range(0);
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);

    auto filter_fn = [](int a) { return a % 2 == 0; };
    auto map_fn = [](int a) { return a / 2; };
    auto reduce_fn = [](int a, int b) { return a + b; };

    StaticReduceNode<int>* ans;
    for (auto _ : state) {
        StaticNodePtr<int, int> filtered = bst.filter_seq_helper(bst.root, filter_fn);
        StaticReduceNode<int>* result = bst.mapreduce_seq_helper(filtered, INT_MAX, map_fn, reduce_fn);
        benchmark::DoNotOptimize(ans = result);
    }
}

static void filter_mapreduce_compute_par(benchmark::State& state) {
    parlay::set_num_workers(state.range(0));
    size_t n = state.range(1);
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);

    auto filter_fn = [](int a) { return a % 2 == 0; };
    auto map_fn = [](int a) { return a / 2; };
    auto reduce_fn = [](int a, int b) { return a + b; };

    StaticReduceNode<int>* ans;
    for (auto _ : state) {
        StaticNodePtr<int, int> filtered = bst.filter_par_helper(bst.root, filter_fn);
        StaticReduceNode<int>* result = bst.mapreduce_par_helper(filtered, INT_MAX, map_fn, reduce_fn);
        benchmark::DoNotOptimize(ans = result);
    }
}

PSAC_STATIC_BENCHMARK(filter_mapreduce_compute)(benchmark::State& state) {
    size_t n = state.range(1);
    Bst<int, int, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);

    auto filter_fn = [](int a) { return a % 2 == 0; };
    auto map_fn = [](int a) { return a / 2; };
    auto reduce_fn = [](int a, int b) { return a + b; };

    ReduceNode<int> res;
    for (auto _ : state) {
        comp = psac_run(bst.filtermapreduce, bst.root.value, &res, INT_MAX, filter_fn, map_fn, reduce_fn);
        record_stats(state);
    }
}

PSAC_DYNAMIC_BENCHMARK(filter_mapreduce_update)(benchmark::State& state) {
    size_t n = state.range(1);
    size_t k = state.range(2);

    Bst<int, int, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);

    auto filter_fn = [](int a) { return a % 2 == 0; };
    auto map_fn = [](int a) { return a / 2; };
    auto reduce_fn = [](int a, int b) { return a + b; };

    ReduceNode<int> res;
    comp = psac_run(bst.filtermapreduce, bst.root.value, &res, INT_MAX, filter_fn, map_fn, reduce_fn);

    for (auto _ : state) {
        // Insert random elements
        state.PauseTiming();
        auto updates = create_random_vector(k);
        std::sort(updates.begin(), updates.end());
        updates.erase(unique(updates.begin(), updates.end()), updates.end());
        state.ResumeTiming();
        bst.batch_insert(updates);

        psac_propagate(comp);
        record_stats(state);
    }
    finalize(state);
}

REGISTER_STATIC_BENCHMARK(filter_mapreduce_compute);
REGISTER_DYNAMIC_BENCHMARK(filter_mapreduce_update);