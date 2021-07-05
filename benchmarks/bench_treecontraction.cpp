#include <algorithm>
#include <functional>
#include <numeric>
#include <random>
#include <set>
#include <queue>

#include <parlay/utilities.h>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/treecontraction.hpp>

const auto max_n = 1000000;
const auto seed = 196883;
#include "common.hpp"

constexpr int degree = 2;

// Permutation used for picking random elements
std::vector<int> perm = []() {
  std::vector<int> perm(max_n);
  std::iota(std::begin(perm), std::end(perm), 0);
  std::shuffle(std::begin(perm), std::end(perm), std::default_random_engine(seed));
  return perm;
}();

template<int t>
struct alignas(64) StaticNodeData {
  std::pair<int,int> P;
  int p_val;
  std::array<int, t> C;
  bool leaf_status;
};

template<int t>
struct StaticTreeContraction {

  // Non-modifiable members
  int n, n_rounds;
  std::vector<int> randomness;
  std::vector<typename DynamicTree<t>::PaddedInt> D;

  // Modifiable members
  std::vector<std::vector<StaticNodeData<t>>> data;
  std::vector<std::vector<bool>> alive;

  StaticNodeData<t>& get_data(int i, int u) {
    return data[i][u];
  }

  std::pair<int,int>& P(int i, int u) { return get_data(i, u).P; }
  int& P_val(int i, int u) { return get_data(i, u).p_val; }
  std::array<int ,t>& C(int i, int u) { return get_data(i, u).C; }
  int& C(int i, int u, int j) { return get_data(i, u).C[j]; }
  bool& leaf_status(int i, int u) { return get_data(i, u).leaf_status; }

  StaticTreeContraction(int _n, const std::vector<std::vector<int>>& adj)
      : n(_n), n_rounds(8 * log2(n) + 16), randomness(n_rounds),
        D(n),
        data(),
        alive(n_rounds + 1, std::vector<bool>(n))
  {
    assert(n > 0);
    assert(n_rounds > 0);

    data.reserve(n_rounds + 1);
    for (int r = 0, table_size = 4 * n; r < n_rounds + 1; r++, table_size = 3 * table_size / 4) {
      data.emplace_back(n);
    }

    for (int v = 0; v < n; v++) {
      P(0, v) = {v, 0};
      P_val(0, v) = 0.0;
      alive[0][v] = true;
    }

    for (int v = 0; v < n; v++) {
      for (size_t j = 0; j < adj[v].size(); j++) {
        int u = adj[v][j];
        P(0, u) = {v, j};
        P_val(0, u) = 1;
      }
    }

    for (int v = 0; v < n; v++) {
      assert(adj[v].size() <= t);
      for (size_t j = 0; j < adj[v].size(); j++) {
        C(0,v,j) = adj[v][j];
      }
      for (size_t j = adj[v].size(); j < t; j++) {
        C(0,v,j) = -1;
      }
      leaf_status(0, v) = adj[v].empty();
    }

    // Generate random bits for coin flips
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, std::numeric_limits<int>::max());
    for (int i = 0; i < n_rounds; i++) {
      randomness[i] = dis(gen);
    }
  }

  // Returns true if vertex u flipped heads on round i
  // For each round i, HEADS(i, u) is a collection of pairwise
  // independent random variables over the nodes u.
  bool HEADS(int i, int u) {
    return (__builtin_popcount(randomness[i] & u) % 2 == 0);
  }

  // Given a sequence of children ids c, if only one is not
  // null (not -1), returns that element, else returns -1.
  int only_child(const std::array<int, t>& c) {
    int cid = -1;
    for (size_t i = 0; i < t; i++) {
      if (c[i] != -1 && cid == -1)
        cid = c[i];
      else if (c[i] != -1 && cid != -1)
        return -1;
    }
    return cid;
  }

  void do_alive(int i, int u, const std::pair<int,int>& p, const std::array<int,t>& c) {
    if (p.first == u) {
      P(i+1, u) = p;
      P_val(i+1, u) = P_val(i, u);
    }
    else {
      C(i+1, p.first, p.second) = u;
    }

    bool leaf = true;
    for (int j = 0; j < t; j++) {
      if (c[j] != -1) {
        auto c_leaf = leaf_status(i, c[j]);
        auto c_val = P_val(i, c[j]);
        P(i+1, c[j]) = {u, j};
        P_val(i+1, c[j]) = c_val;
        leaf = leaf && c_leaf;
      }
      else {
        C(i+1, u, j) = -1;
      }
    }
    leaf_status(i+1, u) = leaf;
  }

  void do_compress(int i, int u, const std::pair<int,int>& p, int v) {
    P_val(i+1, v) = std::max(P_val(i, u), P_val(i, v));
    P(i+1, v) = p;
    C(i+1, p.first, p.second) = v;
    D[u] = i;
  }

  void do_finalize(int i, int u) {
    D[u] = i;
  }

  void do_rake(int i, int u, const std::pair<int,int>& p) {
    C(i+1, p.first, p.second) = -1;
    D[u] = i;
  }

  void compute_round(int i, int u, bool& contracts) {
    auto p = P(i, u);
    bool is_leaf = leaf_status(i, u);

    if (is_leaf && p.first != u) {
      do_rake(i, u, p);
      contracts = true;
    }
    else if (is_leaf && p.first == u) {
      do_finalize(i, u);
      contracts = true;
    }
    else {
      const auto& c = C(i, u);
      if (int v = only_child(c); p.first != u && v != -1) {
        bool v_leaf = leaf_status(i, v);
        if (!v_leaf && HEADS(i, u) && !HEADS(i, p.first)) {
          do_compress(i, u, p, v);
          contracts = true;
        }
        else {
          do_alive(i, u, p, c);
          contracts = false;
        }
      }
      else {
        do_alive(i, u, p, c);
        contracts = false;
      }
    }

  }

  void tree_contraction(bool parallel) {
    if (parallel) {
      for (int r = 0; r < n_rounds; r++) {
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
            bool contracts;
            compute_round(r, u, contracts);
            alive[r+1][u] = contracts;
          }
          else {
            alive[r+1][u] = false;
          }
        }
      }
    }
  }
};

