
#include <limits>
#include <random>
#include <vector>

#include <parlay/alloc.h>
#include <parlay/hash_table.h>

#include <psac/psac.hpp>

template<typename T>
using vec = std::vector<T>;

template<typename T>
using vecvec = std::vector<std::vector<T>>;

// ----------------------------------------------------------------------------
//                               LIST CONTRACTION
// ----------------------------------------------------------------------------

// Each node has three corresponding modifiables at each round
// L = left child, R = right child, P = prefix sum
struct NodeData {
  psac::Mod<int> L, R, P;
};

int log2(int x) { return 31 - __builtin_clz(x); }

// A simple dynamic sequence (a la skip lists) based on self-adjusting
// list contraction. Supports splits, joins, and sums over ranges
struct DynamicSequence {

  const int chunk_size = 30;

  // Unsigned int aligned to 64 bytes
  struct alignas(64) PaddedInt {
    PaddedInt() = default;
    PaddedInt(int x) : val(x) { }
    int val;
    operator int() { return val; }
  };
  using bits_t = PaddedInt;

  // Non-modifiable members
  int n, n_rounds, n_chunks;
  std::vector<int> randomness;
  std::vector<PaddedInt> D;
  
  // Modifiable members
  std::vector<psac::Mod<int>> A;
  std::vector<std::vector<NodeData>> data;
  std::vector<std::vector<psac::Mod<bits_t>>> alive;

  // Computation state
  psac::Computation computation;

  // ------------------------ HASHTABLE INTERFACE ------------------------------

  // Get the data elements corresponding to node u at round i
  // The data elements must be present already
  NodeData& get_data(int i, int u) {
    return data[i][u];
  }
  
  // Direct accessors for mod pointers
  psac::Mod<int>* L(int i, int u) { return &(get_data(i, u).L); }
  psac::Mod<int>* R(int i, int u) { return &(get_data(i, u).R); }
  psac::Mod<int>* P(int i, int u) { return &(get_data(i, u).P); }

  // Return the current value of L[i][u]
  int getL(int i, int u) {
    auto& e = get_data(i, u);
    return e.L.value;
  }

  // Return the current value of R[i][u]
  int getR(int i, int u) {
    auto& e = get_data(i, u);
    return e.R.value;
  }

  // Return the current value of P[i][u]
  int getP(int i, int u) {
    auto& e = get_data(i, u);
    return e.P.value;
  }

  // Returns true if node u is alive at round i
  bool is_alive(int i, int u) {
    bool ans = (alive[i][u / chunk_size].value & (1U << (u % chunk_size))) != 0;
    return ans;
  }

  // -------------------------- SEQUENCE INTERFACE ----------------------------
  
