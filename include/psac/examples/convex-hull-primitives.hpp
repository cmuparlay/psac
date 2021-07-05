#ifndef PSAC_EXAMPLES_CONVEX_HULL_PRIMITIVES_HPP_
#define PSAC_EXAMPLES_CONVEX_HULL_PRIMITIVES_HPP_

#include <vector>
#include <utility>
#include <cmath>

#include "psac/examples/bst-primitives.hpp"
#include <psac/psac.hpp>

class Point{
public:
    int x;
    int y;

    Point() {
        x = y = -1;
    }

    Point(int _x, int _y) : x(_x), y(_y) {}

    bool operator==(const Point& p)const {
        return x == p.x && y == p.y;
    }
    bool operator!=(const Point& p)const {
        return !(*this == p);
    }

    bool operator<(const Point& p)const {
        if (y != p.y) return y < p.y;
        else return x < p.x;
    }

    bool operator>(const Point& p)const {
        if (y != p.y) return y > p.y;
        else return x > p.x;
    }

    bool operator<=(const Point& p)const {
        return (*this < p) || (*this == p);
    }

    bool operator>=(const Point& p)const {
        return (*this > p) || (*this == p);
    }

    Point operator-(const Point& p)const {
        return {x - p.x,y - p.y};
    }
    Point operator-()const {
        return {-x, -y};
    }
    long cross(const Point& p) const {
        return (long)x * p.y - (long)y * p.x;
    }
};

class StaticLCHull{
public:

    struct Node {
        Node* parent; // parent of node
        Point bl, br; // left and right end points of the bridge
        Node *cl, *cr; // left and right child of the node
        Point bottom; // bottommost point in node
        StaticBst<Point, void*> qstar; // contains the convex hull of the points in subtree not in parent
        explicit Node(StaticBst<Point, void*> qstar) : qstar(qstar) {
            parent = cl = cr = nullptr;
        };
    };

    static inline thread_local std::vector<std::unique_ptr<Node>> tree;
    size_t GRAN_LIMIT, GRAN_LIMIT_BST;
    Node* root;

    Node* make_node() {
        StaticBst<Point, void*> bst = StaticBst<Point, void*>(this->GRAN_LIMIT_BST);
        tree.push_back(std::make_unique<Node>(bst));
        return tree.back().get();
    }

    // assume already sorted by y coord
    explicit StaticLCHull(std::vector<std::pair<int, int>> ps, size_t gran_limit_bst, size_t gran_limit) {

        this->GRAN_LIMIT_BST = gran_limit_bst;
        this->GRAN_LIMIT = gran_limit;
        size_t s = ps.size();
        std::vector<Point> points(s);
        for (size_t i = 0; i < s; i++) {
            Point p { ps[i].first, ps[i].second };
            points[i] = p;
        }

        this->root = make_node();

        make_tree(this->root, nullptr, points, 0, s);
        init_bridges(this->root, 0, s);
        init_hull(this->root, 0, s);
    }

    long static cross (Point a, Point b, Point c) {
        return (b - a).cross(c - a);
    }

    bool static is_leaf(Node* node) {
        return (node->cl == nullptr) && (node->cr == nullptr);
    }

    // assume that l and r are both non null
    std::pair<Point, Point> static find_bridge(Node* node) {
        Node *l = node->cl, *r = node->cr;
        int split_y = node->bottom.y; // lowest point of the higher hull
        while(!is_leaf(l) || !is_leaf(r)) {
            Point a = l->bl, b = l->br;
            Point c = r->bl, d = r->br;

            // cross product positive iff the 3 points are counter clockwise
            if (a != b && cross(a, b, c) > 0) l = l->cl;
            else if (c != d && cross(b, c, d) > 0) r = r->cr;
            else if (a == b) r = r->cl;
            else if (c == d) l = l->cr;
            else {
                long s1 = cross(a, b, c); // non-positive
                long s2 = cross(b, a, d); // non-negative
                assert(s1 + s2 >= 0); // sum is non-negative because a, b, c, d are clockwise.

                // extend the lines ab and cd and check if they meet above split_y
                if (s1 + s2 == 0 ||
                    s1 * d.y + s2 * c.y < split_y * (s1 + s2)) l = l->cr;
                else r = r->cl;
            }
        }
        return std::make_pair(l->bottom, r->bottom);
    }

