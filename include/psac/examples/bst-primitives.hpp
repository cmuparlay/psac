#ifndef PSAC_EXAMPLES_BST_PRIMITIVES_HPP_
#define PSAC_EXAMPLES_BST_PRIMITIVES_HPP_

#include <psac/psac.hpp>

template<typename T, typename U>
struct NodePtr;

template<typename T, typename U>
struct INode {
    psac::Mod<NodePtr<T,U>> left;
    psac::Mod<T> key;
    psac::Mod<U> val;
    psac::Mod<NodePtr<T,U>> right;
    INode() = default;
};

template<typename T, typename U>
struct Leaf {
    psac::Mod<std::vector<std::pair<T,U>>> arr;
    Leaf() = default;
};

template<typename T, typename U>
struct SplitNode {
    psac::Mod<NodePtr<T,U>> left;
    psac::Mod<bool> found;
    psac::Mod<NodePtr<T,U>> right;

    SplitNode() = default;
};

template<typename W>
struct ReduceNode {
    psac::Mod<ReduceNode*> left;
    psac::Mod<W> val;
    psac::Mod<ReduceNode*> right;

    ReduceNode() = default;
};

// leaves end their pointer representation in 1 and internal nodes end in 0
template<typename T, typename U>
struct NodePtr {
    union {
        INode<T,U>* inode;
        Leaf<T,U>* leaf;
    };

    NodePtr() {
        leaf = (Leaf<T,U>*)1;
    }

    explicit NodePtr(INode<T,U>* _inode) {
        inode = _inode;
    }

    explicit NodePtr(Leaf<T,U>* _leaf) {
        leaf = (Leaf<T,U>*)((intptr_t)_leaf | 1);
    }

    bool is_leaf() const {
        if ((intptr_t)inode & 1) return true;
        else return false;
    }

    Leaf<T,U>* get_leaf() const {
        return (Leaf<T,U>*)((intptr_t)leaf ^ 1);
    }

    INode<T,U>* get_inode() const {
        return inode;
    }

    bool operator==(const NodePtr& node)const {
        return (intptr_t)this->inode == (intptr_t)node.inode;
    }
    bool operator!=(const NodePtr& node)const {
        return !(*this == node);
    }

};


template<typename T, typename U>
struct StaticNodePtr;

template<typename T, typename U>
struct StaticINode {
    StaticNodePtr<T,U> left;
    T key;
    U val;
    StaticNodePtr<T,U> right;

    StaticINode() = default;

    explicit StaticINode(T _key, U _val) {
        key = std::move(_key);
        val = std::move(_val);
    }
};

template<typename T, typename U>
struct StaticLeaf {
    std::vector<std::pair<T,U>> arr{};
    StaticLeaf() = default;
};

// leaves end their pointer representation in 1 and internal nodes end in 0
template<typename T, typename U>
struct StaticNodePtr {
    union {
        StaticINode<T,U>* inode;
        StaticLeaf<T,U>* leaf;
    };

    StaticNodePtr() {
        leaf = (StaticLeaf<T,U>*)1;
    }

    explicit StaticNodePtr(StaticINode<T,U>* _inode) {
        inode = _inode;
    }

    explicit StaticNodePtr(StaticLeaf<T,U>* _leaf) {
        leaf = (StaticLeaf<T,U>*)((intptr_t)_leaf | 1);
    }

    bool is_leaf() const{
        if ((intptr_t)inode & 1) return true;
        else return false;
    }

    StaticLeaf<T,U>* get_leaf() const{
        return (StaticLeaf<T,U>*)((intptr_t)leaf ^ 1);
    }

    StaticINode<T,U>* get_inode() const{
        return inode;
    }

    bool operator==(const StaticNodePtr& node)const {
        return (intptr_t)this->inode == (intptr_t)node.inode;
    }
    bool operator!=(const StaticNodePtr& node)const {
        return !(*this == node);
    }
};

template<typename T, typename U>
struct StaticSplitNode{
    StaticNodePtr<T,U> left;
    bool found;
    StaticNodePtr<T,U> right;
};

