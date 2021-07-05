#include <vector>
#include <utility>
#include <cmath>
#include <deque>

#include "psac/examples/convex-hull-primitives.hpp"
#include "psac/examples/bst-primitives.hpp"
#include "psac/examples/bst.hpp"
#include <psac/psac.hpp>

const int INF = 1000000000;

class LCHull{
public:

    struct NodeInfo {
        Point bl, br; // left and right end points of the bridge
        Point bottom; // bottommost point in node
        NodeInfo() = default;
    };

    NodeInfo* make_node() {
        this->nodes.push_back(std::make_unique<NodeInfo>());
        return this->nodes.back().get();
    }

    long static cross (Point a, Point b, Point c) {
        return (b - a).cross(c - a);
    }

    static inline thread_local std::vector<std::unique_ptr<NodeInfo>> nodes;
    psac::Computation comp;
    size_t GRAN_LIMIT;
    std::unique_ptr<Bst<Point, void*>> bst;
    std::unique_ptr<Bst<Point, NodeInfo*>> bstres;

    typedef NodePtr<Point, void*> InputNode;
    typedef NodePtr<Point, NodeInfo*> OutputNode;

    // assume already sorted by y coord
    LCHull(std::vector<std::pair<int,int>> ps, size_t gran_limit) {
        this->bst = std::make_unique<Bst<Point, void*>>(gran_limit);
        this->bstres = std::make_unique<Bst<Point, NodeInfo*>>(gran_limit);
        this->GRAN_LIMIT = gran_limit;
        int s = ps.size();
        std::vector<std::pair<Point, void*>> points;
        for (int i = 0; i < s; i++) {
            Point p { ps[i].first, ps[i].second };
            points.push_back(std::make_pair(p, nullptr));
        }

        this->bst->batch_insert(points);
    }

    psac_function(build) {
        psac_read((InputNode bst_root), (&this->bst->root), {
            psac_call(init_bridges, bst_root, &this->bstres->root);
        });
    }

    void bridge_cases_leaf (std::vector<std::pair<Point, NodeInfo*>>& vec,
                  int ll, int lr, int rl, int rr, int split_y, int idx) {

        // single vertex inside the leaf on both traversals
        if (ll + 1 == lr && rl + 1 == rr) {
            vec[idx].second->bl = vec[ll].first;
            vec[idx].second->br = vec[rl].first;
            return;
        }

        int lmid = ll + (lr - ll) / 2;
        int rmid = rl + (rr - rl) / 2;

        // if ll + 1 == lr then we are already at the leaf, and the bridge values are for some other range
        Point a = (ll + 1 == lr) ? vec[ll].first : vec[lmid].second->bl;
        Point b = (ll + 1 == lr) ? vec[ll].first : vec[lmid].second->br;
        Point c = (rl + 1 == rr) ? vec[rl].first : vec[rmid].second->bl;
        Point d = (rl + 1 == rr) ? vec[rl].first : vec[rmid].second->br;
        if (a != b && cross(a, b, c) > 0) bridge_cases_leaf(vec, ll, lmid, rl, rr, split_y, idx);
        else if (c != d && cross(b, c, d) > 0) bridge_cases_leaf(vec, ll, lr, rmid, rr, split_y, idx);
        else if (a == b) bridge_cases_leaf(vec, ll, lr, rl, rmid, split_y, idx);
        else if (c == d) bridge_cases_leaf(vec, lmid, lr, rl, rr, split_y, idx);
        else {
            long s1 = cross(a, b, c); // must be non-positive if we reached this case
            long s2 = cross(b, a, d); // must be non-negative if we reached this case
            assert(s1 + s2 >= 0); // sum is non-negative because a, b, c, d are clockwise.

            // extend the lines ab and cd and check if they meet above split_y
            if (s1 + s2 == 0 ||
                s1 * d.y + s2 * c.y < split_y * (s1 + s2)) bridge_cases_leaf(vec, lmid, lr, rl, rr, split_y, idx);
            else bridge_cases_leaf(vec, ll, lr, rl, rmid, split_y, idx);
        }
    }

