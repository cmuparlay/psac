#include <vector>
#include <algorithm>
#include <utility>
#include <random>
#include <cmath>

#include <benchmark/benchmark.h>
#include <psac/psac.hpp>

const auto max_n = 1000000;
#include "psac/examples/convex-hull-primitives.hpp"
#include "psac/examples/convex-hull.hpp"
#include "common.hpp"

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

const size_t GRAN_LIMIT = 64;
const size_t MOD = 1e9 + 7;
const size_t MAX = 1e6 + 3;

std::vector<std::pair<int,int>> get_points(size_t n) {
    std::vector<std::pair<int,int>> points;
    for (size_t i = 0; i < n; i++) {
        points.push_back(std::make_pair(rand_int() % MAX, rand_int() % MAX));
    }
    std::sort(points.begin(), points.end());
    points.erase(unique(points.begin(), points.end()), points.end());
    for (size_t i = 0; i < points.size(); i++) {
        int tmp = points[i].first;
        points[i].first = points[i].second;
        points[i].second = tmp;
    }
    return points;
}

static void convex_hull_compute_seq(benchmark::State& state) {
    size_t n = state.range(0);

    std::vector<std::pair<int,int>> points = get_points(n);
    for (auto _ : state) {
        StaticLCHull hull(points, GRAN_LIMIT, MOD);
        benchmark::DoNotOptimize(hull);
    }
}

static void convex_hull_compute_par(benchmark::State& state) {
    parlay::set_num_workers(state.range(0));
    size_t n = state.range(1);

    std::vector<std::pair<int,int>> points = get_points(n);
    for (auto _ : state) {
        StaticLCHull hull(points, GRAN_LIMIT, GRAN_LIMIT);
        benchmark::DoNotOptimize(hull);
    }
}

PSAC_STATIC_BENCHMARK(convex_hull_compute)(benchmark::State& state) {
    size_t n = state.range(1);

    std::vector <std::pair<int,int>> points = get_points(n);
    LCHull hull = LCHull(points, GRAN_LIMIT);

    for (auto _ : state) {
        comp = psac_run(hull.build);
        record_stats(state);
    }
}

PSAC_DYNAMIC_BENCHMARK(convex_hull_update)(benchmark::State& state) {
    size_t n = state.range(1);
    size_t k = state.range(2);

    std::vector <std::pair<int,int>> points = get_points(n);
    LCHull hull(points, GRAN_LIMIT);
    comp = psac_run(hull.build);

    for (auto _ : state) {
        // Insert random elements
        state.PauseTiming();
        std::vector <std::pair<int,int>> tmp = get_points(k);
        std::vector <Point> updates;
        for (size_t i = 0; i < tmp.size(); i++) {
            updates.push_back(Point{tmp[i].first, tmp[i].second});
        }
        state.ResumeTiming();

        hull.batch_insert(updates);
        psac_propagate(comp);
        record_stats(state);
    }
    finalize(state);
}

REGISTER_STATIC_BENCHMARK(convex_hull_compute);
REGISTER_DYNAMIC_BENCHMARK(convex_hull_update);