  // Construct a dynamic sequence consisting of elements with values given in a
  // The elements of the sequence are zero-indexed based on their ORIGINAL position
  // in the sequence, i.e. splitting after element 5 means splitting after the
  // element that was originally the 5th in the sequence, not the element that
  // might currently be 5th in the sequence.
  DynamicSequence(const std::vector<int>& a)
    : n(a.size()), n_rounds(8 * log2(n) + 16), n_chunks((n+chunk_size-1)/chunk_size),
      randomness(n_rounds),
      D(n, -1), A(n),
      data(),
      alive(n_rounds+1, std::vector<psac::Mod<bits_t>>(n_chunks))
  {
    assert(n > 0);
    assert(n_chunks > 0);
    assert(n_rounds > 0);  

    // Initialise the hashtables
    data.reserve(n_rounds + 1);
    for (int r = 0, table_size = 4 * n; r < n_rounds + 1; r++, table_size = 7 * table_size / 8) {
      data.emplace_back(std::vector<NodeData>(n));
    }

    // Initialise mods at round 0
    for (int u = 0; u < n; u++) {
      auto& [L, R, P] = get_data(0, u);
      if (u > 0) psac_write(&L, u - 1);
      else psac_write(&L, -1);
      if (u < n - 1) psac_write(&R, u + 1);
      else psac_write(&R, -1);
      psac_write(&P, 0);
      psac_write(&A[u], a[u]);
    }
  
    // Initialise living status 
    for (int i = 0; i < n_chunks; i++) {
      if (n - i * chunk_size < chunk_size) psac_write(&alive[0][i], (1 << (n % chunk_size)) - 1);
      else psac_write(&alive[0][i], (1 << chunk_size) - 1);
    }

    // Generate random bits for coin flips
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, std::numeric_limits<int>::max());
    for (int i = 0; i < n_rounds; i++) {      // TODO: Parallel for loop
      randomness[i] = dis(gen);
    }
  }
  
  ~DynamicSequence() {
  
  }

  DynamicSequence(DynamicSequence&&) = default;

  void go() {
    // Run initial computation
    computation = psac_run(list_contraction);
  }

  // Update the values of the given nodes to the given values
  void batch_update(const std::vector<std::pair<int,int>>& U) {
    parlay::parallel_for(0, U.size(), [&](auto j) {
      int i = U[j].first, v = U[j].second;
      psac_write(&A[i], v);
    });
  }

  // Split the sequence immediately after each node in U
  // Each node in U must have a right neighbour
  void batch_split(const std::vector<int>& U) {
    parlay::parallel_for(0, U.size(), [&](auto j) {
      int u = U[j];
      auto r_val = getR(0, u);
      assert(r_val != -1);
      psac_write(L(0, r_val), -1);
      psac_write(R(0, u), -1);
    });
  }
  
  // Join nodes u <-> v for each pair (u,v) in U
  // Each (u,v) pair must currently have no right/left neighbour respectively
  void batch_join(const std::vector<std::pair<int,int>>& U) {
    parlay::parallel_for(0, U.size(), [&](auto j) {
      int u = U[j].first, v = U[j].second;
#ifndef DNDEBUG
      [[maybe_unused]] auto cur_r = getR(0, u);
      [[maybe_unused]] auto cur_l = getL(0, v);
      assert(cur_r == -1);
      assert(cur_l == -1);
#endif
      psac_write(R(0, u), v);
      psac_write(L(0, v), u);
    });
  }
 
  void update() {
    psac_propagate(computation);
  }

  // ------------------------------ QUERIES ---------------------------------
  
  // Returns the sum of the values on the nodes between u and v inclusive
  // u and v must be connected and u must come before v in the sequence
  int query(int i, int j) {
    assert(i >= 0 && i < n);
    assert(j >= 0 && j < n);
    int u = i, v = j;
    int answer = 0;
    while (u != v) {
      assert(u >= 0 && u < n);
      assert(v >= 0 && v < n);
      assert(D[u] != -1);
      assert(D[v] != -1);
      if (D[u] < D[v]) {
        auto d = D[u];
        auto r_val = getR(d, u);
        auto p_val = getP(d, r_val);
        answer += A[u].value + p_val;
        u = r_val;
      } else {
        auto d = D[v];
        answer += A[v].value + getP(d, v);
        v = getL(d, v);
      }
    }
    answer += A[u].value;
    return answer;
  }
  
  // ---------------------------- LIST CONTRACTION ----------------------------

  // Returns true if node u flipped heads on round i
  // For each round i, HEADS(i, u) is a collection of pairwise
  // independent random variables over the nodes u.
  bool HEADS(int i, int u) {
    return (__builtin_popcount(randomness[i] & u) % 2 == 0);
  }

  // The node u finalizes during round i
  psac_function(do_finalize, int i, int u) {
    D[u] = i;
  }

  // The node u is still alive during round i
  psac_function(do_alive, int i, int u, int l, int r) {
    assert(is_alive(i, u));
    if (r != -1) {
      assert(is_alive(i, r));
      auto& [L_r, _, P_r] = get_data(i+1, r);
      psac_write(&L_r, u);                            // L[i+1][r] = u
      psac::Mod<int>* P_next = &P_r;
      psac_read((auto p), (P(i, r)), {
        psac_write(P_next, p);                        // P[i+1][r] = P[i][r]
      });
    } else {
      psac_write(R(i+1, u), -1);                      // R[i+1][u] = -1
    }
    if (l != -1) {
      assert(is_alive(i, l));
      psac_write(R(i+1, l), u);                       // R[i+1][l] = u
    } else {
      psac_write(L(i+1, u), -1);                      // L[i+1][u] = -1
      psac_read((auto p), (P(i,u)), {
        psac_write(P(i+1, u), p);                     // P[i+1][u] = P[i][u]
      });
    }
  }

  // The node u compresses during round i
  psac_function(do_compress, int i, int u, int l, int r) {
    psac_write(L(i+1, r), l);                         // L[i+1][r] = l
    if (l != -1) psac_write(R(i+1, l), r);            // R[i+1][l] = r
    D[u] = i;
    psac_read((auto p_u, auto a, auto p_r), (P(i,u), &A[u], P(i,r)), {
      psac_write(P(i+1, r), p_u + a + p_r);           // P[i+1][r] = P[i][u] + A[u] + P[i][r]
    });
  }

  // Perform all contractions that occur during round i
  psac_function(compute_round, int i, int u, psac::Mod<bool>* contracts) {
    assert(is_alive(i, u));
    auto& [L_u, R_u, _] = get_data(i, u);
    psac_read((auto l, auto r), (&L_u, &R_u), {
      if (r != -1) {
        assert(is_alive(i, r));
        assert(getL(i, r) == u);
        if (HEADS(i, u) && !HEADS(i, r)) {
          psac_call(do_compress, i, u, l, r);
          psac_write(contracts, true);
        }
        else {
          psac_call(do_alive, i, u, l, r);
          psac_write(contracts, false);
        }
      } else if (l == -1) {
        psac_call(do_finalize, i, u);
        psac_write(contracts, true);
      } else {
        assert(is_alive(i, l));
        assert(getR(i, l) == u);
        psac_call(do_alive, i, u, l, r);
        psac_write(contracts, false);
      }
      assert(contracts->written);
    });
  }

  psac_function(compute_chunk, int i, int c) {
    int k = std::min(chunk_size, n - c * chunk_size);
    assert(alive[i][c].written);
    psac_read((unsigned int live), (&alive[i][c]), {
      if (live != 0) {
        int num_alive = __builtin_popcount(live), live_i = 0;
        psac::Mod<bool>* contracts = psac_alloc_array(bool, num_alive);
        for (int j = 0; j < k; j++) {
          int u = chunk_size * c + j;
          assert(u < n);
          // Alive at round i
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
          assert((next_live & (~live)) == 0);    // Alive at next round is a subset of alive at current round
          psac_write(&alive[i+1][c], next_live);
        });
      }
      else {
        psac_write(&alive[i+1][c], 0U);
      }
    });
  }

  // Perform list contraction
  psac_function(list_contraction) {
    for (int r = 0; r < n_rounds; r++) {
      psac_parallel_for(int c, 0, n_chunks, 4, {
        psac_call(compute_chunk, r, c);
      });
    }
  }
};


