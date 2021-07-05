#include "gtest/gtest.h"
#include "common.hpp"

#include <limits>
#include <utility>
#include <vector>
#include <numeric>

#include <psac/psac.hpp>
#include "psac/examples/bst-primitives.hpp"
#include "psac/examples/convex-hull-primitives.hpp"
#include "psac/examples/convex-hull.hpp"

const size_t GRAN_LIMIT = 1;
const size_t GRAN_LIMIT2 = 4;

INSTANTIATE_MT_TESTS(TestConvexHull);

void print_static_tree(StaticNodePtr<Point, void*> node) {
    if (node.is_leaf()) {
        StaticLeaf<Point, void*>* leaf = node.get_leaf();
        if (leaf == nullptr) return;
        for (size_t i = 0; i < leaf->arr.size(); i++) std::cout<<leaf->arr[i].first.x<<" "<<leaf->arr[i].first.y<<std::endl;
        return;
    }
    StaticINode<Point, void*>* inode = node.get_inode();
    print_static_tree(inode->left);
    std::cout<<inode->key.x<<" "<<inode->key.y<<std::endl;
    print_static_tree(inode->right);
}

template <typename T, typename U>
void vec_equals(std::vector<std::pair<T,U>> v1, std::vector<T> v2) {
    ASSERT_EQ(v1.size(), v2.size());
    for (size_t i = 0; i < v1.size(); i++) {
        ASSERT_EQ(v1[i].first, v2[i]);
    }
}

TEST_P(TestConvexHull, TestStaticHull) {
    std::vector<std::pair<int,int>> points{
        {9, 1}, {4,2}, {8,3}, {6,5},
        {3,6}, {4,7}, {8, 8}, {4,10},
        {5, 11}
    };

    StaticLCHull hull(points, GRAN_LIMIT, GRAN_LIMIT);
    std::vector<Point> ans1{
        Point{9,1}, Point{4,2}, Point{3,6},
        Point{4,10}, Point{5,11}
    };
    vec_equals(hull.root->qstar.flatten(hull.root->qstar.root), ans1);

    Point q0 = hull.query(1);
    Point q1 = hull.query(3);
    Point q2 = hull.query(9);
    Point q3 = hull.query(11);
    ASSERT_EQ(q0.x, 9);
    ASSERT_EQ(q0.y, 1);
    ASSERT_EQ(q1.x, 4);
    ASSERT_EQ(q1.y, 2);
    ASSERT_EQ(q2.x, 3);
    ASSERT_EQ(q2.y, 6);
    ASSERT_EQ(q3.x, 5);
    ASSERT_EQ(q3.y, 11);

    hull.insert(Point{1,9});
    std::vector<Point> ans2{
        Point{9,1}, Point{4,2}, Point{1,9},
        Point{5,11}
    };
    vec_equals(hull.root->qstar.flatten(hull.root->qstar.root), ans2);

    hull.insert(Point{1,4});
    std::vector<Point> ans3{
        Point{9,1}, Point{4,2}, Point{1,4},
        Point{1,9}, Point{5,11}
    };
    vec_equals(hull.root->qstar.flatten(hull.root->qstar.root), ans3);
}

TEST_P(TestConvexHull, TestBatchInsertStaticHull) {
    std::vector<std::pair<int,int>> points{
            {9, 1}, {4,2}, {8,3}, {6,5},
            {3,6}, {4,7}, {8, 8}, {4,10},
            {5, 11}
    };

    StaticLCHull hull(points, GRAN_LIMIT, GRAN_LIMIT);
    std::vector<Point> ans1{
        Point{9,1}, Point{4,2}, Point{3,6},
        Point{4,10}, Point{5,11}
    };
    vec_equals(hull.root->qstar.flatten(hull.root->qstar.root), ans1);

//    print_tree(hull.root->qstar->root.value);

    std::vector<Point> new_ps{
            Point{1,4}, Point{4,4}, Point{5,6}, Point{2,8},
            Point{1,9}
    };
    hull.batch_insert(new_ps);
    std::vector<Point> ans3{
            Point{9,1}, Point{4,2}, Point{1,4},
            Point{1,9}, Point{5,11}
    };
//    print_tree(hull.root->qstar->root.value);
    vec_equals(hull.root->qstar.flatten(hull.root->qstar.root), ans3);
}

