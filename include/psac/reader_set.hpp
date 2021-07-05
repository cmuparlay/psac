
#ifndef PSAC_CONCURRENT_SET_HPP_
#define PSAC_CONCURRENT_SET_HPP_

#include <atomic>
#include <vector>

namespace psac {

constexpr int READER_TREE_GRANULARITY = 1024;

// A hybrid concurrent data structure for storing readers
// of a modifiable.
// If there is only a single reader, the reader is stored
// inline, and no heap memory is used. If there are multiple
// readers, the data structure will convert into a tree.
//
// Supports:
//   insert(reader): Add a new reader to the set
//   remove(reader): Remove (lazily) a reader from the set
//   for_all(f): Execute f on each reader in the set
//
// insert and remove can be called concurrently with themselves
// and each other, but neither should be called concurrently
// with for_all.
//
// Template Parameter <T>: Read node type
template<typename T>
struct ConcurrentReaderSet {

  // A tree node
  struct Node {
    Node(T* _value) : key(hash(_value)), value(_value),
                      left(nullptr), right(nullptr), size(1), deleted(false) { }

    size_t key;                       // The hash value of the node pointer
    T* value;                         // The read node pointer
    std::atomic<Node*> left, right;  
    size_t size;                      // Subtree size (computed during for_all -- not maintained always)
    bool deleted;                     // TODO: Pack this bit into the pointer? :D

    // Destroy subtrees in parallel if sizes have been computed
    // and are large (sizes may not have been computed, in which
    // case destruction will happen sequentially).
    ~Node() {
      auto l = left.load(std::memory_order_relaxed);
      auto r = right.load(std::memory_order_relaxed);
      if (l != nullptr && r != nullptr) {
        if (size >= 1024) {
          parlay::par_do(
            [&]() { delete l; },
            [&]() { delete r; }
          );
        }
        else {
          delete l;
          delete r;
        }
      }
      else if (l) {
        delete l;
      }
      else if (r) {
        delete r;
      }
    }
  };

  ConcurrentReaderSet() : ptr_value(0) { }

  // Convert between pointers and integer types for bitwise operations
  template<typename P>
  static inline uintptr_t as_int(P* ptr) { return reinterpret_cast<uintptr_t>(ptr); }
  static inline Node* as_ptr(uintptr_t val) { return reinterpret_cast<Node*>(val); }

  static inline Node* clean_ptr(Node* ptr) { return as_ptr(as_int(ptr) & uintptr_t(~1)); }
  static inline Node* mask_ptr(Node* ptr) { return as_ptr(as_int(ptr) | uintptr_t(1)); }

  static size_t hash(T* value) {
    return hash64(as_int(value));
  }

  // Check whether the set is currently empty. Will perform all pending
  // lazy deletions and hence is not thread safe with other operations
  bool empty() {
    for_all([](auto) { });
    return ptr_value.load() == 0;
  }

  __attribute__((no_sanitize("integer")))
  static inline uint64_t hash64(uint64_t u ) {
    uint64_t v = u * 3935559000370003845ul + 2691343689449507681ul;
    v ^= v >> 21;
    v ^= v << 37;
    v ^= v >>  4;
    v *= 4768777513237032717ul;
    v ^= v << 20;
    v ^= v >> 41;
    v ^= v <<  5;
    return v;
  }

  // Create a new tree node, returning a
  // pointer to it with its 1 bit set
  Node* make_root(T* value) {
    Node* node = new Node(value);
    return mask_ptr(node);
  }

  // Add an element into the set. Multiple adds can safely happen
  // concurrently with removes, but should not happen concurrently
  // with reads (for_all).
  void insert(T* value) {
    auto curr = ptr_value.load();
    // Reader slot is empty -- value becomes the single reader
    if (curr == 0) {
      insert_single(value);
    }

    // Reader slot contains a single reader -- we try to convert it
    // into a tree
    else if ((curr & 1) == 0) {
      T* head_item = single_item;
      Node* node = make_root(head_item);

      // If we succeed at converting it into a tree, insert value
      // into the tree
      if (ptr_value.compare_exchange_strong(curr, as_int(node))) {
        insert_tree(value);
      }

      // If we fail to convert it into a tree, either someone else
      // did it first, or the single element was deleted
      else {
        delete clean_ptr(node);

        // Someone else converted it into a tree first
        if ((curr & 1) != 0) {
          insert_tree(value);
        }
        
        // The single element was deleted
        else {
          insert_single(value);
        }
      }
    }

    // Reader slot is a tree
    else {
      insert_tree(value);
    }
  }

