#include <numeric>
#include <random>
#include <set>
#include <vector>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>

template<typename It, typename It2, typename It3>
psac_function(multiread, It mod_begin, It mod_end, It2 read_begin, It2 read_end, It3 out_begin, It3 out_end) {
  int n = read_end - read_begin;
  psac_parallel_for(int i, 0, n, 1000, {
    size_t idx = read_begin[i];
    auto mod_it = mod_begin + idx;
    auto out_it = out_begin + idx;
    psac_read((int x), (mod_it), {
      psac_write(out_it, x);
    });
  });
}

static void bench_multireads(benchmark::State& state) {
  size_t n = 1000000;
  size_t m = state.range(0);
  std::vector<psac::Mod<int>> ins(m);
  std::vector<size_t> reads(n);
  std::vector<psac::Mod<int>> outs(n);

  for (size_t i = 0; i < m; i++) {
    ins[i].write(i);
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, m-1);

  for (auto _ : state) {
    state.PauseTiming();
    for (size_t i = 0; i < n; i++) {
      reads[i] = dis(gen);
    }
    state.ResumeTiming();
    auto comp = psac_run(multiread, std::begin(ins), std::end(ins), std::begin(reads), std::end(reads), std::begin(outs), std::end(outs));
    state.PauseTiming();
    comp.destroy();
    psac::GarbageCollector::run();
    state.ResumeTiming();
  }

}

static void bench_multireads_update(benchmark::State& state) {
  size_t n = 1000000;
  size_t m = state.range(0);
  std::vector<psac::Mod<int>> ins(m);
  std::vector<size_t> reads(n);
  std::vector<psac::Mod<int>> outs(n);

  for (size_t i = 0; i < m; i++) {
    ins[i].write(i);
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, m-1);

  for (auto _ : state) {
    state.PauseTiming();
    for (size_t i = 0; i < n; i++) {
      reads[i] = dis(gen);
    }
    auto comp = psac_run(multiread, std::begin(ins), std::end(ins), std::begin(reads), std::end(reads), std::begin(outs), std::end(outs));
    std::vector<size_t> new_vals(m);
    for (size_t i = 0; i < m; i++) {
      new_vals[i] = ins[i].value + 1;
    }
    state.ResumeTiming();

    parlay::parallel_for(0,m, [&](int i) {
      ins[i].write(new_vals[i]);
    });

    psac_propagate(comp);

    state.PauseTiming();
    comp.destroy();
    psac::GarbageCollector::run();
    new_vals.clear();
    state.ResumeTiming();
  }

}

BENCHMARK(bench_multireads)->Unit(benchmark::kMillisecond)->UseRealTime()->RangeMultiplier(10)->Range(1, 1000000);
BENCHMARK(bench_multireads_update)->Unit(benchmark::kMillisecond)->UseRealTime()->RangeMultiplier(10)->Range(1, 1000000);
