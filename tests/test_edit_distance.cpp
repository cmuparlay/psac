#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>
#include <psac/examples/editdistance.hpp>
#include <psac/examples/bst.hpp>
#include <psac/examples/bst-primitives.hpp>

INSTANTIATE_MT_TESTS(TestEditDistance);

// Test that the edit distance for one string is correct
TEST_P(TestEditDistance, TestSingle) {
  std::string s("kitten");

  // Input array of size 1
  psac::ModArray<std::string> in(1);
  in[0].write("sitting");
  
  psac::Mod<int> res;
  auto comp = psac_run(reduce_edit_distance, std::begin(in), std::end(in), &s, &res);
  ASSERT_EQ(res.value, 3);
}


// Test that the minimum edit distance for many strings is correct
TEST_P(TestEditDistance, TestMin) {
  std::string s("kitten");

  // Input array of size 1
  psac::ModArray<std::string> in(4);
  in[0].write("sitting");
  in[1].write("sittan");
  in[2].write("sitten");
  in[3].write("gittang");
  
  psac::Mod<int> res;
  auto comp = psac_run(reduce_edit_distance, std::begin(in), std::end(in), &s, &res);
  ASSERT_EQ(res.value, 1);
}

TEST_P(TestEditDistance, TestDynamic) {

    std::vector<std::pair<int, std::string>> words;
    words.push_back(std::make_pair(1, "sitting"));
    words.push_back(std::make_pair(2, "sittan"));
    words.push_back(std::make_pair(4, "gittang"));
    Bst<int, std::string, int> bst(1);
    std::string target("kitten");
    bst.batch_insert(words);

    auto map_fn = [&](std::string s) { return edit_distance(s, target); };
    auto reduce_fn = [&](int a, int b) { return std::min(a, b); };

    ReduceNode<int>* res = bst.make_reducenode();
    psac::Computation comp = psac_run(bst.mapreduce, bst.root.value, res, INT_MAX, map_fn, reduce_fn);
    ASSERT_EQ(res->val.value, 2);

    bst.insert(3, "sitten");
    psac_propagate(comp);
    psac::GarbageCollector::run();
    ASSERT_EQ(res->val.value, 1);
}