    // node != nullptr
    void make_tree(Node* node, Node* parent, const std::vector<Point>& points, size_t l, size_t r) {
        node->bottom = points[l];
        node->parent = parent;
        if (r - l == 1) {
            node->cl = node->cr = nullptr;
            node->bl = node->br = node->bottom;
            node->qstar = StaticBst<Point, void*>(this->GRAN_LIMIT_BST);
            node->qstar.insert(node->bottom,nullptr);
            return;
        }

        size_t mid = l + (r - l) / 2;
        node->cl = make_node();
        node->cr = make_node();

        if (r - l > this->GRAN_LIMIT) {
            parlay::par_do(
                    [&]() { make_tree(node->cl, node, points, l, mid); },
                    [&]() { make_tree(node->cr, node, points, mid, r); }
            );
        }
        else {
            make_tree(node->cl, node, points, l, mid);
            make_tree(node->cr, node, points, mid, r);
        }
    }

    void init_bridges(Node* node, size_t l, size_t r) {
        if (is_leaf(node)) return;
        size_t mid = l + (r - l) / 2;
        if (r - l > this->GRAN_LIMIT) {
            parlay::par_do(
                    [&]() { init_bridges(node->cl, l, mid); },
                    [&]() { init_bridges(node->cr, mid, r); }
            );
        }
        else {
            init_bridges(node->cl, l, mid);
            init_bridges(node->cr, mid, r);
        }
        std::pair<Point,Point> bridge = find_bridge(node);
        node->bl = bridge.first;
        node->br = bridge.second;
    }

    // update the qstar of node and it's children
    void update_qstar(Node* node) {
        StaticSplitNode<Point, void*>* leftSplitNode = node->cl->qstar.split(node->cl->qstar.root, node->bl);
        StaticSplitNode<Point, void*>* rightSplitNode = node->cr->qstar.split(node->cr->qstar.root, node->br);

        // the parts of the CH that are not in the parent
        node->cl->qstar.root = leftSplitNode->right;
        node->cr->qstar.root = rightSplitNode->left;

        node->qstar = StaticBst<Point, void*>(this->GRAN_LIMIT_BST); // old qstar gets thrown out
        node->qstar.root = node->qstar.join2(leftSplitNode->left, rightSplitNode->right);
        node->qstar.insert(node->bl, nullptr);
        node->qstar.insert(node->br, nullptr);
    }

    void init_hull(Node* node, size_t l, size_t r){
        if (is_leaf(node)) return;
        size_t mid = l + (r - l) / 2;
        if (r - l > this->GRAN_LIMIT) {
            parlay::par_do(
                    [&]() { init_hull(node->cl, l, mid); },
                    [&]() { init_hull(node->cr, mid, r); }
            );
        }
        else {
            init_hull(node->cl, l, mid);
            init_hull(node->cr, mid, r);
        }
        update_qstar(node);
    }

    void insert_helper(Node* node, Point p, bool from_left) {
        if (is_leaf(node)) {
            Node* new_internal = make_node();
            Node* child_left = make_node();
            Node* child_right = make_node();

            Point small, big;
            if (p < node->bottom) {
                small = p;
                big = node->bottom;
            }
            else {
                small = node->bottom;
                big = p;
            }

            if (node->parent != nullptr) {
                if (from_left) node->parent->cl = new_internal;
                else node->parent->cr = new_internal;
            }

            new_internal->bl = small;
            new_internal->br = big;
            new_internal->cl = child_left;
            new_internal->cr = child_right;
            new_internal->bottom = small;
            new_internal->parent = node->parent;
            new_internal->qstar = StaticBst<Point, void*>(this->GRAN_LIMIT_BST);
            new_internal->qstar.insert(small, nullptr);
            new_internal->qstar.insert(big, nullptr);

            child_left->bottom = child_left->bl = child_left->br = small;
            child_left->parent = new_internal;

            child_right->bottom = child_right->bl = child_right->br = big;
            child_right->parent = new_internal;
        }
        else {
            // first push the qstar down to create complete hulls for children
            StaticSplitNode<Point, void*>* topSplitNode = node->qstar.split(node->qstar.root, node->bl);
            node->cl->qstar.root = node->cl->qstar.join(topSplitNode->left, node->bl, nullptr, node->cl->qstar.root);
            node->cr->qstar.root = node->cr->qstar.join2(node->cr->qstar.root, topSplitNode->right);
            if (p < node->cr->bottom) insert_helper(node->cl, p, true);
            else insert_helper(node->cr, p, false);

            if (p < node->bottom) node->bottom = p;
            std::pair<Point,Point> bridge = find_bridge(node);
            node->bl = bridge.first;
            node->br = bridge.second;
            update_qstar(node);
        }
    }

