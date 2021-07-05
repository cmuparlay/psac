// Tree contraction implementation

#ifndef PSAC_EXAMPLES_TREECONTRACTION_HPP_
#define PSAC_EXAMPLES_TREECONTRACTION_HPP_

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include <parlay/utilities.h>

#include <psac/psac.hpp>

template<typename T>
using vec = std::vector<T>;

template<typename T>
using vecvec = std::vector<std::vector<T>>;

using vmi = std::vector<psac::Mod<int>>;
using vvmi = std::vector<std::vector<psac::Mod<int>>>;

// ----------------------------------------------------------------------------
//                               TREE CONTRACTION
// ----------------------------------------------------------------------------

template<int t>
struct alignas(128) NodeData {
  psac::Mod<std::pair<int,int>> P;          // Stores (parent, pos)
  //psac::Mod<double> p_max;                  // stores path aggregate
  //psac::Mod<double> p_sum;
  //psac::Mod<double> node_sum;
  std::array<psac::Mod<int>, t> C;          // Stores list of children
  psac::Mod<bool> leaf_status;              // true if the node is a leaf
};

int log2(int x) { return 31 - __builtin_clz(x); }

template<int t>
struct DynamicTree {

  const int chunk_size = 30;

  struct alignas(64) PaddedInt {
    PaddedInt() = default;
    PaddedInt(unsigned int x) : val(x) { }
    unsigned int val;
    operator unsigned int() { return val; }
  };

  using bits_t = PaddedInt;

  // Non-modifiable members
  int n, n_chunks, n_rounds;
  std::vector<int> randomness;
  std::vector<PaddedInt> D;

  // Modifiable members
  std::vector<std::vector<NodeData<t>>> data;
  std::vector<std::vector<psac::Mod<bits_t>>> alive;

  // Computation state
  psac::Computation computation;
  
  // ------------------------ HASHTABLE INTERFACE ------------------------------

  // Get the data elements corresponding to node u at round i
  // The data elements must be present already
  NodeData<t>& get_data(int i, int u) {
    return data[i][u];
  }
  
  // Direct accessors for mod pointers
  psac::Mod<std::pair<int,int>>* P(int i, int u) { return &(get_data(i, u).P); }
  //psac::Mod<double>* P_max(int i, int u) { return &(get_data(i, u).p_max); }
  //psac::Mod<double>* P_sum(int i, int u) { return &(get_data(i, u).p_sum); }
  //psac::Mod<double>* node_sum(int i, int u) { return &(get_data(i, u).node_sum); }
  std::array<psac::Mod<int>, t>& C(int i, int u) { return get_data(i, u).C; }
  psac::Mod<int>* C(int i, int u, int j) { return &(get_data(i, u).C[j]); }
  psac::Mod<bool>* leaf_status(int i, int u) { return &(get_data(i, u).leaf_status); }
 
  int getP(int i, int u) { return P(i, u)->value.first; }

  std::vector<int> getC(int i, int u) {
    std::vector<int> c;
    for (int j = 0; j < t; j++) {
      int val = C(i, u, j)->value;
      c.push_back(val);
    }
    return c;
  }

  int degree(int i, int u) {
    int degree = 0;
    for (int j = 0; j < t; j++) {
      if (C(i, u, j)->value == -1)
        degree++;
    }
    return degree;
  }

  // ------------------------------ INTERFACE ---------------------------------
  
