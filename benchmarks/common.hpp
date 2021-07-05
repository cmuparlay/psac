// Methods and macros for setting up benchmarks

#ifndef BENCHMARKS_COMMON_HPP_
#define BENCHMARKS_COMMON_HPP_

#include <chrono>
#include <numeric>

// Define a benchmark for the initial run of a psac algorithm
#define PSAC_STATIC_BENCHMARK(name)                                                           \
  BENCHMARK_DEFINE_F(PsacStaticFixture, name ## _psac)

// Define a benchmark for a dynamic update of a psac algorithm
#define PSAC_DYNAMIC_BENCHMARK(name)                                                          \
  BENCHMARK_DEFINE_F(PsacDynamicFixture, name ## _psac)

// Register a series of benchmarks for the initial computation of an algorithm
// Compares a static sequential algorithm, a static parallel
// algorithm and the parallel self-adjusting algorithm
#define REGISTER_STATIC_BENCHMARK(name)                                                       \
  BENCHMARK(name ## _seq)->Unit(benchmark::kMillisecond)->UseRealTime()                       \
                     ->Apply(SequentialBaseline);                                             \
                                                                                              \
  BENCHMARK(name ## _par)->Unit(benchmark::kMillisecond)->UseRealTime()                       \
                     ->Apply(ParallelBaseline);                                               \
                                                                                              \
  BENCHMARK_REGISTER_F(PsacStaticFixture, name ## _psac)->Unit(benchmark::kMillisecond)       \
                      ->UseRealTime()                                                         \
                      ->Apply(PsacComputation);

// Register a benchmark for an update propagation test
#define REGISTER_DYNAMIC_BENCHMARK(name)                                                      \
  BENCHMARK_REGISTER_F(PsacDynamicFixture, name ## _psac)->Unit(benchmark::kMillisecond)      \
                          ->UseRealTime()                                                     \
                          ->Apply(PsacUpdate);

// Returns the sequence of core numbers to be used for benchmarking.
// Uses powers of 2 until the maximum number of cores, plus double
// the number of cores for hyperthreading
std::vector<int> get_core_series() {
  const auto num_cores = static_cast<int>(std::thread::hardware_concurrency() / 2);
  std::vector<int> ps; 
  for (int p = 1; p < num_cores; p = 2*p) ps.push_back(p);
  ps.push_back(num_cores);
  ps.push_back(num_cores * 2);
  return ps;
}

// Argument generator for static sequential baselines. Varies the
// number of threads, keeping the input size n at its maximum value
static void SequentialBaseline(benchmark::internal::Benchmark* b) {
  b->Arg(max_n);
}

// Argument generator for static parallel baselines. Varies the
// number of threads, keeping the input size n at its maximum value
static void ParallelBaseline(benchmark::internal::Benchmark* b) {
  for (int p : get_core_series()) {
    b->Args({p, max_n});
  }
}

// Argument generator for initial computations. Varies the number
// of threads, keeping the input size n at its maximum value.
static void PsacComputation(benchmark::internal::Benchmark* b) {
  for (int p : get_core_series()) {
    b->Args({p, max_n});
  }
}

// Argument generator for update tests. Varies the number of threads
// and the batch size of the updates, with maximum input size n.
static void PsacUpdate(benchmark::internal::Benchmark* b) {
  for (int p : get_core_series()) {
    for (int k = 1; k <= max_n; k *= 10) {
      b->Args({p, max_n, k});
    }
  }
}

// ------------------------------------------------------------------
//                Fixture for static tests
//
// Records the average tree size and the amount of memory used
// by storing the SP trees.
// ------------------------------------------------------------------
struct PsacStaticFixture : public benchmark::Fixture {
  void SetUp(const ::benchmark::State& state) {
    parlay::set_num_workers(state.range(0));  // Set the number of worker threads
  }

  void TearDown(const ::benchmark::State& state) {
    assert(comp.root == nullptr);
    size_t avg_ts = std::accumulate(std::begin(ts), std::end(ts), size_t(0)) / (double)ts.size();
    size_t avg_mem = std::accumulate(std::begin(mems), std::end(mems), size_t(0)) / (double)mems.size();
    double avg_gc_time = std::accumulate(std::begin(gc_time), std::end(gc_time), 0.0) / gc_time.size();
    auto& nc_state = const_cast<benchmark::State&>(state);
    nc_state.counters["SP ts"] = benchmark::Counter(avg_ts);
    nc_state.counters["SP mem"] = benchmark::Counter(avg_mem);
    nc_state.counters["SP cleanup"] = benchmark::Counter(avg_gc_time);
  }
  
  void record_stats(benchmark::State& state) { 
    // Pause the timers since we do not want to count the destructor
    state.PauseTiming();
    ts.push_back(comp.treesize());  
    mems.push_back(comp.memory());

    // Time the destruction
    auto start = std::chrono::high_resolution_clock::now();
    comp.destroy();
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_seconds = std::chrono::duration<double>(end - start).count();
    gc_time.push_back(elapsed_seconds);

    state.ResumeTiming();
  }

  psac::Computation comp;      // Recorded SP tree
  std::vector<size_t> ts;      // Recorded SP tree sizes
  std::vector<size_t> mems;    // Recorded SP tree memory usage
  std::vector<double> gc_time; // Recorded time to destroy SP tree
};

// ------------------------------------------------------------------
//                Fixture for dynamic update tests
//
// Records the average tree size and the amount of memory used
// by storing the SP trees, and the memory used by the garbage
// collector.
// ------------------------------------------------------------------
struct PsacDynamicFixture : public benchmark::Fixture {
  void SetUp(const ::benchmark::State& state) {
    parlay::set_num_workers(state.range(0));  // Set the number of worker threads
  }

  void record_stats(benchmark::State& state) {
    state.PauseTiming();

    // Garbage collection
    gc_nodes.push_back(psac::GarbageCollector::nodes());
    gc_mems.push_back(psac::GarbageCollector::memory());

    // Time the garbage collector
    auto start = std::chrono::high_resolution_clock::now();
    psac::GarbageCollector::run();
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_seconds = std::chrono::duration<double>(end - start).count();
    gc_time.push_back(elapsed_seconds);

    state.ResumeTiming();
  }

  void finalize(benchmark::State& state) {
    ts = comp.treesize();
    mem = comp.memory();
    auto start = std::chrono::high_resolution_clock::now();
    comp.destroy();
    auto end = std::chrono::high_resolution_clock::now();
    double destroy_seconds = std::chrono::duration<double>(end - start).count();
    assert(comp.root == nullptr);

    // Stats on SP tree
    state.counters["SP ts"] = ts;
    state.counters["SP mem"] = mem;
    state.counters["SP cleanup"] = destroy_seconds;

    // Stats on garbage collection for dynamic updates
    state.counters["GC mem"] = std::accumulate(std::begin(gc_mems), std::end(gc_mems), size_t(0)) / (double)gc_mems.size();
    state.counters["GC nodes"] = std::accumulate(std::begin(gc_nodes), std::end(gc_nodes), size_t(0)) / (double)gc_nodes.size();
    state.counters["GC time"] = std::accumulate(std::begin(gc_time), std::end(gc_time), 0.0) / gc_time.size();

    gc_mems.clear();
    gc_nodes.clear();
  }

  void TearDown(const ::benchmark::State&) {
  
  }

  psac::Computation comp;   // Recorded SP tree
  size_t ts;
  size_t mem;
  std::vector<size_t> gc_mems;
  std::vector<size_t> gc_nodes;
  std::vector<double> gc_time;
};


#endif  // BENCHMARKS_COMMON_HPP_

