#include "gtest/gtest.h"
#include "common.hpp"

#include <limits>
#include <utility>
#include <vector>
#include <numeric>

#include <psac/psac.hpp>
#include <psac/examples/bst.hpp>
#include <psac/examples/bst-primitives.hpp>

const size_t GRAN_LIMIT = 1;
const size_t GRAN_LIMIT2 = 10;

INSTANTIATE_MT_TESTS(TestBst);

TEST_P(TestBst, TestStaticSplit) {
    std::vector<int> nums{1,2,4,5,6,8,9,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 8; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(pairs);

    ASSERT_EQ(bst.sz(bst.root), 8);
    ASSERT_FALSE(bst.root.is_leaf());

    StaticINode<int, int>* root = bst.root.get_inode();
    ASSERT_EQ(root->key, 6);
    ASSERT_FALSE(root->left.is_leaf());
    ASSERT_FALSE(root->right.is_leaf());

    StaticINode<int, int>* lcroot = root->left.get_inode();
    StaticINode<int, int>* rcroot = root->right.get_inode();

    ASSERT_EQ(lcroot->key, 4);
    ASSERT_EQ(rcroot->key, 9);

    StaticSplitNode<int, int>* res = bst.split(bst.root, 8);

    ASSERT_TRUE(res->found);
    ASSERT_EQ(bst.sz(res->left), 5);
    ASSERT_EQ(bst.sz(res->right), 2);
}


TEST_P(TestBst, TestStaticSplitGranControl) {
    std::vector<int> nums{1,2,4,5,6,8,9,10};
    StaticBst<int, int, int> bst(GRAN_LIMIT2);
    for (size_t i = 0; i < nums.size(); i++) bst.insert(nums[i], nums[i]);

    ASSERT_EQ(bst.sz(bst.root), 8);
    ASSERT_TRUE(bst.root.is_leaf());
    StaticSplitNode<int, int>* res = bst.split(bst.root, 8);

    ASSERT_TRUE(res->found);
    ASSERT_EQ(bst.sz(res->left), 5);
    ASSERT_EQ(bst.sz(res->right), 2);
}

TEST_P(TestBst, TestStaticFilter) {
    std::vector<int> nums{1,2,4,5,6,8,9,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 8; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(pairs);

    ASSERT_EQ(bst.sz(bst.root), 8);

    StaticNodePtr<int, int> res = bst.filter_par_helper(bst.root, [](int val) {
        if (val % 2 == 0) return true;
        else return false;
    });

    ASSERT_EQ(bst.sz(res), 5);

    StaticNodePtr<int, int> res2 = bst.filter_seq_helper(bst.root, [](int val) {
        if (val % 2 == 0) return true;
        else return false;
    });

    ASSERT_EQ(bst.sz(res2), 5);
}

TEST_P(TestBst, TestStaticReduce) {
    std::vector<int> nums{1,2,4,5,6,8,9,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 8; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    StaticBst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(pairs);

    StaticReduceNode<int>* res = bst.mapreduce_par_helper(bst.root,0,
                        [](int a) { return a + 1; },
                        [](int a, int b) { return a + b; });

    ASSERT_EQ(res->val, 53);
}

TEST_P(TestBst, TestSmallSplit) {
    std::vector <int> nums{2,6,4,9,8,1,4,5,10,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 10; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.create(pairs);

    ASSERT_EQ(bst.sz(bst.root.value), 8);
    ASSERT_FALSE(bst.root.value.is_leaf());

    INode<int, int>* root = bst.root.value.get_inode();
    ASSERT_FALSE(root->left.value.is_leaf());
    ASSERT_FALSE(root->right.value.is_leaf());

    SplitNode<int, int>* res = bst.make_splitnode();
    psac::Computation comp = psac_run(bst.split, 4, bst.root.value, res);

    ASSERT_TRUE(res->found.value);
    ASSERT_EQ(bst.sz(res->left.value), 2);
    ASSERT_EQ(bst.sz(res->right.value), 5);

    bst.insert(3, 3);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(bst.sz(bst.root.value), 9);
    ASSERT_EQ(bst.sz(res->left.value), 3);
    ASSERT_EQ(bst.sz(res->right.value), 5);

    bst.insert(11, 11);
    bst.insert(7, 7);
    bst.insert(12, 12);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(bst.sz(res->left.value), 3);
    ASSERT_EQ(bst.sz(res->right.value), 8);
}

TEST_P(TestBst, TestSmallFilter) {
    std::vector <int> nums{2,6,4,9,8,1,4,5,10,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 10; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.create(pairs);

    psac::Mod<NodePtr<int, int> > res;
    psac::Computation comp = psac_run(bst.filter, bst.root.value, &res, [](int val) {
        if (val % 2 == 0) return true;
        else return false;
    });

    ASSERT_EQ(bst.sz(res.value), 5);

    bst.insert(11, 11);
    bst.insert(7, 7);
    bst.insert(12, 12);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(bst.sz(res.value), 6);
}

TEST_P(TestBst, TestSmallReduce) {
    std::vector <int> nums{2,6,4,9,8,1,4,5,10,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 10; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.create(pairs);

    ReduceNode<int>* res = bst.make_reducenode();
    psac::Computation comp = psac_run(bst.mapreduce, bst.root.value, res, 0,
                        [](int a) { return a + 1; },
                        [](int a, int b) { return a + b; });

    ASSERT_EQ(res->val.value, 53);

    bst.insert(11, 11);
    bst.insert(7, 7);
    bst.insert(12, 12);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(res->val.value, 86);
}

TEST_P(TestBst, TestBatchInsert) {
    std::vector <int> nums{1,2,4,5,6,8,9,10};
    std::vector<std::pair<int,int>> pairs;
    for (size_t i = 0; i < 8; i++) pairs.push_back(std::make_pair(nums[i], nums[i]));
    Bst<int, int, int> bst(GRAN_LIMIT);
    bst.batch_insert(pairs);

    ASSERT_EQ(bst.sz(bst.root.value), 8);
    ASSERT_FALSE(bst.root.value.is_leaf());

    INode<int, int>* root = bst.root.value.get_inode();
    ASSERT_EQ(root->val.value, 6);
    ASSERT_FALSE(root->left.value.is_leaf());
    ASSERT_FALSE(root->right.value.is_leaf());

    INode<int, int>* lcroot = root->left.value.get_inode();
    INode<int, int>* rcroot = root->right.value.get_inode();

    ASSERT_EQ(lcroot->val.value, 4);
    ASSERT_EQ(rcroot->val.value, 9);

    SplitNode<int, int>* res = bst.make_splitnode();
    psac::Computation comp = psac_run(bst.split, 8, bst.root.value, res);

    ASSERT_TRUE(res->found.value);
    ASSERT_EQ(bst.sz(res->left.value), 5);
    ASSERT_EQ(bst.sz(res->right.value), 2);

    std::vector <int> new_nums{7,11,12};
    std::vector<std::pair<int,int>> new_pairs;
    for (size_t i = 0; i < 3; i++) new_pairs.push_back(std::make_pair(new_nums[i], new_nums[i]));
    bst.batch_insert(new_pairs);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(bst.sz(res->left.value), 6);
    ASSERT_EQ(bst.sz(res->right.value), 4);
}

TEST_P(TestBst, TestLargeFilter) {
    std::vector <std::pair<int,int>> nums;
    for (size_t i = 1; i < 10000; i++) nums.push_back(std::make_pair(i, i));
    Bst<int, int, int> bst(GRAN_LIMIT2);
    bst.batch_insert(nums);

    psac::Mod<NodePtr<int, int>> res;
    psac::Computation comp = psac_run(bst.filter, bst.root.value, &res, [](int val) {
        if (val % 5 == 0) return true;
        else return false;
    });

    ASSERT_EQ(bst.sz(res.value), 1999);

    std::vector<std::pair<int, int>> new_nums;
    for (size_t i = 20001; i < 21000; i++) new_nums.push_back(std::make_pair(i, i));
    bst.batch_insert(new_nums);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(bst.sz(res.value), 2198);
}

TEST_P(TestBst, TestLargeReduce) {
    std::vector<std::pair<int, int>> nums;
    for (size_t i = 1; i < 2000; i++) nums.push_back(std::make_pair(i*5, i*5));
    Bst<int, int, int> bst(GRAN_LIMIT2);
    bst.batch_insert(nums);

    ReduceNode<int>* res = bst.make_reducenode();
    psac::Computation comp = psac_run(bst.mapreduce, bst.root.value, res, 0,
                        [](int a) { return 2 * a; },
                        [](int a, int b) { return std::max(a, b); });

    ASSERT_EQ(res->val.value, 19990);
    ASSERT_EQ(res->left.value->val.value, 9990);

    std::vector <int> new_nums{4997, 4998, 8001, 9001};
    std::vector<std::pair<int,int>> new_pairs;
    for (size_t i = 0; i < 3; i++) new_pairs.push_back(std::make_pair(new_nums[i], new_nums[i]));
    bst.batch_insert(new_pairs);

    psac_propagate(comp);
    psac::GarbageCollector::run();

    ASSERT_EQ(res->val.value, 19990);
    ASSERT_EQ(res->left.value->val.value, 9996);
}

TEST_P(TestBst, TestLargeCompose) {
    std::vector<std::pair<int, int>> nums;
    for (size_t i = 1; i < 2000; i++) nums.push_back(std::make_pair(i*5, i*5));
    Bst<int, int, int> bst(GRAN_LIMIT2);
    bst.batch_insert(nums);

    psac::Mod<NodePtr<int, int>> res_filter;
    ReduceNode<int>* res_reduce = bst.make_reducenode();

    psac::Computation comp1 = psac_run(bst.filter, bst.root.value, &res_filter,
                                       [](int a) { return a % 10 == 0; } );

    psac::Computation comp2 = psac_run(bst.mapreduce, res_filter.value, res_reduce, 0,
                        [](int a) { return 2 * a; },
                        [](int a, int b) { return std::max(a, b); });

    ASSERT_EQ(res_reduce->val.value, 19980);
    ASSERT_EQ(res_reduce->left.value->val.value, 9980);

    std::vector <int> new_nums{4997, 8000, 9000, 12000, 12345, 33333};
    std::vector<std::pair<int,int>> new_pairs;
    for (size_t i = 0; i < 6; i++) new_pairs.push_back(std::make_pair(new_nums[i], new_nums[i]));
    bst.batch_insert(new_pairs);

    psac_propagate(comp1);
    psac_propagate(comp2);
    psac::GarbageCollector::run();

    ASSERT_EQ(res_reduce->val.value, 24000);
}