template<typename W>
struct StaticReduceNode{
    StaticReduceNode<W>* left;
    W val;
    StaticReduceNode<W>* right;
};

// a BST interface for non self-adjusting functions
template <typename T, typename U, typename W>
class StaticBst {
public:

    StaticNodePtr<T,U> root;
    size_t GRAN_LIMIT;

    StaticBst(size_t gran_limit) {
        root = StaticNodePtr<T,U>();
        GRAN_LIMIT = gran_limit;
    }

    static inline thread_local std::vector<std::unique_ptr<StaticINode<T,U>>> inodes;
    static inline thread_local std::vector<std::unique_ptr<StaticLeaf<T,U>>> leaves;
    static inline thread_local std::vector<std::unique_ptr<StaticSplitNode<T,U>>> splitnodes;
    static inline thread_local std::vector<std::unique_ptr<StaticReduceNode<W>>> reducenodes;

    StaticINode<T,U>* make_inode() {
        inodes.push_back(std::make_unique<StaticINode<T,U>>());
        return inodes.back().get();
    }
    StaticLeaf<T,U>* make_leaf() {
        leaves.push_back(std::make_unique<StaticLeaf<T,U>>());
        return leaves.back().get();
    }
    StaticSplitNode<T,U>* make_splitnode() {
        splitnodes.push_back(std::make_unique<StaticSplitNode<T,U>>());
        return splitnodes.back().get();
    }
    StaticReduceNode<W>* make_reducenode() {
        reducenodes.push_back(std::make_unique<StaticReduceNode<W>>());
        return reducenodes.back().get();
    }

    size_t sz(StaticNodePtr<T,U> node) {
        if (node.is_leaf()) {
            if (node == StaticNodePtr<T,U>()) return 0;
            else return node.get_leaf()->arr.size();
        }
        else {
            return 1 + sz(node.get_inode()->left) + sz(node.get_inode()->right);
        }
    }

    // assumes input is sorted and deduped
    // the indices specify the left (inclusive) and right (exclusive) positions for making the tree
    StaticNodePtr<T,U> make_tree(const std::vector<std::pair<T,U>>& vals, size_t l, size_t r) {
        if (r - l <= this->GRAN_LIMIT) {
            StaticLeaf<T,U>* leaf = make_leaf();
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = l; i < r; i++) new_vec.push_back(vals[i]);
            leaf->arr = new_vec;
            return StaticNodePtr<T,U>(leaf);
        }

        size_t mid = l + (r-l)/2;
        StaticINode<T,U>* inode = make_inode();
        inode->key = vals[mid].first;
        inode->val = vals[mid].second;

        parlay::par_do(
                [&]() { inode->left = make_tree(vals, l, mid); },
                [&]() { inode->right = make_tree(vals, mid+1, r); }
        );