TEST_P(TestConvexHull, TestBatchInsertDynamicHull) {
    std::vector<std::pair<int,int>> points{
            {9, 1}, {4,2}, {8,3}, {6,5},
            {3,6}, {4,7}, {8, 8}, {4,10},
            {5, 11}
    };

    LCHull hull(points, GRAN_LIMIT);
    hull.comp = psac_run(hull.build);

//    std::vector<Point> ans1{
//            Point{9,1}, Point{4,2}, Point{3,6},
//            Point{4,10}, Point{5,11}
//    };

    Point q0 = hull.query(1);
    Point q1 = hull.query(3);
    Point q2 = hull.query(9);
    Point q3 = hull.query(11);
    ASSERT_EQ(q0.x, 9);
    ASSERT_EQ(q0.y, 1);
    ASSERT_EQ(q1.x, 4);
    ASSERT_EQ(q1.y, 2);
    ASSERT_EQ(q2.x, 3);
    ASSERT_EQ(q2.y, 6);
    ASSERT_EQ(q3.x, 5);
    ASSERT_EQ(q3.y, 11);

    std::vector<Point> new_ps{
            Point{1,4}, Point{4,4}, Point{5,6}, Point{2,8},
            Point{1,9}
    };
    hull.batch_insert(new_ps);
//    std::vector<Point> ans3{
//            Point{9,1}, Point{4,2}, Point{1,4},
//            Point{1,9}, Point{5,11}
//    };
    psac_propagate(hull.comp);
    psac::GarbageCollector::run();

    Point q4 = hull.query(5);
    Point q5 = hull.query(7);
    Point q6 = hull.query(10);
    Point q7 = hull.query(11);
    ASSERT_EQ(q4.x, 1);
    ASSERT_EQ(q4.y, 4);
    ASSERT_EQ(q5.x, 1);
    ASSERT_EQ(q5.y, 4);
    ASSERT_EQ(q6.x, 1);
    ASSERT_EQ(q6.y, 9);
    ASSERT_EQ(q7.x, 5);
    ASSERT_EQ(q7.y, 11);
}

TEST_P(TestConvexHull, TestBatchInsertDynamicHullGranControl) {
    std::vector<std::pair<int,int>> points{
            {9, 1}, {4,2}, {8,3}, {6,5},
            {3,6}, {4,7}, {8, 8}, {4,10},
            {5, 11}
    };

    LCHull hull(points, GRAN_LIMIT2);
    hull.comp = psac_run(hull.build);

    std::vector<Point> ans1{
            Point{9,1}, Point{4,2}, Point{3,6},
            Point{4,10}, Point{5,11}
    };

    Point q0 = hull.query(1);
    Point q1 = hull.query(3);
    Point q2 = hull.query(9);
    Point q3 = hull.query(11);
    ASSERT_EQ(q0.x, 9);
    ASSERT_EQ(q0.y, 1);
    ASSERT_EQ(q1.x, 4);
    ASSERT_EQ(q1.y, 2);
    ASSERT_EQ(q2.x, 3);
    ASSERT_EQ(q2.y, 6);
    ASSERT_EQ(q3.x, 5);
    ASSERT_EQ(q3.y, 11);

    std::vector<Point> new_ps{
            Point{1,4}, Point{4,4}, Point{5,6}, Point{2,8},
            Point{1,9}
    };
    hull.batch_insert(new_ps);
//    std::vector<Point> ans3{
//            Point{9,1}, Point{4,2}, Point{1,4},
//            Point{1,9}, Point{5,11}
//    };
    psac_propagate(hull.comp);
    psac::GarbageCollector::run();

    Point q4 = hull.query(5);
    Point q5 = hull.query(7);
    Point q6 = hull.query(10);
    Point q7 = hull.query(11);
    ASSERT_EQ(q4.x, 1);
    ASSERT_EQ(q4.y, 4);
    ASSERT_EQ(q5.x, 1);
    ASSERT_EQ(q5.y, 4);
    ASSERT_EQ(q6.x, 1);
    ASSERT_EQ(q6.y, 9);
    ASSERT_EQ(q7.x, 5);
    ASSERT_EQ(q7.y, 11);
}