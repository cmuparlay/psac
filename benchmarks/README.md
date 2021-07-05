
# Benchmarking Self-adjusting Computation

Thorough benchmarking of self-adjusting computations requires many steps. Broadly, we can break them down into the following:

* **Benchmarking a static sequential algorithm:** The first and simplest baseline is to benchmark a static sequential algorithm.
* **Benchmarking a parallel static algorithm:** To understand how much improvement we get from parallelizing an algorithm, we should measure a parallel equivalent of the static sequential algorithm. For the most information we should measure on many different numbers of processors, typically 1, 2, 4, 8, 16, 32, ... in powers of two.
* **Benchmarking the parallel self-adjusting algorithm initial run:** Before propagating any updates, we should measure the speed of the self-adjusting algorithm when running on the original input data. This allows us to measure how much of a slowdown the overhead of self-adjusting computation has introduced when compared to the parallel static algorithm. For comparison with the static parallel algorithm, we should also use 1, 2, 4, 8, 16, 32, ... processors.
* **Benchmarking update propagation for various classes/sizes of updates:** Finally, we should benchmark the effectiveness of change propagation. This typically involves one or more classes of updates to the input. For example, we might distinguish between adding or deleting an edge, if benchmarking a dynamic trees algorithm. We should also, if applicable to the given problem, measure the performance with respect to the size of the update. For example, we might update just a single element in the input array, or many elements. Typically, we measure update sizes in powers of ten, so we measure batches of size 1, 10, 100, 1000, 10000, up until the total input size. It is interesting to examine the point at which self-adjusting computation is outperformed by simply rerunning the static algorithm. For coarse grained algorithms, which typically suffer less overhead from self-adjusting computation, this often occurs when the update size is the size of the entire input. For finer-grained algorithms, the crossover often happens a couple of orders of magnitude before the total input size.
* **Interpreting and presenting the results:** There are multiple ways to visualize the output of the benchmarks. To help, we have a custom reporting script that takes the raw benchmark output data and constructs nice tables and plots.

Let's demonstrate a complete example. We will benchmark a very simple algorithm, **map**, which takes as input, a sequence, and writes to an output sequence, the result of applying a given function to the elements of the input sequence.