    void insert(Point p) {
        insert_helper(root, p, true);
    }

    void batch_insert_helper(Node* node, std::vector<Point>& ps, bool from_left, size_t l, size_t r) {
        if (l == r) return;

        if (is_leaf(node)) {
            // add the point p to ps
            std::vector<Point> new_ps;
            bool b = true;
            Point p = node->bottom;
            for (size_t i = l; i < r; i++) {
                if (p == ps[i]) b = false;
                if (b && p < ps[i]) {
                    new_ps.push_back(p);
                    b = false;
                }
                new_ps.push_back(ps[i]);
            }
            if (b) new_ps.push_back(p);

            Node* new_internal = make_node();
            make_tree(new_internal, node->parent, new_ps, 0, new_ps.size());
            init_bridges(new_internal, 0, new_ps.size());
            init_hull(new_internal, 0, new_ps.size());

            if (node->parent != nullptr) {
                if (from_left) node->parent->cl = new_internal;
                else node->parent->cr = new_internal;
            }
        }
        else {
            // first push the qstar down to create complete hulls for children
            StaticSplitNode<Point, void*>* topSplitNode = node->qstar.split(node->qstar.root, node->bl);
            node->cl->qstar.root = node->cl->qstar.join(topSplitNode->left, node->bl, nullptr, node->cl->qstar.root);
            node->cr->qstar.root = node->cr->qstar.join2(node->cr->qstar.root, topSplitNode->right);
            if (ps[l] < node->bottom) node->bottom = ps[l];
            size_t mid = r;
            for (size_t i = l; i < r; i++) {
                if (node->cr->bottom <= ps[i]) {
                    mid = i;
                    break;
                }
            }
            if (r - l > this->GRAN_LIMIT) {
                parlay::par_do(
                        [&]() { batch_insert_helper(node->cl, ps, true, l, mid); },
                        [&]() { batch_insert_helper(node->cr, ps, false, mid, r); }
                );
            }
            else {
                batch_insert_helper(node->cl, ps, true, l, mid);
                batch_insert_helper(node->cr, ps, false, mid, r);
            }

            std::pair<Point,Point> bridge = find_bridge(node);
            node->bl = bridge.first;
            node->br = bridge.second;
            update_qstar(node);
        }
    }

    void batch_insert(std::vector<Point>& ps) {
        batch_insert_helper(this->root, ps, true, 0, ps.size());
    }

    // report the point in the convex hull that has the y coordinate just <= y
    Point query(int y) {
        StaticNodePtr<Point, void*> node = this->root->qstar.root;
        Point res;
        while(!node.is_leaf()) {
            StaticINode<Point, void*>* inode = node.get_inode();
            if (inode->key.y == y) return inode->key;
            else if (inode->key.y < y) {
                res = inode->key;
                node = inode->right;
            }
            else node = inode->left;
        }
        StaticLeaf<Point, void*>* leaf = node.get_leaf();
        if (leaf != nullptr) {
            for (size_t i = 0; i < leaf->arr.size(); i++) {
                if (leaf->arr[i].first.y <= y) res = leaf->arr[i].first;
            }
        }
        return res;
    }
};

#endif