// Make a random rooted tree on n vertices where each vertex has
// maximum number of children <= t
std::pair<int, std::vector<std::vector<int>>> random_tree(size_t n, size_t t, size_t seed = 0) {
  std::vector<int> vertices(n);
  std::iota(std::begin(vertices), std::end(vertices), 0);
  std::shuffle(std::begin(vertices), std::end(vertices), std::default_random_engine(seed));
  std::vector<std::vector<int>> adj(n);
  int root = vertices[0];
  parlay::random rng(seed);
  for (size_t i = 1; i < n; i++) {
    int u = vertices[i];
    int parent = vertices[rng.ith_rand(i) % i];
    while (adj[parent].size() >= t) {
      parent = vertices[rng.ith_rand(i + ++seed) % i];
    }
    adj[parent].push_back(u);
  }
  return std::make_pair(root, adj);
}

std::vector<int> to_parents(const std::vector<std::vector<int>>& adj) {
  std::vector<int> p(adj.size());
  std::iota(std::begin(p), std::end(p), 0);
  for (size_t i = 0; i < adj.size(); i++) {
    for (auto c : adj[i]) {
      p[c] = i;
    }
  }
  return p;
}
static void tree_contraction_compute_seq(benchmark::State& state) {
  size_t n = state.range(0);

  size_t rand_seed = 0;
  for (auto _ : state) {
    state.PauseTiming();
    auto T = random_tree(n, degree, rand_seed++).second;
    auto tc = std::make_unique<StaticTreeContraction<degree>>(n, T);
    state.ResumeTiming();

    tc->tree_contraction(false);

    state.PauseTiming();
    T.clear();
    tc.reset();
    state.ResumeTiming();
  }

}