    void init_bridges_leaf (std::vector<std::pair<Point, void*>>& vec, int l, int r,
                  std::vector<std::pair<Point, NodeInfo*>>& res_vec) {
        if (l + 1 == r) {
            res_vec[l].first = vec[l].first;
            res_vec[l].second = make_node();
            res_vec[l].second->bl = res_vec[l].second->br = res_vec[l].second->bottom = vec[l].first;
            return;
        }
        int mid = l + (r - l) / 2;
        init_bridges_leaf(vec, l, mid, res_vec);
        init_bridges_leaf(vec, mid, r, res_vec);
        int rmid = mid + (r - mid) / 2;
        int split_y = res_vec[rmid].second->bottom.y;
        bridge_cases_leaf(res_vec, l, mid, mid, r, split_y, mid);
    }

    // bridge_cases and find_bridge_helper are mutually recursive:
    // bridge_cases does the bridge finding logic, and find_bridge_helper
    // gets the children of the current node to get proper next recursive call
    // for bridge_cases
    psac_function(bridge_cases, Point a, Point b, Point c, Point d, INode<Point, NodeInfo*>* inode, Point old_bottom,
                  OutputNode left, OutputNode right, int ll, int lr, int rl, int rr, int split_y, bool left_is_final, bool right_is_final) {
        assert(a != c);
        assert(b != d);
        assert(a != Point() && b != Point() && c != Point() && d != Point());
        assert(left != right);
        // if we call bridge on two singleton points then just go back to find_bridge_helper
        if (a == b && c == d) {
//            std::cout<<"case0"<<std::endl;
            psac_call(find_bridge_helper, inode, old_bottom, left, right,
                      ll, lr, rl, rr, split_y, true, true);
            return;
        }
        if (a != b && cross(a, b, c) > 0) {
//            std::cout<<"case1"<<std::endl;
            if (left.is_leaf()) {
                int lmid = ll + (lr - ll) / 2;
                psac_call(find_bridge_helper, inode, old_bottom, left, right, 
                          ll, lmid, rl, rr, split_y, (lmid - ll <= 1), right_is_final);
            }
            else {
                auto linode = left.get_inode();
                assert(linode != nullptr);
                psac_read((OutputNode lcl), (&linode->left), {
                    if (lcl.is_leaf() && lcl.get_leaf() == nullptr) {
                        psac_call(find_bridge_helper, inode, old_bottom, left, right,
                                  ll, lr, rl, rr, split_y, true, right_is_final);
                    }
                    else {
                        psac_call(find_bridge_helper, inode, old_bottom, lcl, right, 
                                  ll, lr, rl, rr, split_y, left_is_final, right_is_final);
                    }
                });
            }
        }
        else if (c != d && cross(b, c, d) > 0) {
//            std::cout<<"case2"<<std::endl;
            if (right.is_leaf()) {
                int rmid = rl + (rr - rl) / 2;
                psac_call(find_bridge_helper, inode, old_bottom, left, right, 
                          ll, lr, rmid, rr, split_y, left_is_final, (rr - rmid <= 1));
            }
            else {
                auto rinode = right.get_inode();
                assert(rinode != nullptr);
                psac_read((OutputNode rcr), (&rinode->right), {
                    if (rcr.is_leaf() && rcr.get_leaf() == nullptr) {
                        psac_call(find_bridge_helper, inode, old_bottom, left, right,
                                  ll, lr, rl, rr, split_y, left_is_final, true);
                    }
                    else {
                        psac_call(find_bridge_helper, inode, old_bottom, left, rcr,
                                  ll, lr, rl, rr, split_y, left_is_final, right_is_final);
                    }
                });
            }
        }
        else if (a == b) {
//            std::cout<<"case3"<<std::endl;
            if (right.is_leaf()) {
                int rmid = rl + (rr - rl) / 2;
                psac_call(find_bridge_helper, inode, old_bottom, left, right,
                          ll, lr, rl, rmid, split_y, left_is_final, (rmid - rl <= 1));
            }
            else {
                auto rinode = right.get_inode();
                assert(rinode != nullptr);
                psac_read((OutputNode rcl), (&rinode->left), {
                    if (rcl.is_leaf() && rcl.get_leaf() == nullptr) {
                        psac_call(find_bridge_helper, inode, old_bottom, left, right,
                                  ll, lr, rl, rr, split_y, left_is_final, true);
                    }
                    else {
                        psac_call(find_bridge_helper, inode, old_bottom, left, rcl,
                                  ll, lr, rl, rr, split_y, left_is_final, right_is_final);
                    }
                });
            }
        }
        else if (c == d) {
//            std::cout<<"case4"<<std::endl;
            if (left.is_leaf()) {
                int lmid = ll + (lr - ll) / 2;
                psac_call(find_bridge_helper, inode, old_bottom, left, right,
                          lmid, lr, rl, rr, split_y, (lmid - ll <= 1), right_is_final);
            }
            else {
                auto linode = left.get_inode();
                assert(linode != nullptr);
                psac_read((OutputNode lcr), (&linode->right), {
                    if (lcr.is_leaf() && lcr.get_leaf() == nullptr) {
                        psac_call(find_bridge_helper, inode, old_bottom, left, right,
                                  ll, lr, rl, rr, split_y, true, right_is_final);
                    }
                    else {
                        psac_call(find_bridge_helper, inode, old_bottom, lcr, right,
                                  ll, lr, rl, rr, split_y, left_is_final, right_is_final);
                    }
                });
            }
        }
        else {
            long s1 = cross(a, b, c); // must be non-positive if we reached this case
            long s2 = cross(b, a, d); // must be non-negative if we reached this case
            assert(s1 + s2 >= 0); // sum is non-negative because a, b, c, d are clockwise.

            // extend the lines ab and cd and check if they meet above split_y
            if (s1 + s2 == 0 ||
                s1 * d.y + s2 * c.y < split_y * (s1 + s2)) {
//                std::cout<<"case5"<<std::endl;
                if (left.is_leaf()) {
                    int lmid = ll + (lr - ll) / 2;
                    psac_call(find_bridge_helper, inode, old_bottom, left, right,
                              lmid, lr, rl, rr, split_y, (lmid - ll <= 1), right_is_final);
                }
                else {
                    auto linode = left.get_inode();
                    assert(linode != nullptr);
                    psac_read((OutputNode lcr), (&linode->right), {
                        if (lcr.is_leaf() && lcr.get_leaf() == nullptr) {
                            psac_call(find_bridge_helper, inode, old_bottom, left, right,
                                      ll, lr, rl, rr, split_y, true, right_is_final);
                        }
                        else {
                            psac_call(find_bridge_helper, inode, old_bottom, lcr, right,
                                      ll, lr, rl, rr, split_y, left_is_final, right_is_final);
                        }
                    });
                }
            }
            else {
//                std::cout<<"case6"<<std::endl;
                if (right.is_leaf()) {
                    int rmid = rl + (rr - rl) / 2;
                    psac_call(find_bridge_helper, inode, old_bottom, left, right,
                              ll, lr, rl, rmid, split_y, left_is_final, (rmid - rl <= 1));
                }
                else {
                    auto rinode = right.get_inode();
                    assert(rinode != nullptr);
                    psac_read((OutputNode rcl), (&rinode->left), {
                        if (rcl.is_leaf() && rcl.get_leaf() == nullptr) {
                            psac_call(find_bridge_helper, inode, old_bottom, left, right,
                                      ll, lr, rl, rr, split_y, left_is_final, true);
                        }
                        else {
                            psac_call(find_bridge_helper, inode, old_bottom, left, rcl,
                                      ll, lr, rl, rr, split_y, left_is_final, right_is_final);
                        }
                    });
                }
            }
        }
    }

