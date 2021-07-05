#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>

#include <psac/examples/treecontraction.hpp>

// Random number generator for tests
std::mt19937 gen(0);
std::uniform_int_distribution<int> dis(0, std::numeric_limits<int>::max());
int rand_int() { return dis(gen); }

// Make a random rooted tree on n vertices where each vertex has
// maximum number of children <= t
std::vector<std::vector<int>> random_tree(size_t n, size_t t) {
  std::vector<std::vector<int>> adj(n);
  for (size_t u = 1; u < n; u++) {
    size_t parent = rand_int() % u;
    while (adj[parent].size() >= t) {
      parent = rand_int() % u;
    }
    adj[parent].push_back(u);
  }
  return adj;
}

// Make a random rooted forest on n vertices where each vertex
// has maximum number of children <= t, and in expectation has
// c connected components.
std::vector<std::vector<int>> random_forest(size_t n, size_t t, size_t c) {
  std::vector<std::vector<int>> adj(n);
  for (size_t u = 1; u < n; u++) {
    if (rand_int() % (n/c) != 0) {
      size_t parent = rand_int() % u;
      while (adj[parent].size() >= t) {
        parent = rand_int() % u;
      }
      adj[parent].push_back(u);
    }
  }
  return adj;
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

// Given the parent relationship p, find the representative
// (root of its component) of the vertex u
int find_rep(const std::vector<int>& p, int u) {
  while (p[u] != u) {
    u = p[u];
  }
  return u;
}

double subtree_sum(std::vector<std::vector<int>>& adj, int u) {
  double ans = 1.0 * u;
  for (int v : adj[u]) {
    ans += subtree_sum(adj, v);
  }
  return ans;
}

double path_max(std::vector<int>& p, int u) {
  if (p[u] != u) {
    double weight = 1.0 * (u - p[u]);
    return std::max(weight, path_max(p, p[u]));
  }
  return 0.0;
}

// ----------------------------------------------------------------------------
//                              Small tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestTreeSmall);

// Test that the list contraction algorithm produces a computation
TEST_P(TestTreeSmall, TestConstruction) {
  auto adj = random_tree(16, 3);
  auto T = DynamicTree<3>(16, adj);
  T.go();
  ASSERT_NE(T.computation.root, nullptr);
}

// Test that queries are correct for random forests
TEST_P(TestTreeSmall, TestQuery) {
  auto adj = random_forest(16, 3, 3);
  DynamicTree<3> T(16, adj);
  T.go();
  auto p = to_parents(adj);
  for (int u = 0; u < 16; u++) {
    ASSERT_EQ(find_rep(p, u), T.find_rep(u));
  }
}

TEST_P(TestTreeSmall, TestUpdate) {
  std::vector<std::vector<int>> adj = {
    {1, 2},
    {},
    {}
  };

  DynamicTree<3> T(3, adj);
  T.go();
  
  ASSERT_EQ(T.find_rep(0), 0);
  ASSERT_EQ(T.find_rep(1), 0);
  ASSERT_EQ(T.find_rep(2), 0);

  T.batch_cut({{0,1}});
  T.update();

  ASSERT_EQ(T.find_rep(0), 0);
  ASSERT_EQ(T.find_rep(1), 1);
  ASSERT_EQ(T.find_rep(2), 0);

  T.batch_link({{1,{0}}});
  T.update();

  ASSERT_EQ(T.find_rep(0), 1);
  ASSERT_EQ(T.find_rep(1), 1);
  ASSERT_EQ(T.find_rep(2), 1);

  psac::GarbageCollector::run();
}

/*
TEST_P(TestTreeSmall, TestSubtreeSum) {
  auto adj = random_forest(16, 3, 3);
  DynamicTree<3> T(16, adj);
  T.go();
  auto p = to_parents(adj);
  for (int u = 0; u < 16; u++) {
    ASSERT_EQ(subtree_sum(adj, u), T.subtree_sum(u));
  }
}


TEST_P(TestTreeSmall, TestPathMax) {
  auto adj = random_forest(16, 3, 3);
  DynamicTree<3> T(16, adj);
  T.go();
  auto p = to_parents(adj);
  for (int u = 0; u < 16; u++) {
    ASSERT_EQ(path_max(p, u), T.path_max(u));
  }
}
*/

// ----------------------------------------------------------------------------
//                              Large tests
// ----------------------------------------------------------------------------

INSTANTIATE_MT_TESTS(TestTreeLarge);

// Test that the list contraction algorithm produces a computation
TEST_P(TestTreeLarge, TestConstruction) {
  auto adj = random_tree(10000, 10);
  auto T = DynamicTree<10>(10000, adj);
  T.go();
  ASSERT_NE(T.computation.root, nullptr);
}

// Test that queries are correct for random forests
TEST_P(TestTreeLarge, TestQuery) {
  auto adj = random_forest(10000, 10, 20);
  DynamicTree<10> T(10000, adj);
  T.go();
  auto p = to_parents(adj);
  for (int u = 0; u < 10000; u++) {
    ASSERT_EQ(find_rep(p, u), T.find_rep(u));
  }
}
/*
TEST_P(TestTreeLarge, TestSubtreeSum) {
  auto adj = random_forest(1000, 10, 20);
  DynamicTree<10> T(1000, adj);
  T.go();
  auto p = to_parents(adj);
  for (int u = 0; u < 1000; u++) {
    ASSERT_EQ(subtree_sum(adj, u), T.subtree_sum(u));
  }
}

TEST_P(TestTreeLarge, TestPathMax) {
  auto adj = random_forest(1000, 10, 20);
  DynamicTree<10> T(1000, adj);
  T.go();
  auto p = to_parents(adj);
  for (int u = 0; u < 1000; u++) {
    ASSERT_EQ(path_max(p, u), T.path_max(u));
  }
}
*/
