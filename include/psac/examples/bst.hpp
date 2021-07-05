#include <vector>
#include <utility>

#include <psac/psac.hpp>
#include <psac/examples/bst-primitives.hpp>

template<typename T, typename U, typename W>
class Bst {
    public:
    psac::Mod<NodePtr<T,U>> root;
    size_t GRAN_LIMIT;
    NodePtr<T,U> BLANK;

    explicit Bst(size_t gran_limit) {
        BLANK = NodePtr<T,U>();
        GRAN_LIMIT = gran_limit;
        root.value = BLANK;
    }

    static inline thread_local std::vector<std::unique_ptr<INode<T,U>>> inodes;
    static inline thread_local std::vector<std::unique_ptr<Leaf<T,U>>> leaves;
    static inline thread_local std::vector<std::unique_ptr<SplitNode<T,U>>> splitnodes;
    static inline thread_local std::vector<std::unique_ptr<ReduceNode<W>>> reducenodes;

    INode<T,U>* make_inode() {
        inodes.push_back(std::make_unique<INode<T,U>>());
        return inodes.back().get();
    }
    Leaf<T,U>* make_leaf() {
        leaves.push_back(std::make_unique<Leaf<T,U>>());
        return leaves.back().get();
    }
    SplitNode<T,U>* make_splitnode() {
        splitnodes.push_back(std::make_unique<SplitNode<T,U>>());
        return splitnodes.back().get();
    }
    ReduceNode<W>* make_reducenode() {
        reducenodes.push_back(std::make_unique<ReduceNode<W>>());
        return reducenodes.back().get();
    }

    void write_lambda(psac::Mod<NodePtr<T,U>>* node_mod, NodePtr<T,U> node) {
        psac_write(node_mod, node);
    }

    // assumes input is sorted and deduped
    // the indices specify the left (inclusive) and right (exclusive) positions for making the tree
    NodePtr<T,U> make_tree(const std::vector<std::pair<T,U>>& vals, size_t l, size_t r) {
        if (r - l <= this->GRAN_LIMIT) {
            if (l == r) return NodePtr<T,U>();
            Leaf<T,U>* leaf = make_leaf();
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = l; i < r; i++) new_vec.push_back(vals[i]);
            assert(new_vec.size() > 0);
            psac_write(&leaf->arr, new_vec);
            return (NodePtr<T,U>(leaf));
        }

        size_t mid = l + (r-l)/2;
        INode<T,U>* node = make_inode();
        psac_write(&node->key, vals[mid].first);
        psac_write(&node->val, vals[mid].second);

        parlay::par_do(
            [&]() { write_lambda(&node->left, make_tree(vals, l, mid)); },
            [&]() { write_lambda(&node->right, make_tree(vals, mid+1, r)); }
        );