    psac_function(find_bridge_helper, INode<Point, NodeInfo*>* inode, Point old_bottom,
                  OutputNode left, OutputNode right, int ll, int lr, int rl, int rr, int split_y,
                  bool left_is_final, bool right_is_final) {
        // if both are leaves and the binary search on the leaves has ended
        assert(left != right);
        NodeInfo* new_info = make_node();
        new_info->bottom = old_bottom;
        psac_write(&inode->val, new_info);
        if (left_is_final && right_is_final) {
            if (left.is_leaf() && right.is_leaf()) {
                auto lleaf = left.get_leaf();
                auto rleaf = right.get_leaf();
                assert(lleaf != nullptr);
                assert(rleaf != nullptr);
                psac_read((std::vector<std::pair<Point, NodeInfo*>>& lvec), (&lleaf->arr), { new_info->bl = lvec[ll].first; });
                psac_read((std::vector<std::pair<Point, NodeInfo*>>& rvec), (&rleaf->arr), { new_info->br = rvec[rl].first; });
            }
            else if (left.is_leaf() && !right.is_leaf()) {
                auto lleaf = left.get_leaf();
                auto rinode = right.get_inode();
                assert(lleaf != nullptr);
                psac_read((std::vector<std::pair<Point, NodeInfo*>>& lvec), (&lleaf->arr), { new_info->bl = lvec[ll].first; });
                psac_read((Point rp), (&rinode->key), { new_info->br = rp; });
            }
            else if (!left.is_leaf() && right.is_leaf()) {
                auto linode = left.get_inode();
                auto rleaf = right.get_leaf();
                assert(rleaf != nullptr);
                psac_read((Point lp), (&linode->key), { new_info->bl = lp; });
                psac_read((std::vector<std::pair<Point, NodeInfo*>>& rvec), (&rleaf->arr), { new_info->br = rvec[rl].first; });
            }
            else {
                auto linode = left.get_inode();
                auto rinode = right.get_inode();
                psac_read((Point lp), (&linode->key), { new_info->bl = lp; });
                psac_read((Point rp), (&rinode->key), { new_info->br = rp; });
            }
            return;
        }

        // 4 different cases depending on whether left and right nodes are leaf are not
        if (left.is_leaf() && right.is_leaf()) {
//            std::cout<<"both leaves"<<std::endl;
            auto lleaf = left.get_leaf();
            auto rleaf = right.get_leaf();
            assert(lleaf != nullptr);
            assert(rleaf != nullptr);
            psac_read((std::vector<std::pair<Point, NodeInfo*>>& lvec, std::vector<std::pair<Point, NodeInfo*>>& rvec),
                      (&lleaf->arr, &rleaf->arr), {
                int newll = ll, newlr = lr, newrl = rl, newrr = rr;
                if (ll == -1) {
                    newll = 0;
                    newlr = lvec.size();
                }
                if (rl == -1) {
                    newrl = 0;
                    newrr = rvec.size();
                }
                int lmid = newll + (newlr - newll) / 2;
                int rmid = newrl + (newrr - newrl) / 2;
                Point a = (newll + 1 == newlr) ? lvec[newll].first : lvec[lmid].second->bl;
                Point b = (newll + 1 == newlr) ? lvec[newll].first : lvec[lmid].second->br;
                Point c = (newrl + 1 == newrr) ? rvec[newrl].first : rvec[rmid].second->bl;
                Point d = (newrl + 1 == newrr) ? rvec[newrl].first : rvec[rmid].second->br;
                psac_call(bridge_cases, a, b, c, d, inode, old_bottom, left, right,
                          newll, newlr, newrl, newrr, split_y, left_is_final, right_is_final);
            });
        }
        else if (left.is_leaf() && !right.is_leaf()) {
//            std::cout<<"only left leaf"<<std::endl;
            auto lleaf = left.get_leaf();
            auto rinode = right.get_inode();
            assert(lleaf != nullptr);
            psac_read((std::vector<std::pair<Point, NodeInfo*>>& lvec, NodeInfo* rinfo, Point rkey),
                      (&lleaf->arr, &rinode->val, &rinode->key), {
                int newll = ll, newlr = lr, newrl = rl, newrr = rr;
                if (ll == -1) {
                    newll = 0;
                    newlr = lvec.size();
                }
                int lmid = newll + (newlr - newll) / 2;
                Point a = (newll + 1 == newlr) ? lvec[newll].first : lvec[lmid].second->bl;
                Point b = (newll + 1 == newlr) ? lvec[newll].first : lvec[lmid].second->br;
                Point c = right_is_final ? rkey : rinfo->bl;
                Point d = right_is_final ? rkey : rinfo->br;
                psac_call(bridge_cases, a, b, c, d, inode, old_bottom, left, right,
                          newll, newlr, newrl, newrr, split_y, left_is_final, right_is_final);
            });
        }
        else if (!left.is_leaf() && right.is_leaf()) {
//            std::cout<<"only right leaf"<<std::endl;
            auto linode = left.get_inode();
            auto rleaf = right.get_leaf();
            assert(rleaf != nullptr);
            psac_read((NodeInfo* linfo, Point lkey, std::vector<std::pair<Point, NodeInfo*>>& rvec),
                      (&linode->val, &linode->key, &rleaf->arr), {
                int newll = ll, newlr = lr, newrl = rl, newrr = rr;
                if (rl == -1) {
                    newrl = 0;
                    newrr = rvec.size();
                }
                int rmid = newrl + (newrr - newrl) / 2;
                Point a = left_is_final ? lkey : linfo->bl;
                Point b = left_is_final ? lkey : linfo->br;
                Point c = (newrl + 1 == newrr) ? rvec[newrl].first : rvec[rmid].second->bl;
                Point d = (newrl + 1 == newrr) ? rvec[newrl].first : rvec[rmid].second->br;
                psac_call(bridge_cases, a, b, c, d, inode, old_bottom, left, right,
                          newll, newlr, newrl, newrr, split_y, left_is_final, right_is_final);
            });
        }
        else {
//            std::cout<<"neither leaves"<<std::endl;
            auto linode = left.get_inode();
            auto rinode = right.get_inode();
            psac_read((NodeInfo* linfo, Point lkey, NodeInfo* rinfo, Point rkey),
                      (&linode->val, &linode->key, &rinode->val, &rinode->key), {
                Point a = left_is_final ? lkey : linfo->bl;
                Point b = left_is_final ? lkey : linfo->br;
                Point c = right_is_final ? rkey : rinfo->bl;
                Point d = right_is_final ? rkey : rinfo->br;
                psac_call(bridge_cases, a, b, c, d, inode, old_bottom, left, right,
                          ll, lr, rl, rr, split_y, left_is_final, right_is_final);
            });
        }
    }