static void tree_contraction_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  size_t n = state.range(1);

  size_t rand_seed = 0;
  for (auto _ : state) {
    state.PauseTiming();
    auto T = random_tree(n, degree, rand_seed++).second;
    auto tc = std::make_unique<StaticTreeContraction<degree>>(n, T);
    state.ResumeTiming();

    tc->tree_contraction(true);

    state.PauseTiming();
    T.clear();
    tc.reset();
    state.ResumeTiming();
  }
}

PSAC_STATIC_BENCHMARK(tree_contraction_compute)(benchmark::State& state) {
  size_t n = state.range(1);

  size_t rand_seed = 0;
  for (auto _ : state) {
    state.PauseTiming();
    auto T = random_tree(n, degree, rand_seed++).second;
    auto dt = std::make_unique<DynamicTree<degree>>(n, T);
    state.ResumeTiming();

    dt->go();
    comp = std::move(dt->computation);
    record_stats(state);

    state.PauseTiming();
    T.clear();
    dt.reset();
    state.ResumeTiming();
  }
}

PSAC_DYNAMIC_BENCHMARK(tree_contraction_update)(benchmark::State& state) {
  size_t n = state.range(1);
  size_t k = std::min(n - 1, (size_t)state.range(2));

  auto [root, T] = random_tree(n, degree, 0);

  DynamicTree<degree> dt(n, T);
  dt.go();

  size_t rand_seed = 0;
  for (auto _ : state) {
    // Generate k random updates (k cuts and k links)
    state.PauseTiming();
    // Generate k random cuts
    parlay::random rng(rand_seed++);
    std::vector<int> roots;
    for (int i = 0; i < k; i++) {
      int j = i + (rng.ith_rand(i) % (n - i));
      while (perm[j] == root) j = i + (rng.ith_rand(i + rand_seed++) % (n - i));
      std::swap(perm[i], perm[j]);
      assert(dt.getP(0, perm[i]) != perm[i]);
      roots.push_back(perm[i]);
    }

    std::vector<std::pair<int,int>> cuts;
    cuts.reserve(k);
    for (auto r : roots) {
      assert(dt.getP(0, r) != r);
      cuts.emplace_back(dt.getP(0, r), r);
    }

    // Generate k random links
    std::vector<std::pair<int,int>> links;

    roots.push_back(root);
    std::shuffle(std::begin(roots), std::end(roots), std::default_random_engine(seed));
    std::vector<int> is_root(n, false);
    for (int r : roots) is_root[r] = true;
    std::priority_queue<std::tuple<int,int,int>> pq;

    // Compute the degree of u after making the cuts
    auto get_degree = [&](int u) {
      auto c = dt.getC(0, u);
      return std::count_if(std::begin(c), std::end(c), [&](int v) { return v != -1 && !is_root[v]; });
    };

    std::function<void(int)> add_to_queue = [&](int r) {
      int d = get_degree(r);
      if (d < degree) {
        pq.emplace(rng.ith_rand(r), r, d);
      }
      for (auto c : dt.getC(0, r)) {
        if (c != -1 && !is_root[c]) {
          add_to_queue(c);
        }
      }
    };

    root = roots[0];  // set new root
    for (auto r : roots) {
      if (!pq.empty()) {
        auto [_, v, d] = pq.top(); pq.pop();
        links.emplace_back(v, r);
        if (d + 1 < degree) pq.emplace(rng.ith_rand(v + d), v, d + 1);
      }
      add_to_queue(r);
    }

    // Semisort links
    std::sort(std::begin(links), std::end(links));
    std::vector<std::pair<int, std::vector<int>>> c_links;
    for (const auto& link : links) {
      if (c_links.empty() || c_links.back().first != link.first) {
        c_links.emplace_back(link.first, std::vector<int>{});
      }
      c_links.back().second.push_back(link.second);
    }
    state.ResumeTiming();
    // ---------------------------------------------------------------------------
    dt.batch_cut(cuts);
    dt.batch_link(c_links);
    dt.update();

    record_stats(state);
  }

  comp = std::move(dt.computation);
  finalize(state);
}

REGISTER_STATIC_BENCHMARK(tree_contraction_compute);
REGISTER_DYNAMIC_BENCHMARK(tree_contraction_update);