        return (NodePtr<T,U>(node));
    }

    size_t sz(NodePtr<T,U> node) {
        if (node.is_leaf()) {
            if (node == BLANK) return 0;
            else return node.get_leaf()->arr.value.size();
        }
        else return 1 + sz(node.get_inode()->left.value) + sz(node.get_inode()->right.value);
    }

    static bool key_equals(std::pair<T,U> pair1, std::pair<T,U> pair2) {
        return pair1.first == pair2.first;
    }

    void batch_insert_to_leaf(psac::Mod<NodePtr<T,U>>* node_mod, const std::vector<std::pair<T,U>>& vals, size_t l, size_t r) {
        NodePtr<T,U> node = node_mod->value;
        assert(node.is_leaf());

        if (node == BLANK) {
            Leaf<T,U>* leaf = make_leaf();
            NodePtr<T,U> res = NodePtr<T,U>(leaf);
            psac_write(node_mod, res);
        }
        node = node_mod->value;
        Leaf<T,U>* leaf = node.get_leaf();
        std::vector<std::pair<T,U>>& vec = leaf->arr.value;

        std::vector<std::pair<T,U>> sorted_vals(vec.size() + (r-l));
        std::merge(vec.begin(), vec.end(), vals.begin() + l, vals.begin() + r, sorted_vals.begin());
        sorted_vals.erase(unique(sorted_vals.begin(), sorted_vals.end(), key_equals), sorted_vals.end());
        size_t num_arrs = (sorted_vals.size() - 1) / this->GRAN_LIMIT + 1;

        if (num_arrs == 1) { // can insert rest of the elements into leaf
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = 0; i < sorted_vals.size(); i++) new_vec.push_back(sorted_vals[i]);
            assert(new_vec.size() > 0);
            psac_write(&leaf->arr, new_vec);
        }
        else { // need to delete the leaf and put a new internal node in its place
            NodePtr<T,U> new_node = make_tree(sorted_vals, 0, sorted_vals.size());
            psac_write(node_mod, new_node);
        }
    }

    void insert(T key, U val) {
        psac::Mod<NodePtr<T,U>>* node_mod = &this->root;
        while (!node_mod->value.is_leaf()) {
            INode<T,U>* inode = node_mod->value.get_inode();
            if (inode->key.value > key) {
                node_mod = &(inode->left);
            }
            else if (inode->key.value < key) {
                node_mod = &(inode->right);
            }
            else return;
        }
        std::vector<std::pair<T,U>> vals{std::make_pair(key, val)};
        batch_insert_to_leaf(node_mod, vals, 0, 1);
    }

    void batch_insert_helper(const std::vector<std::pair<T,U>>& vals, psac::Mod<NodePtr<T,U>>* node_mod, size_t l, size_t r) {
        if (l == r) return;

        if (node_mod->value.is_leaf()) {
            batch_insert_to_leaf(node_mod, vals, l, r);
            return;
        }

        INode<T,U>* inode = node_mod->value.get_inode();
        T node_key = inode->key.value;
        size_t left_idx = r;
        for (size_t i = l; i < r; i++) {
            if (vals[i].first >= node_key) {
                left_idx = i;
                break;
            }
        }

        size_t right_idx = left_idx;
        if (left_idx != r && vals[left_idx].first == node_key) right_idx += 1;

        parlay::par_do(
            [&]() { batch_insert_helper(vals, &inode->left, l, left_idx); },
            [&]() { batch_insert_helper(vals, &inode->right, right_idx, r); }
        );
    }

    void batch_insert(std::vector<std::pair<T,U>>& vals) {
        batch_insert_helper(vals, &this->root, 0, vals.size());
    }

    void create(const std::vector<std::pair<T,U>>& vals) {
        for (size_t i = 0; i < vals.size(); i++) {
            this->insert(vals[i].first, vals[i].second);
        }
    }

    psac_function(join, NodePtr<T,U> l, T key, U val, NodePtr<T,U> r, INode<T,U>* res) {
        psac_write(&res->key, key);
        psac_write(&res->val, val);
        psac_write(&res->left, l);
        psac_write(&res->right, r);
    }

    // assumes that leaf is not null
    psac_function(join_leaf, Leaf<T,U>* leaf, INode<T,U>* final_res) {
        assert(leaf != nullptr);
        psac_read((std::vector<std::pair<T,U>>& leaf_vec), (&leaf->arr), {
            assert(leaf_vec.size() > 0);
            std::pair<T,U> last_val = leaf_vec.back();
            psac_write(&final_res->key, last_val.first);
            psac_write(&final_res->val, last_val.second);
            if (leaf_vec.size() == 1) {
                NodePtr<T,U> res = BLANK;
                psac_write(&final_res->left, res);
            }
            else {
                Leaf<T,U>* new_leaf = make_leaf();
                std::vector<std::pair<T,U>> new_vec;
                new_vec.reserve(this->GRAN_LIMIT);
                for (size_t i = 0; i < leaf_vec.size() - 1; i++) new_vec.push_back(leaf_vec[i]);
                assert(new_vec.size() > 0);
                psac_write(&new_leaf->arr, new_vec);

                NodePtr<T,U> res = NodePtr<T,U>(new_leaf);
                psac_write(&final_res->left, res);
            }
        });
    }

    psac_function(join2, NodePtr<T,U> l, NodePtr<T,U> r, INode<T,U>* res) {
        if (l.is_leaf()) {
            if (l == BLANK) {
                if (r.is_leaf()) {
                    assert(r != BLANK);
                    Leaf<T,U>* rleaf = r.get_leaf();
                    psac_call(join_leaf, rleaf, res);
                    psac_write(&res->right, BLANK);
                }
                else {
                    INode<T,U>* rinode = r.get_inode();
                    psac_read((NodePtr<T,U> rl, T rk, U rv, NodePtr<T,U> rr),
                              (&rinode->left, &rinode->key, &rinode->val, &rinode->right), {
                        psac_call(join, rl, rk, rv, rr, res);
                    });
                }
            }
            else {
                Leaf<T,U>* lleaf = l.get_leaf();
                psac_call(join_leaf, lleaf, res);
                psac_write(&res->right, r);
            }
            return;
        }

        INode<T,U>* right_res = make_inode();
        INode<T,U>* linode = l.get_inode();
        psac_read((NodePtr<T,U> ll, T lk, U lv, NodePtr<T,U> lr),
                  (&linode->left, &linode->key, &linode->val, &linode->right), {
            if (lr == BLANK && r == BLANK) {
                psac_call(join, ll, lk, lv, BLANK, res);
            }
            else {
                psac_call(join2, lr, r, right_res);
                psac_call(join, ll, lk, lv, NodePtr<T,U>(right_res), res);
            }
        });
    }

    psac_function(split_leaf, T key, Leaf<T,U>* leaf, SplitNode<T,U>* res) {
        assert(leaf != nullptr);
        psac_read((std::vector<std::pair<T,U>>& leaf_vec), (&leaf->arr), {
            assert(leaf_vec.size() > 0);
            int idx = -1;
            for (size_t i = 0; i < leaf_vec.size(); i++) {
                if (key <= leaf_vec[i].first) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) { // all elements in the vector are smaller than v
                psac_write(&res->found, false);
                NodePtr<T,U> res_left = NodePtr<T,U>(leaf), res_right = BLANK;
                psac_write(&res->left, res_left);
                psac_write(&res->right, res_right);
                return;
            }

            size_t right_index; // the starting index for the right leaf
            if (leaf_vec[idx].first == key) {
                psac_write(&res->found, true);
                right_index = idx + 1;
            }
            else {
                psac_write(&res->found, false);
                right_index = idx;
            }

            // left leaf
            if (idx > 0) {
                Leaf<T,U>* left_leaf = make_leaf();
                std::vector<std::pair<T,U>> new_vec;
                new_vec.reserve(this->GRAN_LIMIT);
                for (size_t i = 0; i < (size_t)idx; i++) new_vec.push_back(leaf_vec[i]);
                assert(new_vec.size() > 0);
                psac_write(&left_leaf->arr, new_vec);
                NodePtr<T,U> res_left = NodePtr<T,U>(left_leaf);
                psac_write(&res->left, res_left);
            }
            else {
                psac_write(&res->left, BLANK);
            }

            // right leaf
            if ((int)right_index < (int)leaf_vec.size()) {
                Leaf<T,U>* right_leaf = make_leaf();
                std::vector<std::pair<T,U>> new_vec;
                new_vec.reserve(this->GRAN_LIMIT);
                for (size_t i = right_index; i < leaf_vec.size(); i++) new_vec.push_back(leaf_vec[i]);
                assert(new_vec.size() > 0);
                psac_write(&right_leaf->arr, new_vec);
                NodePtr<T,U> res_right = NodePtr<T,U>(right_leaf);
                psac_write(&res->right, res_right);
            }
            else {
                psac_write(&res->right, BLANK);
            }
        });
    }

    psac_function(split, T key, NodePtr<T,U> node, SplitNode<T,U>* final_res) {
        if (node.is_leaf()) {
            if (node == BLANK) {
                psac_write(&final_res->found, false);
                psac_write(&final_res->left, BLANK);
                psac_write(&final_res->right, BLANK);
            }
            else {
                Leaf<T,U>* leaf = node.get_leaf();
                psac_call(split_leaf, key, leaf, final_res);
            }
            return;
        }

        INode<T,U>* inode = node.get_inode();
        psac_read((T k), (&inode->key), {
            if (key == k) {
                psac_write(&final_res->found, true);
                psac_read((NodePtr<T,U> left), (&inode->left), {
                    psac_write(&final_res->left, left);
                });
                psac_read((NodePtr<T,U> right), (&inode->right), {
                    psac_write(&final_res->right, right);
                });
            }
            else if (key < k) {
                SplitNode<T,U>* child = make_splitnode();
                psac_read((NodePtr<T,U> left), (&inode->left), {
                    psac_call(split, key, left, child);
                });

                INode<T,U>* res_inode = make_inode();
                psac_read((NodePtr<T,U> cl, NodePtr<T,U> cr, bool found, NodePtr<T,U> right, U v),
                          (&child->left, &child->right, &child->found, &inode->right, &inode->val), {
                    psac_write(&final_res->left, cl);
                    psac_write(&final_res->found, found);
                    psac_call(join, cr, k, v, right, res_inode);
                });
                NodePtr<T,U> res_right = NodePtr<T,U>(res_inode);
                psac_write(&final_res->right, res_right);
            }
            else {
                SplitNode<T,U>* child = make_splitnode();
                psac_read((NodePtr<T,U> right), (&inode->right), {
                    psac_call(split, key, right, child);
                });

                INode<T,U>* res_inode = make_inode();
                psac_read((NodePtr<T,U> cl, NodePtr<T,U> cr, bool found, NodePtr<T,U> left, U v),
                          (&child->left, &child->right, &child->found, &inode->left, &inode->val), {
                    psac_write(&final_res->right, cr);
                    psac_write(&final_res->found, found);
                    psac_call(join, left, k, v, cl, res_inode);
                });
                NodePtr<T,U> res_left = NodePtr<T,U>(res_inode);
                psac_write(&final_res->left, res_left);
            }
        });
    }

    psac_function(filter_leaf, Leaf<T,U>* leaf, psac::Mod<NodePtr<T,U>>* final_res, const std::function<bool(U)>& f) {
        assert(leaf != nullptr);
        psac_read((std::vector<std::pair<T,U>>& leaf_vec), (&leaf->arr), {
            assert(leaf_vec.size() > 0);
            Leaf<T,U>* new_leaf = make_leaf();
            std::vector<std::pair<T,U>> new_vec;
            new_vec.reserve(this->GRAN_LIMIT);
            for (size_t i = 0; i < leaf_vec.size(); i++) {
                if (f(leaf_vec[i].second)) new_vec.push_back(std::move(leaf_vec[i]));
            }
            if (new_vec.size() == 0) {
                psac_write(final_res, BLANK);
            }
            else {
                psac_write(&new_leaf->arr, new_vec);
                NodePtr<T,U> res = NodePtr<T,U>(new_leaf);
                psac_write(final_res, res);
            }
        });
    }

    psac_function(filter, NodePtr<T,U> node, psac::Mod<NodePtr<T,U>>* final_res, const std::function<bool(U)>& f) {
        if (node.is_leaf()) {
            if (node == BLANK) {
                psac_write(final_res, BLANK);
            }
            else {
                Leaf<T,U>* leaf = node.get_leaf();
                psac_call(filter_leaf, leaf, final_res, f);
            }
            return;
        }

        INode<T,U>* new_res = make_inode();
        INode<T,U>* inode = node.get_inode();
        psac_par(
            psac_read((NodePtr<T,U> left), (&inode->left), {
                psac_call(filter, left, &new_res->left, f);
            }),
            psac_read((NodePtr<T,U> right), (&inode->right), {
                psac_call(filter, right, &new_res->right, f);
            })
        );

        INode<T,U>* node_res = make_inode();
        psac_read((T key, U val, NodePtr<T,U> left, NodePtr<T,U> right),
                  (&inode->key, &inode->val, &new_res->left, &new_res->right), {
            NodePtr<T,U> write_res = BLANK;
            if (f(val)) {
                psac_call(join, left, key, val, right, node_res);
                write_res = NodePtr<T,U>(node_res);
            }
            else {
                if (left != BLANK || right != BLANK) {
                    psac_call(join2, left, right, node_res);
                    write_res = NodePtr<T,U>(node_res);
                }
            }
            psac_write(final_res, write_res);
        });
    }

    psac_function(mapreduce_leaf, Leaf<T,U>* leaf, psac::Mod<W>* res, W base,
                  const std::function<W(U)>& m, const std::function<W(W, W)>& r) {
        assert(leaf != nullptr);
        psac_read((std::vector<std::pair<T,U>>& leaf_vec), (&leaf->arr), {
            assert(leaf_vec.size() > 0);
            W total = base;
            for (size_t i = 0; i < leaf_vec.size(); i++) total = r(total, m(leaf_vec[i].second));
            psac_write(res, total);
        });
    }

    psac_function(mapreduce, NodePtr<T,U> node, ReduceNode<W>* res, W base,
                  const std::function<W(U)>& m, const std::function<W(W, W)>& r) {
        if (node.is_leaf()) {
            if (node == BLANK) {
                psac_write(&res->val, base);
            }
            else {
                Leaf<T,U>* leaf = node.get_leaf();
                psac_call(mapreduce_leaf, leaf, &res->val, base, m, r);
            }
            psac_write(&res->left, nullptr);
            psac_write(&res->right, nullptr);
            return;
        }

        ReduceNode<W>* res_left = make_reducenode();
        ReduceNode<W>* res_right = make_reducenode();

        INode<T,U>* inode = node.get_inode();
        psac_par(
            psac_read((NodePtr<T,U> left), (&inode->left), {
                psac_call(mapreduce, left, res_left, base, m, r);
                psac_write(&res->left, res_left);
            }),
            psac_read((NodePtr<T,U> right), (&inode->right), {
                psac_call(mapreduce, right, res_right, base, m, r);
                psac_write(&res->right, res_right);
            })
        );

        psac_read((W resl, U val, W resr), (&res_left->val, &inode->val, &res_right->val), {
            W final_res = r(r(resl, m(val)), resr);
            psac_write(&res->val, final_res);
        });
    }

    psac_function(filtermapreduce, NodePtr<T,U> node, ReduceNode<W>* res, W base,
                  const std::function<bool(U)>& f,
                  const std::function<W(U)>& m,
                  const std::function<W(W, W)>& r) {

        psac::Mod<NodePtr<T,U>> res_filter;
        psac_call(filter, node, &res_filter, f);
        psac_call(mapreduce, res_filter.value, res, base, m, r);
    }

};