    // dir is true if node is left child of parent and false otherwise
    psac_function(init_bridges, InputNode node, psac::Mod<OutputNode>* res) {
        if (node.is_leaf()) {
            auto leaf = node.get_leaf();
            if (leaf == nullptr) {
                OutputNode final_res = NodePtr<Point, NodeInfo*>();
                psac_write(res, final_res);
                return;
            }
            Leaf<Point, NodeInfo*>* output_leaf = this->bstres->make_leaf();
            psac_read((std::vector<std::pair<Point, void*>>& vec), (&leaf->arr), {
//                std::cout<<vec[0].first.x<<vec[0].first.y<<std::endl;
                std::vector<std::pair<Point, NodeInfo*>> res_vec;
                res_vec.reserve(this->GRAN_LIMIT);
                res_vec.resize(vec.size());
                init_bridges_leaf(vec, 0, vec.size(), res_vec);
                psac_write(&output_leaf->arr, res_vec);
                OutputNode final_res = NodePtr<Point, NodeInfo*>(output_leaf);
                psac_write(res, final_res);
            });
        }
        else {
            INode<Point, void*>* inode = node.get_inode();
            INode<Point, NodeInfo*>* tmp_res = this->bstres->make_inode();
            psac_par(
                psac_read((InputNode lnode), (&inode->left), {
                    psac_call(init_bridges, lnode, &tmp_res->left);
                }),
                psac_read((InputNode rnode), (&inode->right), {
                    psac_call(init_bridges, rnode, &tmp_res->right);
                }
            ));

            INode<Point, NodeInfo*>* res_node = this->bstres->make_inode();
            psac_read((Point p, OutputNode left, OutputNode right),
                      (&inode->key, &tmp_res->left, &tmp_res->right), {
//                std::cout<<p.x<<p.y<<std::endl;

                Point dummyleft = Point{INF, p.y}; // when the single node is to the left of the new node
                Point dummyright = Point{INF, p.y - 1}; // when the single node is to the right of the new node

                // create new vertex for the key/val pair at the inode
                Leaf<Point, NodeInfo*>* output_singleton = this->bstres->make_leaf();
                NodeInfo* output_singleton_info = make_node();
                output_singleton_info->bl = output_singleton_info->br = output_singleton_info->bottom = p;
                std::vector<std::pair<Point, NodeInfo*>> output_singleton_arr = {std::make_pair(p, output_singleton_info)};
                psac_write(&output_singleton->arr, output_singleton_arr);
                OutputNode output_single = NodePtr<Point, NodeInfo*>(output_singleton);

                // combine the singleton and the result of the left child, if the left child is not null
                // if the left child is null then we can directly combine the singleton and the right child
                if (left.is_leaf() && left.get_leaf() == nullptr) {
//                    std::cout<<"left null"<<std::endl;
                    if (right.is_leaf()) {
                        assert(right.get_leaf() != nullptr); // both left and right children cannot be null
                        psac_read((std::vector<std::pair<Point, NodeInfo*>>& vec), (&right.get_leaf()->arr), {
                            psac_call(find_bridge_helper, res_node, p, output_single, right,
                                      0, 1, 0, vec.size(), vec[vec.size()/2].second->bottom.y, true, false);
                        });
                    }
                    else {
                        psac_read((NodeInfo* right_info), (&right.get_inode()->val), {
                            psac_call(find_bridge_helper, res_node, p, output_single, right,
                                      0, 1, -1, -1, right_info->bottom.y, true, false);
                        });
                    }
                    psac_write(&res_node->left, output_single);
                    psac_write(&res_node->right, right);
                    psac_write(&res_node->key, dummyleft);
                }

                // if the right child is null then we can directly combine the left child and the singleton into the inode
                else if (right.is_leaf() && right.get_leaf() == nullptr) {
//                    std::cout<<"right null"<<std::endl;
                    if (left.is_leaf()) {
                        assert(left.get_leaf() != nullptr); // both left and right children cannot be null
                        psac_read((std::vector<std::pair<Point, NodeInfo*>>& vec), (&left.get_leaf()->arr), {
                                psac_call(find_bridge_helper, res_node, vec[vec.size()/2].second->bottom, left, output_single,
                                          0, vec.size(), 0, 1, p.y, false, true);
                        });
                    }
                    else {
                        psac_read((NodeInfo* left_info), (&left.get_inode()->val), {
                                psac_call(find_bridge_helper, res_node, left_info->bottom, left, output_single,
                                          -1, -1, 0, 1, p.y, false, true);
                        });
                    }
                    psac_write(&res_node->left, left);
                    psac_write(&res_node->right, output_single);
                    psac_write(&res_node->key, dummyright);
                }

                else {
//                    std::cout<<"neither null"<<std::endl;
                    // left right are not null so combine left and single into s1, then s1 and right into inode
                    INode<Point, NodeInfo*>* s1 = this->bstres->make_inode();

                    if (left.is_leaf()) {
                        assert(left.get_leaf() != nullptr); // both left and right children cannot be null
                        psac_read((std::vector<std::pair<Point, NodeInfo*>>& vec), (&left.get_leaf()->arr), {
                                psac_call(find_bridge_helper, s1, vec[vec.size()/2].second->bottom, left, output_single,
                                          0, vec.size(), 0, 1, p.y, false, true);
                        });
                    }
                    else {
                        psac_read((NodeInfo* left_info), (&left.get_inode()->val), {
                                psac_call(find_bridge_helper, s1, left_info->bottom, left, output_single,
                                          -1, -1, 0, 1, p.y, false, true);
                        });
                    }

                    psac_write(&s1->left, left);
                    psac_write(&s1->right, output_single);
                    psac_write(&s1->key, dummyright);
                    OutputNode s1_root = NodePtr<Point, NodeInfo*>(s1);

                    psac_read((NodeInfo* s1_info), (&s1->val), {
                        if (right.is_leaf()) {
                            psac_read((std::vector<std::pair<Point, NodeInfo*>> & vec), (&right.get_leaf()->arr), {
                                    psac_call(find_bridge_helper, res_node, s1_info->bottom, s1_root, right,
                                              -1, -1, 0, vec.size(), vec[0].first.y, false, false);
                            });
                        } else {
                            psac_read((NodeInfo * right_info), (&right.get_inode()->val), {
                                    psac_call(find_bridge_helper, res_node, s1_info->bottom, s1_root, right,
                                              -1, -1, -1, -1, right_info->bottom.y, false, false);
                            });
                        }
                    });

                    psac_write(&res_node->left, s1_root);
                    psac_write(&res_node->right, right);
                    psac_write(&res_node->key, dummyleft);
                }

                NodePtr<Point, NodeInfo*> res_write = NodePtr<Point, NodeInfo*>(res_node);
                psac_write(res, res_write);
            });
        }
    }

