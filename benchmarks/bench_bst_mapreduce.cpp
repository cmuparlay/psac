#include <algorithm>
#include <numeric>
#include <random>
#include <set>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/bst.hpp>
#include <psac/examples/editdistance.hpp>

const auto max_n = 100000;
#include "psac/examples/bst-primitives.hpp"
#include "common.hpp"

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

const size_t GRAN_LIMIT = 128;
const std::string bank = "abcdefghijklmnopqrstuvwxyz";

std::string create_random_string(size_t L) {
    std::string ret("");
    for (size_t i = 0; i < L; i++) ret += bank[rand_int() % bank.length()];
    return ret;
}

std::vector<std::pair<int, std::string>> create_random_vector(size_t n, size_t L) {
    std::vector<std::pair<int, std::string>> vec;
    for (size_t i = 0; i < n; i++) {
        int num = rand_int() % (100 * n);
        std::string s = create_random_string(L);
        vec.push_back(std::make_pair(num, s));
    }
    return vec;
}

static void mapreduce_compute_seq(benchmark::State& state) {
    size_t n = state.range(0);
    StaticBst<int, std::string, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n, 10);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);
    std::string target("juggernaut");

    auto map_fn = [&](std::string s) { return edit_distance(s, target); };
    auto reduce_fn = [&](int a, int b) { return std::min(a, b); };

    StaticReduceNode<int>* ans;
    for (auto _ : state) {
        StaticReduceNode<int>* result = bst.mapreduce_seq_helper(bst.root, INT_MAX, map_fn, reduce_fn);
        benchmark::DoNotOptimize(ans = result);
    }
}

static void mapreduce_compute_par(benchmark::State& state) {
    parlay::set_num_workers(state.range(0));
    size_t n = state.range(1);
    StaticBst<int, std::string, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n, 10);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);
    std::string target("juggernaut");

    auto map_fn = [&](std::string s) { return edit_distance(s, target); };
    auto reduce_fn = [&](int a, int b) { return std::min(a, b); };

    StaticReduceNode<int>* ans;
    for (auto _ : state) {
        StaticReduceNode<int>* result = bst.mapreduce_par_helper(bst.root, INT_MAX, map_fn, reduce_fn);
        benchmark::DoNotOptimize(ans = result);
    }
}

PSAC_STATIC_BENCHMARK(mapreduce_compute)(benchmark::State& state) {
    size_t n = state.range(1);
    Bst<int, std::string, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n, 10);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);
    std::string target("juggernaut");

    auto map_fn = [&](std::string s) { return edit_distance(s, target); };
    auto reduce_fn = [&](int a, int b) { return std::min(a, b); };

    ReduceNode<int> res;
    for (auto _ : state) {
        comp = psac_run(bst.mapreduce, bst.root.value, &res, INT_MAX, map_fn, reduce_fn);
        record_stats(state);
    }
}

PSAC_DYNAMIC_BENCHMARK(mapreduce_update)(benchmark::State& state) {
    size_t n = state.range(1);
    size_t k = state.range(2);

    Bst<int, std::string, int> bst(GRAN_LIMIT);
    auto vec = create_random_vector(n, 10);
    std::sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
    bst.batch_insert(vec);
    std::string target("juggernaut");

    auto map_fn = [&](std::string s) { return edit_distance(s, target); };
    auto reduce_fn = [&](int a, int b) { return std::min(a, b); };

    ReduceNode<int> res;
    comp = psac_run(bst.mapreduce, bst.root.value, &res, INT_MAX, map_fn, reduce_fn);

    for (auto _ : state) {
        // Insert random elements
        state.PauseTiming();
        auto updates = create_random_vector(k, 10);
        std::sort(updates.begin(), updates.end());
        updates.erase(unique(updates.begin(), updates.end()), updates.end());
        state.ResumeTiming();
        bst.batch_insert(updates);

        psac_propagate(comp);
        record_stats(state);
    }
    finalize(state);
}

REGISTER_STATIC_BENCHMARK(mapreduce_compute);
REGISTER_DYNAMIC_BENCHMARK(mapreduce_update);