  // Insert a single reader into a currently empty reader set
  // The single reader is stored inline, with no dynamic allocation
  // necessary. If another reader beats us and becomes the single
  // reader first, we will convert the data structure into a tree
  void insert_single(T* value) {
    assert(value != nullptr);
    uintptr_t curr = 0;

    // If we lose, someone else inserted a single reader first
    if (!ptr_value.compare_exchange_strong(curr, as_int(value), std::memory_order_relaxed)) {
      
      // Only one item has been inserted -- convert to a tree
      if ((curr & 1) == 0) {
        T* head_item = single_item;
        Node* node = make_root(head_item);

        // Someone else converted the single reader to a tree already
        if (!ptr_value.compare_exchange_strong(curr, as_int(node), std::memory_order_relaxed)) {
          delete clean_ptr(node);
        }

        insert_tree(value);
      }

      // Multiple items have been inserted so the data structure is already a tree
      else {
        insert_tree(value);
      }
    }
  }

  // Insert a reader into the set when it is in a tree state, i.e. when
  // there are multiple readers already registered.
  void insert_tree(T* value) {
    assert(value != nullptr);
    Node* new_node = new Node(value);
    Node* curr_node = clean_ptr(root);

    while (true) {
      if (new_node->key <= curr_node->key) {
        if (curr_node->left.load(std::memory_order_relaxed) == nullptr) {
          Node* left = nullptr;
          if (!curr_node->left.compare_exchange_strong(left, new_node, std::memory_order_relaxed)) {
            curr_node = left;
          }
          else {
            return;
          }
        }
        else {
          curr_node = curr_node->left.load(std::memory_order_relaxed);
        }
      }
      else {
        if (curr_node->right.load(std::memory_order_relaxed) == nullptr) {
          Node* right = nullptr;
          if (!curr_node->right.compare_exchange_strong(right, new_node, std::memory_order_relaxed)) {
            curr_node = right;
          }
          else {
            return;
          }
        }
        else {
          curr_node = curr_node->right.load(std::memory_order_relaxed);
        }
      }
    }
  }

  // Remove the given reader from the set
  void remove(T* value) {
    assert(value != nullptr);
    auto curr = ptr_value.load(std::memory_order_relaxed);
    assert(curr != 0);

    // Reader slot is a single reader -- just set it to null
    if ((curr & 1) == 0) {
      assert(curr == as_int(value));

      // Someone else converted the single reader to a tree first
      if (!ptr_value.compare_exchange_strong(curr, 0, std::memory_order_relaxed)) {
        remove_tree(value);
      }
    }
    // Reader slot is a tree
    else {
      remove_tree(value);
    }
  }

  // Remove the given value (lazily) from the set. The node
  // will actually be removed the next time the tree is
  // traversed. This operation is thread safe with add
  // and other removes, but not with reads (for_all).
  void remove_tree(T* value) {
    Node* node = clean_ptr(root);
    size_t key = hash(value);

    while (node != nullptr) {
      if (node->value == value) {
        node->deleted = true;
        return;
      }
      else {
        if (key <= node->key) {
          node = node->left.load(std::memory_order_relaxed);
        }
        else {
          node = node->right.load(std::memory_order_relaxed);
        }
      }
    }

    assert(false && "Value not found");
  }
  
  // Compute the size of the subtree under the given node,
  // not including nodes marked as deleted
  size_t compute_tree_size(Node* node) {
    auto l = node->left.load(std::memory_order_relaxed);
    auto r = node->right.load(std::memory_order_relaxed);
    assert(node != nullptr);
    size_t alive = !(node->deleted);
    if (l == nullptr && r == nullptr) {
      node->size = alive;
    }
    else if (l == nullptr) {
      node->size = alive + compute_tree_size(r);
    }
    else if (r == nullptr) {
      node->size = alive + compute_tree_size(l);
    }
    else {
      size_t left_size, right_size;
      parlay::par_do(
        [&]() { left_size = compute_tree_size(l); },
        [&]() { right_size = compute_tree_size(r); }
      );
      node->size = alive + left_size + right_size;
    }
    return node->size;
  }

