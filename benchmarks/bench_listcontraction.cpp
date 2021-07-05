#include <algorithm>
#include <numeric>
#include <random>
#include <set>

#include <parlay/utilities.h>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/listcontraction.hpp>

const auto max_n = 1000000;
const auto seed = 196883;

#include "common.hpp"

struct StaticNodeData {
  int L, R, P;
};

using static_node_data_allocator = parlay::type_allocator<StaticNodeData>;

struct StaticListContraction {
  int n, n_rounds;
  std::vector<int> randomness;
  std::vector<int> A, D;

  std::vector<std::vector<int>> alive;
  std::vector<std::vector<StaticNodeData>> data;
 
  StaticNodeData& get_data(int i, int u) {
    return data[i][u];
  }

  int& L(int i, int u) { return get_data(i, u).L; }
  int& R(int i, int u) { return get_data(i, u).R; }
  int& P(int i, int u) { return get_data(i, u).P; }

  StaticListContraction(const std::vector<int>& a) :
    n(a.size()), n_rounds(8 * log2(n) + 16),
    randomness(n_rounds), A(n), D(n), alive(n_rounds + 1, std::vector<int>(n, true)),
    data()
  {
    assert(n > 0);
  
    // Initialise the hashtables
    data.reserve(n_rounds + 1);
    for (int r = 0, table_size = 2 * n; r < n_rounds + 1; r++, table_size = 7 * table_size / 8) {
      data.emplace_back(n);
    }
    
    // Initialise mods at round 0
    for (int i = 0; i < n; i++) {
      if (i > 0) L(0, i) = i - 1;
      else L(0, i) = -1;
      if (i < n - 1) R(0, i) = i + 1;
      else R(0, i) = -1;
      P(0, i) = 0;
      A[i] = a[i];
      alive[0][i] = true;
    }

     // Random bits for coin flips
     std::random_device rd;
     std::mt19937 gen(rd());
     std::uniform_int_distribution<> dis(0, std::numeric_limits<int>::max());
     for (int i = 0; i < n_rounds; i++) {
       randomness[i] = dis(gen);
     }
  }

  bool HEADS(int i, int u) {
    return (__builtin_popcount(randomness[i] & u) % 2 == 0);
  }

  void do_finalize(int i, int u) {
    D[u] = i;
  }

  void do_alive(int i, int u, int l, int r) {
    if (r != -1) {
      assert(L(i, r) == u);
      L(i+1, r) = u;
      P(i+1, r) = P(i, r);
    }
    else {
      R(i+1, u) = -1;
    }

    if (l != -1) {
      assert(R(i, l) == u);
      R(i+1, l) = u;
    }
    else {
      L(i+1, u) = -1;
      P(i+1, u) = P(i, u);
    }
  }

  void do_compress(int i, int u, int l, int r) {
    L(i+1, r) = l;
    if (l != -1) R(i+1, l) = r;
    P(i+1, r) = P(i, u) + A[u] + P(i, r);
    D[u] = i;
  }

  void compute_round(int i, int u, bool& lives) {
    int l = L(i, u), r = R(i, u);
    if (l != -1) { assert(alive[i][l]); }
    if (r != -1) { assert(alive[i][r]); }
    if (r != -1) {
      if (HEADS(i, u) && !HEADS(i, r)) {
        do_compress(i, u, l, r);
        lives = false;
      }
      else {
        do_alive(i, u, l, r);
        lives = true;
      }
    } else if (l == -1) {
      do_finalize(i, u);
      lives = false;
    } else {
      do_alive(i, u, l, r);
      lives = true;
    } 
  }

  void list_contraction(bool par) {
    if (par) {
      for (int r = 0; r < n_rounds; r++) {
        // Phase 2 -- compute
        parlay::parallel_for(0, (n+63)/64, [&](int c) {
          for (int j = 0; j < 64 && j + c*64 < n; j++) {
            int u = j + c*64;
            if (alive[r][u]) {
              bool lives = false;
              compute_round(r, u, lives);
              alive[r+1][u] = lives;
            }
            else {
              alive[r+1][u] = false;
            }
          }
        });
      }
    }
    else {
      for (int r = 0; r < n_rounds; r++) {
        for (int u = 0; u < n; u++) {
          if (alive[r][u]) {
            bool lives = false;
            compute_round(r, u, lives);
            alive[r+1][u] = lives;
          }
          else {
            alive[r+1][u] = false;
          }
        }
      }
    }
  }

};



// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

static void list_contraction_compute_seq(benchmark::State& state) {
  size_t n = state.range(0);
  std::vector<int> A(n);
  std::iota(std::begin(A), std::end(A), 0);
  std::shuffle(std::begin(A), std::end(A), std::default_random_engine(seed));
  StaticListContraction S(A);
  
  int max_d = 0;

  for (auto _ : state) {
    S.list_contraction(false);
  }

  state.counters["max-d"] = max_d;
}

static void list_contraction_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  size_t n = state.range(1);
  std::vector<int> A(n);
  std::iota(std::begin(A), std::end(A), 0);
  std::shuffle(std::begin(A), std::end(A), std::default_random_engine(seed));
  StaticListContraction S(A);

  for (auto _ : state) {
    S.list_contraction(true);
  }
}

PSAC_STATIC_BENCHMARK(list_contraction_compute)(benchmark::State& state) {
  size_t n = state.range(1);
  std::vector<int> A(n);
  std::iota(std::begin(A), std::end(A), 0);
  std::shuffle(std::begin(A), std::end(A), std::default_random_engine(seed));
  DynamicSequence S(A);

  for (auto _ : state) {
    S.go();
    comp = std::move(S.computation);
    record_stats(state);
  }
}

PSAC_DYNAMIC_BENCHMARK(list_contraction_update)(benchmark::State& state) {
  size_t n = state.range(1);
  size_t k = std::min(n - 1, (size_t)state.range(2));

  std::vector<int> A(n);
  std::iota(std::begin(A), std::end(A), 0);
  std::shuffle(std::begin(A), std::end(A), std::default_random_engine(seed));
  int left = 0, right = n - 1;

  DynamicSequence S(A);
  S.go();

  for (auto _ : state) {

    // Make k random changes (actually 2k since we do k splits followed by k joins)
    // Essentially, we pick k random nodes at which to split after, then we take the
    // k + 1 pieces formed and shuffle them into a random order, rejoining them
    state.PauseTiming();

    std::set<int> split_points;
    while(split_points.size() < k) {
      int cand = rand_int() % n;
      if (int r = S.getR(0, cand); r != -1 && split_points.find(cand) == split_points.end()) {
        split_points.insert(cand);
      }
    }
    std::vector<int> splits(split_points.begin(), split_points.end());

    // Find the endpoints of each piece after cutting and shuffle them
    auto find_left = [&](int r) {
      int l = r, ll = S.getL(0, r);
      while (ll != -1 && !std::binary_search(std::begin(splits), std::end(splits), ll)) {
        l = ll;
        ll = S.getL(0, ll);
      }
      return l;
    };
    std::vector<std::pair<int,int>> pieces;
    for (int sp : splits) {
      pieces.emplace_back(find_left(sp), sp);
    }
    pieces.emplace_back(find_left(right), right);
    std::shuffle(std::begin(pieces), std::end(pieces), std::default_random_engine(seed));

    // Update endpoints
    left = pieces.front().first;
    right = pieces.back().second;

    // Create update data
    std::vector<std::pair<int,int>> joins;
    assert(pieces.size() == k + 1);
    for (size_t i = 0; i < k; i++) {
      joins.emplace_back(pieces[i].second, pieces[i+1].first);
    }

    state.ResumeTiming();
    // ---------------------------------------------------------------------------

    S.batch_split(splits);
    S.batch_join(joins);
    S.update();

    record_stats(state);
  }
  finalize(state);
}

REGISTER_STATIC_BENCHMARK(list_contraction_compute);
REGISTER_DYNAMIC_BENCHMARK(list_contraction_update);