        return (StaticNodePtr<T,U>(inode));
    }

    static bool key_equals(std::pair<T,U> pair1, std::pair<T,U> pair2) {
        return pair1.first == pair2.first;
    }

    void batch_insert_to_leaf(StaticNodePtr<T,U>& node, const std::vector<std::pair<T,U>>& vals, size_t l, size_t r) {
        if (!node.is_leaf()) return;

        StaticLeaf<T,U>* leaf = node.get_leaf();
        if (leaf == nullptr) {
            if (l == r) return;
            node = make_tree(vals, l, r);
            return;
        }

        std::vector<std::pair<T,U>> sorted_vals(leaf->arr.size() + (r-l));
        std::merge(leaf->arr.begin(), leaf->arr.end(), vals.begin() + l, vals.begin() + r, sorted_vals.begin());
        sorted_vals.erase(std::unique(sorted_vals.begin(), sorted_vals.end(), key_equals), sorted_vals.end());
        size_t num_arrs = (sorted_vals.size() - 1) / this->GRAN_LIMIT + 1;

        if (num_arrs == 1) { // can insert rest of the elements into leaf
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = 0; i < sorted_vals.size(); i++) new_vec.push_back(sorted_vals[i]);
            leaf->arr = new_vec;
        }
        else { // need to delete the leaf and put a new internal node in its place
            node = make_tree(sorted_vals, 0, sorted_vals.size());
        }
    }

    void batch_insert_helper(std::vector<std::pair<T,U>>& vals, StaticNodePtr<T,U>& node, size_t l, size_t r) {
        if (l == r) return;

        if (node.is_leaf()) {
            batch_insert_to_leaf(node, vals, l, r);
            return;
        }

        StaticINode<T,U>* inode = node.get_inode();
        size_t left_idx = r;
        for (size_t i = l; i < r; i++) {
            if (vals[i].first >= inode->key) {
                left_idx = i;
                break;
            }
        }

        size_t right_idx = left_idx;
        if (left_idx != r && vals[left_idx].first == inode->key) right_idx += 1;

        if (r - l > this->GRAN_LIMIT) {
            parlay::par_do(
                    [&]() { batch_insert_helper(vals, inode->left, l, left_idx); },
                    [&]() { batch_insert_helper(vals, inode->right, right_idx, r); }
            );
        }
        else {
            batch_insert_helper(vals, inode->left, l, left_idx);
            batch_insert_helper(vals, inode->right, right_idx, r);
        }
    }

    void batch_insert(std::vector<std::pair<T,U>>& vals) {
        batch_insert_helper(vals, this->root, 0, vals.size());
    }

    void insert_helper(T key, U val, StaticNodePtr<T,U>& node) {
        if (node.is_leaf()) batch_insert_to_leaf(node, {std::make_pair(key,val)}, 0, 1);
        else {
            StaticINode<T,U>* inode = node.get_inode();
            if (inode->key > key) insert_helper(key, val, inode->left);
            else if (inode->key < key) insert_helper(key, val, inode->right);
        }
    }

    void insert(T key, U val) {
        insert_helper(key, val, this->root);
    }

    StaticNodePtr<T,U> join(StaticNodePtr<T,U> l, T key, U val, StaticNodePtr<T,U> r) {
        StaticINode<T,U>* res = this->make_inode();
        res->left = l;
        res->right = r;
        res->key = key;
        res->val = val;
        return StaticNodePtr(res);
    }

    // assume that the leaf is not null and res is already allocated
    void join_leaf(StaticLeaf<T,U>* leaf, StaticINode<T,U>* res) {
        assert(leaf != nullptr);
        assert(leaf->arr.size() > 0);
        std::pair<T,U> last_val = leaf->arr.back();
        res->key = last_val.first;
        res->val = last_val.second;
        if (leaf->arr.size() == 1) res->left = StaticNodePtr<T,U>();
        else {
            StaticLeaf<T,U>* new_leaf = make_leaf();
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = 0; i < leaf->arr.size() - 1; i++) new_vec.push_back(leaf->arr[i]);
            new_leaf->arr = new_vec;
            res->left = StaticNodePtr<T,U>(new_leaf);
        }
    }

    StaticNodePtr<T,U> join2(StaticNodePtr<T,U> l, StaticNodePtr<T,U> r) {
        if (l.is_leaf()) {
            if (l == StaticNodePtr<T,U>()) return r;
            else {
                StaticINode<T,U>* res = make_inode();
                StaticLeaf<T,U>* lleaf = l.get_leaf();
                join_leaf(lleaf, res);
                res->right = r;
                return StaticNodePtr<T,U>(res);
            }
        }

        StaticINode<T,U>* linode = l.get_inode();
        StaticNodePtr<T,U> right_res = join2(linode->right, r);
        return join(linode->left, linode->key, linode->val, right_res);
    }

    // assumes that leaf is not null
    StaticSplitNode<T,U>* split_leaf (StaticLeaf<T,U>* leaf, T key) {
        StaticSplitNode<T,U>* res = make_splitnode();
        int idx = -1;
        for (size_t i = 0; i < leaf->arr.size(); i++) {
            if (key <= leaf->arr[i].first) {
                idx = i;
                break;
            }
        }
        if (idx == -1) { // all elements in the vector are smaller than v
            res->found = false;
            res->left = StaticNodePtr<T,U>(leaf);
            res->right = StaticNodePtr<T,U>();
            return res;
        }

        size_t right_index; // the starting index for the right leaf
        if (leaf->arr[idx].first == key) {
            res->found = true;
            right_index = idx + 1;
        }
        else {
            res->found = false;
            right_index = idx;
        }

        // left leaf
        if (idx > 0) {
            StaticLeaf<T,U>* left_leaf = make_leaf();
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = 0; i < (size_t)idx; i++) new_vec.push_back(leaf->arr[i]);
            left_leaf->arr = new_vec;
            res->left = StaticNodePtr<T,U>(left_leaf);
        }
        else res->left = StaticNodePtr<T,U>();

        // right leaf
        if ((int)right_index < (int)leaf->arr.size()) {
            StaticLeaf<T,U>* right_leaf = make_leaf();
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = right_index; i < leaf->arr.size(); i++) new_vec.push_back(leaf->arr[i]);
            right_leaf->arr = new_vec;
            res->right = StaticNodePtr<T,U>(right_leaf);
        }
        else res->right = StaticNodePtr<T,U>();

        return res;
    }

    StaticSplitNode<T,U>* split(StaticNodePtr<T,U> node, T key) {
        if (node.is_leaf()) {
            if (node == StaticNodePtr<T,U>()) {
                StaticSplitNode<T,U>* res = make_splitnode();
                res->found = false;
                res->left = StaticNodePtr<T,U>();
                res->right = StaticNodePtr<T,U>();
                return res;
            }
            else {
                StaticLeaf<T,U>* leaf = node.get_leaf();
                return split_leaf(leaf, key);
            }
        }
        else {
            StaticSplitNode<T,U>* res = make_splitnode();
            StaticINode<T,U>* inode = node.get_inode();
            if (inode->key == key) {
                res->found = true;
                res->left = inode->left;
                res->right = inode->right;
            }
            else if (key < inode->key) {
                StaticSplitNode<T,U>* res_left = split(inode->left, key);
                res->found = res_left->found;
                res->left = res_left->left;
                res->right = join(res_left->right, inode->key, inode->val, inode->right);
            }
            else {
                StaticSplitNode<T,U>* res_right = split(inode->right, key);
                res->found = res_right->found;
                res->right = res_right->right;
                res->left = join(inode->left, inode->key, inode->val, res_right->left);
            }
            return res;
        }
    }

    std::vector<std::pair<T,U>> flatten_leaf(StaticLeaf<T,U>* leaf) {
        if (leaf == nullptr) {
            std::vector<std::pair<T,U>> a;
            return a;
        }
        return leaf->arr;
    }

    std::vector<std::pair<T,U>> flatten(StaticNodePtr<T,U> node) {
        if (node.is_leaf()) return flatten_leaf(node.get_leaf());
        StaticINode<T,U>* inode = node.get_inode();
        std::vector<std::pair<T,U>> vec1 = flatten(inode->left);
        std::vector<std::pair<T,U>> vec2 = flatten(inode->right);
        vec1.push_back(std::make_pair(inode->key, inode->val));
        for (size_t i = 0; i < vec2.size(); i++) vec1.push_back(vec2[i]);
        return vec1;
    }

    StaticNodePtr<T,U> filter_leaf(StaticLeaf<T,U>* leaf, const std::function<bool(U)>& f) {
        StaticLeaf<T,U>* new_leaf = make_leaf();
        std::vector<std::pair<T,U>> new_vec;
        new_vec.reserve(this->GRAN_LIMIT);
        for (size_t i = 0; i < leaf->arr.size(); i++) {
            if (f(leaf->arr[i].second)) new_vec.push_back(leaf->arr[i]);
        }
        new_leaf->arr = new_vec;
        if (leaf->arr.size() == 0) return StaticNodePtr<T,U>();
        else return StaticNodePtr<T,U>(new_leaf);
    }

    StaticNodePtr<T,U> filter_seq_helper(StaticNodePtr<T,U> node, const std::function<bool(U)>& f) {
        if (node.is_leaf()) {
            StaticLeaf<T,U>* leaf = node.get_leaf();
            if (leaf == nullptr) return StaticNodePtr<T,U>();
            else return filter_leaf(leaf, f);
        }

        StaticINode<T,U>* inode = node.get_inode();
        auto res_left = this->filter_seq_helper(inode->left, f);
        auto res_right = this->filter_seq_helper(inode->right, f);

        StaticNodePtr<T,U> res;
        if (f(inode->val)) {
            StaticINode<T,U>* inode_res = this->make_inode();
            inode_res->key = inode->key;
            inode_res->val = inode->val;
            inode_res->left = res_left;
            inode_res->right = res_right;
            res = StaticNodePtr<T,U>(inode_res);
        }
        else res = this->join2(res_left, res_right);
        return res;
    }

    StaticNodePtr<T,U> filter_par_helper(StaticNodePtr<T,U> node, const std::function<bool(U)>& f) {
        if (node.is_leaf()) {
            StaticLeaf<T,U>* leaf = node.get_leaf();
            if (leaf == nullptr) return StaticNodePtr<T,U>();
            else return filter_leaf(leaf, f);
        }

        StaticINode<T,U>* inode = node.get_inode();
        StaticNodePtr<T,U> res_left, res_right;
        parlay::par_do(
                [&]() { res_left = this->filter_par_helper(inode->left, f); },
                [&]() { res_right = this->filter_par_helper(inode->right, f); }
        );

        StaticNodePtr<T,U> res;
        if (f(inode->val)) {
            StaticINode<T,U>* inode_res = this->make_inode();
            inode_res->key = inode->key;
            inode_res->val = inode->val;
            inode_res->left = res_left;
            inode_res->right = res_right;
            res = StaticNodePtr<T,U>(inode_res);
        }
        else {
            res = this->join2(res_left, res_right);
        }
        return res;
    }

    StaticReduceNode<W>* mapreduce_leaf(StaticLeaf<T,U>* leaf, W base,
                                              const std::function<W(U)>& m, const std::function<W(W, W)>& r) {
        assert(leaf != nullptr);
        W total = base;
        StaticReduceNode<W>* res = this->make_reducenode();
        for (size_t i = 0; i < leaf->arr.size(); i++) total = r(total, m(leaf->arr[i].second));
        res->val = total;
        res->left = res->right = nullptr;
        return res;
    };

    StaticReduceNode<W>* mapreduce_seq_helper(StaticNodePtr<T,U> node, W base,
                                              const std::function<W(U)>& m, const std::function<W(W, W)>& r) {
        if (node.is_leaf()) {
            StaticLeaf<T,U>* leaf = node.get_leaf();
            if (leaf == nullptr) return nullptr;
            else return mapreduce_leaf(leaf, base, m, r);
        }

        StaticINode<T,U>* inode = node.get_inode();
        auto res_left = this->mapreduce_seq_helper(inode->left, base, m, r);
        auto res_right = this->mapreduce_seq_helper(inode->right, base, m, r);

        StaticReduceNode<W>* res = this->make_reducenode();
        W val_left = res_left ? res_left->val : base;
        W val_right = res_right ? res_right->val : base;
        res->left = res_left;
        res->right = res_right;
        res->val = r(r(val_left, m(inode->val)), val_right);
        return res;
    }

    StaticReduceNode<W>* mapreduce_par_helper(StaticNodePtr<T,U> node, W base,
                                              const std::function<W(U)>& m, const std::function<W(W, W)>& r) {
        if (node.is_leaf()) {
            StaticLeaf<T,U>* leaf = node.get_leaf();
            if (leaf == nullptr) return nullptr;
            else return mapreduce_leaf(leaf, base, m, r);
        }

        StaticINode<T,U>* inode = node.get_inode();
        StaticReduceNode<W> *res_left, *res_right;
        parlay::par_do(
                [&]() { res_left = this->mapreduce_par_helper(inode->left, base, m, r); },
                [&]() { res_right = this->mapreduce_par_helper(inode->right, base, m, r); }
        );

        StaticReduceNode<W>* res = this->make_reducenode();
        W val_left = res_left ? res_left->val : base;
        W val_right = res_right ? res_right->val : base;
        res->left = res_left;
        res->right = res_right;
        res->val = r(r(val_left, m(inode->val)), val_right);
        return res;
    }
};

#endif