    // dir is true if going left, false otherwise
    Point query_helper(OutputNode node, int y, bool dir, Point acc) {
        if (node.is_leaf()) {
            Point res = acc;
            assert(node.get_leaf() != nullptr);
            std::vector<std::pair<Point, NodeInfo*>>& vec = node.get_leaf()->arr.value;
            int l = 0, r = vec.size();
            while (l + 1 < r) {
                int mid = l + (r - l) / 2;
                if (dir) {
                    if (vec[mid].second->bl.y <= y) return vec[mid].second->bl;
                    else r = mid;
                }
                else {
                    if (y < vec[mid].second->br.y) return res;
                    else {
                        res = vec[mid].second->br;
                        l = mid;
                    }
                }
            }
            if (dir) {
                if (vec[l].second->bl.y <= y) return vec[l].second->bl;
            }
            else {
                if (y < vec[l].second->br.y) return res;
                else res = vec[l].second->br;
            }
            return res;
        }

        auto inode = node.get_inode();
        NodeInfo* info = inode->val.value;
        if (dir) {
            if (info->bl.y <= y) return info->bl;
            else return query_helper(inode->left.value, y, dir, acc);
        }
        else {
            if (y < info->br.y) return acc;
            else return query_helper(inode->right.value, y, dir, info->br);
        }
    }

    Point query(int y) {
        OutputNode node = this->bstres->root.value;
        NodeInfo* info;
        if (node.is_leaf()) {
            assert(node.get_leaf() != nullptr);
            std::vector<std::pair<Point, NodeInfo*>>& vec = node.get_leaf()->arr.value;
            info = vec[vec.size() / 2].second;
        }
        else info = node.get_inode()->val.value;

        if (info->bl.y <= y && y < info->br.y) return info->bl;
        if (y < info->bl.y) return query_helper(node, y, true, Point());
        else return query_helper(node, y, false, Point());
    }

    void batch_insert(std::vector<Point>& ps) {
        std::vector<std::pair<Point, void*>> points;
        for (size_t i = 0; i < ps.size(); i++) points.push_back(std::make_pair(ps[i], nullptr));
        this->bst->batch_insert(points);
    }
};