  // Flatten the tree into the given buffer. Values that are marked
  // as deleted are not included
  void flatten(Node* node, T** buffer, size_t offset) {
    bool alive = !(node->deleted);
    auto l = node->left.load(std::memory_order_relaxed);
    auto r = node->right.load(std::memory_order_relaxed);
    size_t left_offset = (l == nullptr ? 0 : l->size);
    if (alive) {
      buffer[offset + left_offset] = node->value;
    }
    if (l != nullptr && r != nullptr) {
      parlay::par_do(
        [&]() { flatten(l, buffer, offset); },
        [&]() { flatten(r, buffer, offset + left_offset + alive); }
      );
    }
    else if (l != nullptr) {
      flatten(l, buffer, offset);
    }
    else if (r != nullptr) {
      flatten(r, buffer, offset + left_offset + alive);
    }
  }

  // For debugging -- sequentially compute the size of the reader tree
  size_t _size(Node* node) {
    if (node == nullptr) return 0;
    else return 1 + _size(node->left) + _size(node->right);
  }

  // For debugging -- sequentially compute the height of the reader tree
  size_t _height(Node* node) {
    if (node == nullptr) return 0;
    else return 1 + std::max(_height(node->left), _height(node->right));
  }

  // Build a balanced tree on the elements in buffer[i...j]
  Node* build_tree(T** buffer, size_t i, size_t j) {
    if (i >= j) {
      return nullptr;
    }
    else if (i == j - 1) {
      return new Node(buffer[i]);
    }
    else {
      auto mid = i + (j - i) / 2;
      Node* root = new Node(buffer[mid]);
      if (j - i <= READER_TREE_GRANULARITY) {
        root->left.store(build_tree(buffer, i, mid), std::memory_order_relaxed);
        root->right.store(build_tree(buffer, mid + 1, j), std::memory_order_relaxed);
      }
      else {
        parlay::par_do(
          [&]() { root->left.store(build_tree(buffer, i, mid), std::memory_order_relaxed); },
          [&]() { root->right.store(build_tree(buffer, mid + 1, j), std::memory_order_relaxed); }
        );
      }
      return root;
    }
  }

  // Apply the given function to every element of the set.
  // Should not be called concurrently with adds or removes.
  // Performs cleanup of lazily deleted nodes.
  template<typename Function>
  void for_all(Function f) {
    auto curr = ptr_value.load();

    // The reader set is empty
    if (curr == 0) {
      // Do nothing
    }

    // The reader set contains a single item
    else if ((curr & 1) == 0) {
      T* item = single_item;
      assert(item != nullptr);
      f(item);
    }

    // The reader set is a tree
    else {

      // Flatten tree contents
      Node* node = clean_ptr(root);
      size_t size = compute_tree_size(node);
      T** flattened = (T**) ::operator new(size * sizeof(T*));
      flatten(node, flattened, 0);

      // Execute callback on all elements
      parlay::parallel_for(0, size, [&](auto i) {
        assert(flattened[i] != nullptr);
        f(flattened[i]);
      });

      // Delete existing tree structure
      delete node;

      // If all items were deleted, the reader set is empty
      if (size == 0) {
        ptr_value.store(0);
      }
      // If there is a single reader remaining, write it inline
      else if (size == 1) {
        single_item = flattened[0];
      }
      // Rebuild tree without deleted elements
      else {
        Node* new_root = build_tree(flattened, 0, size);
        assert(new_root != nullptr);
        root = mask_ptr(new_root);
#ifndef DNDEBUG
        assert(compute_tree_size(new_root) == size);
#endif
      }

      // Remove temporary buffer
      ::operator delete(flattened);
    }
  }

  ~ConcurrentReaderSet() {
    auto curr = ptr_value.load();
    if ((curr & 1) != 0) {
      Node* node = clean_ptr(root);
      delete node;
    }
  }

  // Store either a single reader pointer (T*) or a
  // tree of reader pointers. ptr_value is
  // used for convenient uniform CASing since we
  // are technically CASing between different tpyes
  union {
    std::atomic<uintptr_t> ptr_value;
    T* single_item;
    Node* root;
  };
};

}  // namespace psac

#endif  // PSAC_CONCURRENT_SET_HPP_