All of the benchmarks use the [Google Benchmark](https://github.com/google/benchmark) framework. Read their documentation for an overview of how the tool works and how to write basic benchmarks in it. To summarize, benchmarks are declared as static functions that take a `benchmark::State& state` parameter. Inside the benchmark function, we first perform some initialization, then the main loop that is to be measured, followed by any cleanup.

```c++
static void benchmark_something(benchmark::State& state) {
  // Initialize things here
  for (auto _ : state) {
    // Perform something to be measured here
  }
  // Cleanup here
}
```

PSAC++ includes some custom macros and structures that streamline the process of writing benchmarks for self-adjusting code, which we will demonstrate here.

## The static sequential algorithm

For the static sequential algorithm, we will just use the standard libraries `std::transform` routine, which is essentially just a map. We write the following benchmark

```c++
// Sequential baseline for map
static void map_compute_seq(benchmark::State& state) {
  auto n = state.range(0);
  std::vector<int> A(n), B(n);
  for (auto _ : state) {
    std::transform(std::begin(A), std::end(A), std::begin(B), [](int x) { return 2 * x; });
  }
}
```

In Google Benchmark, `state.range(i)` refers to the `i`th variable passed to the benchmark. In the case of static sequential benchmarks in PSAC++, this corresponds to the input size to be benchmarked.

## The parallel static algorithm

We then write the parallel version. For now, we can use `psac::_parallel_for` to perform a parallel for loop (but do not use this inside self-adjusting code! Not to be confused with psac_parallel_for, which is for self-adjusting code!!)

```c++
// Parallel static baseline for map
static void map_compute_par(benchmark::State& state) { 
  auto p = state.range(0);
  auto n = state.range(1);
  psac::set_num_workers(p);
  std::vector<int> A(n), B(n);
  for (int i = 0; i < n; i++) A[i] = i;

  for (auto _ : state) {
    auto f = [](int x) { return 2 * x; };
    psac::_parallel_for(0, n, [&](auto i) { B[i] = f(A[i]); });
  }
}
```
In a parallel static benchmark in PSAC++, the first parameter `state.range(0)` is the number of threads to use, and `state.range(1)` is the size of the input to test. We should therefore always write `psac::set_num_workers(p)` in the beginning of the parallel benchmark.


## The parallel self-adjusting computation

Next, we measure the performance of the parallel self-adjusting computation without any change propagation. A simple self-adjusting map might look like

```c++
template<typename It>
psac_function(map, It in_begin, It in_end, It out_begin, std::function<int(int)> f) {
  int n = std::distance(in_begin, in_end);
  psac_parallel_for(int i, 0, n, 1024, {
    psac_read((auto x), (in_begin + i), {
      psac_write(out_begin + i, f(x));
    });
  });
}
```

Then, to write the benchmark, we use a special macro `PSAC_STATIC_BENCHMARK`, which injects the benchmark with a custom fixture that tells it how to record various useful statistics, such as the size of the SP trees, the amount of memory used by the SP trees, and how long it took to garbage collect the SP trees.

```c++
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
    record_stats(state);  // Must call this after measuring the self-adjusting code!
  }
}
```

Note that after performing the computation to be measured, we should write `record_stats(state)`, which calls the fixture I just mentioned in order to compute the statistics. Also, note that we did not have to manually set the number of worker threads this time since it is taken care of by the custom fixture.

Lastly, it is important that we named our benchmarks consistently. Note that all three begin with `map_compute_` and are followed by `seq`, `par`, and `psac` respectively. This must be the case. Once we have written these three, we must *register* the benchmarks with the `REGISTER_STATIC_BENCHMARK` macro, like so

```c++
REGISTER_STATIC_BENCHMARK(map_compute);
```

The `REGISTER_STATIC_BENCHMARK` macro takes as input, the prefix of the benchmark names, which must each be followed by `seq`, `par`, and `psac` respectively.

## Benchmarking parallel change propagation

Benchmarking the performance of updates is the hardest part. In particular, it can be challenging, in part, due to the requirement of having to generate good random updates to the input. For some problems, like modifying random elements of an array, this is not so bad, but for others, like linking and cutting random edges in a tree while keeping the tree connected and free of cycles is much harder!

To create a benchmark for parallel dynamic updates, we use the `PSAC_DYNAMIC_BENCHMARK` macro. The test comes with three parameters, `state.range(0)`, which is the number of processors (handled for us, so this can be ignored), `state.range(1)`, which is the input size, and `state.range(2)` which is the size of the update batches.

Here is a complete example benchmark for map.

```c++
// ----- Random subset generator for tests -----
std::vector<int> perm = []() {
  std::vector<int> p(max_n);
  std::iota(std::begin(p), std::end(p), 0);
  return p;
}();

std::vector<int> random_k(size_t n, size_t k, size_t seed) {
  pbbs::random rng(seed);
  for (int i = 0; i < k; i++) {
    int j = i + (rng.ith_rand(i) % (n - i));
    std::swap(perm[i], perm[j]);
  }
  return std::vector(perm.begin(), perm.begin() + k);
}

// ---- Parallel self-adjusting benchmark for dynamic map ----
PSAC_DYNAMIC_BENCHMARK(map_update)(benchmark::State& state) { 
  
  size_t n = state.range(1);
  size_t k = state.range(2);
  std::vector<psac::Mod<int>> A(n), B(n);
  for (size_t i = 0; i < n; i++) {
    psac_write(&A[i], i);
  }

  comp = psac_run(map, std::begin(A), std::end(A), std::begin(B), [](int x) { return 2*x; });

  size_t random_seed = 0;
  for (auto _ : state) {
    // Make random changes
    state.PauseTiming();
    auto change_indices = random_k(n, k, random_seed++);
    std::vector<std::pair<int,int>> updates;
    for (auto i : change_indices) {
      updates.emplace_back(i, rand_int() % (1 << 30));
    }
    state.ResumeTiming();

    // Write updates
    psac::_parallel_for(0, k, [&](auto i) {
      psac_write(&A[updates[i].first], updates[i].second);
    });

    // Propagate changes
    psac_propagate(comp);
    record_stats(state);
  }

  finalize(state);
}

REGISTER_DYNAMIC_BENCHMARK(map_update);
```

Note some key differences here between the static version:

* We run the initial self-adjusting computation outside the measured loop, then perform updates only inside the loop, since this is what we want to measure.
* Since we do not want our update time to also contain the time required to *generate* the random updates, we use `state.PauseTiming()` and `state.ResumeTiming()` around the code where we generate the updates. This ensures that we do not measure this part in the running time of our algorithm.
* We conclude the benchmark with `finalize(state);`, which sums up the total statistics for the SP tree sizes and memory, and the amount of stuff in the garbage collector.
* We generate random subsets of the indices to create random updates. To do so efficiently, we maintain a permutation `perm` of the numbers `0` through `max_n - 1`, where `max_n` is the largest value of `n` that the benchmark might use. To select `k` random distinct elements, we essentially perform the first `k` steps of a [Knuth Shuffle](https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle) on the `perm` vector, then return the first `k` elements. This is much more efficient than shuffling the entire vector when `k` is small. We use the random number generator from pbbslib, which should be updated to ParlayLib when I get the chance.
* At the end, we register the benchmark using the `REGISTER_DYNAMIC_BENCHMARK` macro.

## Running the benchmarks

To run the benchmarks, make sure that you compile in *Release* mode so that optimizations are turned on. This can be achieved by ensuring that you add `-DCMAKE_BUILD_TYPE=Release` to your CMake configuration command when you configure the build. If you have written a new benchmark, you will need to add it to `benchmarks/CMakeLists.txt` for it to be registered and compiled with the build. Once you have compiled the benchmark, you can run it, and see some output like this. Note that for high-quality data, the benchmarks should be ran on a machine with lots of processors (32 or more is good), with no other executables running in the background. This example shows my personal computer with a mere 6 cores, an plenty of stuff running, so the results are not as good and are much more noisy!

![Throughput and speedup plots](https://danielanderson.net/images/parallel-sac/psac_map.png)

You might want to open it in a new tab and zoom in.

## Generating reports

The benchmarks output a lot of raw data, but we can get a better handle on everything by generating some nice plots and tables.  In summary, these can be generated by running the target `reports`, e.g.

```
cmake --build . --target reports
```

**WARNING: This target will run every configuration of every benchmark 10 times to get a complete picture of the performance. This means it might take a while on a machine with many processors... You probably want to leave it overnight**

You can also just generate reports for a single benchmark, rather than all of them, with the target `report-benchmark_name`. If you wrote a new benchmark, you will need to register it and add it to `reports/CMakeLists.txt` for it to be included. Alternatively, for something slightly quicker, you can use the report generation script `reports/make_report.py` manually, in which case you can supply it one or more runs of the benchmarks rather than 10, and you can tweak the settings. This script takes as input, the Google Benchmark output in JSON format. See the script's help output (`make_report.py --help`), and Google Benchmark's documentation for more information on outputting benchmark data and feeding it to the reporting script.

The reports will generate six plots, including:
* Absolute throughput of the static computations vs. the number of processors
* Relative speedup of the static computations relative to the sequential baseline vs. the number of processors
* Absolute throughput of dynamic updates vs. the batch sizes
* Relative speedup of dynamic updates relative to running the sequential baseline from scratch vs. the batch size
* Absolute throughput of dynamic updates vs. the number of processors
* Relative speedup of dynamic updates relative to running the sequential baseline from scratch vs. the number of processors

Two examples are shown below, the absolute throughput of the dynamic updates vs. the batch size, and the relative speedup of the dynamic updates vs. the number of processors. These are from a 32 core, hyperthreaded machine, though they are from a different benchmark than the one constructed above.


![Throughput and speedup plots](https://danielanderson.net/images/parallel-sac/psac_plots.png)

In addition to the plots, the script will also arrange the timings in a table, and compute the speedups.

![Throughput and speedup plots](https://danielanderson.net/images/parallel-sac/psac_table.png)

Here, SU denotes the speedup of the parallel algorithm on 32 threads with hyperthreading relative to the sequential equivalent. WS denotes the *work savings*, which is the speedup of running the self-adjusting dynamic update versus running the sequential static algorithm from scratch. Total denotes the total speedup, which is equivalent to the product of the speedup and the work savings.