  DynamicTree(int _n, const std::vector<std::vector<int>>& adj)
    : n(_n), n_chunks((n+chunk_size-1)/chunk_size), n_rounds(4 * log2(n) + 16),
    randomness(n_rounds),
    D(n),
    data(),
    alive(n_rounds + 1, std::vector<psac::Mod<bits_t>>(n_chunks))
  {
    assert(n > 0);
    assert(n_chunks > 0);
    assert(n_rounds > 0);

    // Initialise the hashtables
    data.reserve(n_rounds + 1);
    for (int r = 0, table_size = 4 * n; r < n_rounds + 1; r++, table_size = 3 * table_size / 4) {
      data.emplace_back(n);
    }


    // Initialise mods at round 0
    for (int v = 0; v < n; v++) {
      psac_write(P(0, v), std::make_pair(v,0));
      //psac_write(P_max(0, v), 0);
      //psac_write(P_sum(0, v), 0);
      //psac_write(node_sum(0, v), 1.0 * v);
    }

    for (int v = 0; v < n; v++) {
      for (size_t j = 0; j < adj[v].size(); j++) {
        int u = adj[v][j];
        psac_write(P(0, u), std::make_pair(v, j));
        //psac_write(P_max(0, u), 1.0 * (u - v));
      }
    }

    for (int v = 0; v < n; v++) { 
      assert(adj[v].size() <= t);
      for (size_t j = 0; j < adj[v].size(); j++) {
        psac_write(C(0,v,j), adj[v][j]);
      }
      for (size_t j = adj[v].size(); j < t; j++) {
        psac_write(C(0,v,j), -1);
      }
      psac_write(leaf_status(0, v), adj[v].empty());
    }
    
    for (int i = 0; i < n_chunks; i++) {
      if (n - i * chunk_size < chunk_size) psac_write(&alive[0][i], (1 << (n % chunk_size)) - 1);
      else psac_write(&alive[0][i], (1 << chunk_size) - 1);
    }

    // Generate random bits for coin flips
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, std::numeric_limits<int>::max());
    for (int i = 0; i < n_rounds; i++) {
      randomness[i] = dis(gen);
    }
  }
 
  void go() { 
    // Run initial computation
    computation = psac_run(tree_contraction);
  }

  ~DynamicTree() {

  }

  // ------------------------------ QUERIES ---------------------------------

  /*
  // Return the heaviest weight on the path from u to the root
  double path_max(int u) {
    double ans = 0.0;
    while (P(D[u], u)->value.first != u) {
      ans = std::max(ans, P_max(D[u], u)->value);
      u = P(D[u], u)->value.first;
    }
    return ans;
  }
  
  // Return the sum of the vertex weights in the subtree of u
  double subtree_sum(int u) {
    double ans = node_sum(D[u], u)->value;
    for (int j = 0; j < t; j++) {
      auto c = C(D[u], u, j)->value;
      if (c != -1) {
        auto edge_val = P_sum(D[u], c)->value;
        ans += edge_val;
        return ans + subtree_sum(c);
      }
    }
    return ans;
  }
  */

  // Return the root of the component containing u
  int find_rep(int u) {
    while (P(D[u], u)->value.first != u) {
      u = P(D[u], u)->value.first;
    }
    return u;
  } 
 
  // Remove the given set of edges from the tree. The edges must be
  // of the form (parent, child)
  void batch_cut(const std::vector<std::pair<int,int>>& U) {
    parlay::parallel_for(0, U.size(), [&](auto j) {
      auto [p, v] = U[j];
      assert(getP(0, v) == p);
      auto pos = P(0, v)->value.second;
      assert(getC(0, p)[pos] == v);
      psac_write(C(0, p, pos), -1);
      psac_write(P(0, v), std::make_pair(v, 0));
    });
    parlay::parallel_for(0, U.size(), [&](auto j) {
      auto p = U[j].first;
      if (degree(0, p) == 0) psac_write(leaf_status(0, p), true);
    });
  }

  // Insert the given set of edges into the tree. The edges
  // must be of the form (parent, child) and must not form
  // a cycle. The input format is a vector of
  // {parent, [list of new children]}
  void batch_link(const std::vector<std::pair<int,std::vector<int>>>& U) {
    parlay::parallel_for(0, U.size(), [&](auto j) {
      const auto& [p, new_c] = U[j];
      for (auto v : new_c) {
        assert(getP(0, v) == v);
        bool found_slot = false; 
        for (int i = 0; i < t; i++) {
          if (C(0,p,i)->value == -1) {
            psac_write(C(0,p,i), v);
            psac_write(P(0,v), std::make_pair(p, i));
            psac_write(leaf_status(0, p), false);
            found_slot = true;
            break;
          }
        }
        assert(found_slot);
      }
    }); 
  }

  void update() {
    psac_propagate(computation);
  }

  // ----------------------------- TREE CONTRACTION ---------------------------
  
  // Returns true if vertex u flipped heads on round i
  // For each round i, HEADS(i, u) is a collection of pairwise
  // independent random variables over the nodes u.
  bool HEADS(int i, int u) {
    return (__builtin_popcount(randomness[i] & u) % 2 == 0);
  }
  
  // Given a sequence of children ids c, if only one is not 
  // null (not -1), returns that element, else returns -1.
  int only_child(const std::vector<int>& c) {
    if (std::count(std::begin(c), std::end(c), -1) == t - 1)
      return *std::find_if(std::begin(c), std::end(c), [](auto x) { return x != -1; });
    else
      return -1;
  }
  
  // Vertex u remains alive in round i
  psac_function(do_alive, int i, int u, const std::pair<int,int>& p, const std::vector<int>& c) {
    if (p.first == u) {
      psac_write(P(i+1, u), p);
      //assert(P_max(i, u)->written);
      //assert(P_sum(i, u)->written);
      //psac_read((auto p_max, auto p_sum), (P_max(i, u), P_sum(i, u)), {
      //  psac_write(P_max(i+1, u), p_max);
      //  psac_write(P_sum(i+1, u), p_sum);
      //});
    }
    else {
      psac_write(C(i+1, p.first, p.second), u);
    }
    psac_dynamic_context({
      bool leaf = true;
      //double sum = psac_dynamic_read(this->node_sum(i, u));
      for (int j = 0; j < t; j++) {
        if (c[j] != -1) {
          assert(leaf_status(i, c[j])->written);
          //assert(P_max(i, c[j])->written);
          //assert(P_sum(i, c[j])->written);
          auto c_leaf = psac_dynamic_read(this->leaf_status(i, c[j]));
          
          //auto child_val = psac_dynamic_read(this->node_sum(i, c[j]));
          //auto edge_max = psac_dynamic_read(this->P_max(i, c[j]));
          //auto edge_val = psac_dynamic_read(this->P_sum(i, c[j]));
          
          //if (c_leaf) {
          //  sum += child_val + edge_val;
          //}
          
          psac_write(P(i+1, c[j]), std::make_pair(u, j));
          //psac_write(P_max(i+1, c[j]), edge_max);
          //psac_write(P_sum(i+1, c[j]), edge_val);

          leaf = leaf && c_leaf;
        }
        else {
          psac_write(C(i+1, u, j), -1);
        }
      }
      psac_write(leaf_status(i+1, u), leaf);
      //psac_write(node_sum(i+1, u), sum);
    });
  }
 
  // Vertex u compresses in round i
  psac_function(do_compress, int i, int u, const std::pair<int,int>& p, int v) {
    //assert(P_max(i, u)->written);
    //assert(P_sum(i, v)->written);
    //psac_read((auto u_max, auto u_sum, auto v_max, auto v_sum, auto u_val), (P_max(i, u), P_sum(i, u), P_max(i, v), P_sum(i, v), node_sum(i, u)), {
    //  psac_write(P_max(i+1, v), std::max(u_max, v_max));
    //  psac_write(P_sum(i+1, v), u_sum + v_sum + u_val);
    //});
    psac_write(P(i+1, v), p);
    psac_write(C(i+1, p.first, p.second), v);
    D[u] = i;
  }

  // Vertex u finalizes in round i
  psac_function(do_finalize, int i, int u) {
    D[u] = i;
  }
  
  // Vertex u rakes in round i
  psac_function(do_rake, int i, int u, const std::pair<int,int>& p) {
    psac_write(C(i+1, p.first, p.second), -1);
    D[u] = i;
  }
  
  psac_function(compute_chunk, int i, int c) {
    int k = std::min(chunk_size, n - c * chunk_size);
    assert(alive[i][c].written);
    psac_read((unsigned int live), (&alive[i][c]), {
      if (live != 0) {
        int num_alive = __builtin_popcount(live), live_i = 0;
        psac::Mod<bool>* contracts = psac_alloc_array(bool, num_alive);
        for (int j = 0; j < k; j++ ) {
          int u = chunk_size * c + j;
          assert(u < n);
          if (live & (1U << j)) {
            psac_call(compute_round, i, u, &contracts[live_i++]);
          }
        }
        assert(live_i == num_alive);
        psac_read_array(auto dies, std::make_pair(contracts, contracts + num_alive), {
          unsigned int next_live = 0;
          for (int j = 0, alive_index = 0; j < k; j++) {
            if ((live & (1U << j)) && dies[alive_index++] == false) {
              next_live |= (1U << j);
            }
          }
          assert((next_live & (~live)) == 0);
          psac_write(&alive[i+1][c], next_live);
        });
      }
      else {
        psac_write(&alive[i+1][c], 0U);
      }
    });
  }

  // Compute the result of vertex u in round i. Either u will contract
  // via compression, rake, or finalization, or it will remain alive.
  psac_function(compute_round, int i, int u, psac::Mod<bool>* contracts) {
    assert(P(i,u)->written);
    assert(leaf_status(i, u)->written);
    psac_read((auto p, auto is_leaf), (P(i, u), leaf_status(i, u)), {
      if (is_leaf && p.first != u) {
        psac_call(do_rake, i, u, p);
        psac_write(contracts, true);
      }
      else if (is_leaf && p.first == u) {
        psac_call(do_finalize, i, u);
        psac_write(contracts, true);
      }
      else {
        psac_read_array(const auto& c, std::make_pair(std::begin(C(i, u)), std::end(C(i, u))), {
          if (int v = only_child(c); p.first != u && v != -1) {
            assert(leaf_status(i, v)->written);
            psac_read((auto v_leaf), (leaf_status(i, v)), {
              if (!v_leaf && HEADS(i, u) && !HEADS(i, p.first)) {
                psac_call(do_compress, i, u, p, v);
                psac_write(contracts, true);
              }
              else {
                psac_call(do_alive, i, u, p, c);
                psac_write(contracts, false);
              }
            });
          }
          else {
            psac_call(do_alive, i, u, p, c);
            psac_write(contracts, false);
          }
        });
      }
    });
  }
  
  // Perform tree contraction
  psac_function(tree_contraction) {
    for (int r = 0; r < n_rounds; r++) {
      psac_parallel_for(int c, 0, n_chunks, 4, {
        psac_call(compute_chunk, r, c);
      });
    }
  }
  
};

#endif  // PSAC_EXAMPLES_TREECONTRACTION_HPP_

