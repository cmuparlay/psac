
#ifndef PSAC_CONCURRENT_SET_HPP_
#define PSAC_CONCURRENT_SET_HPP_

#include <atomic>
#include <vector>

namespace psac {

// A concurrent data structure for storing readers with
// a small buffer optimization for single readers. i.e.
// if there is only a single reader, the reader is stored
// inline, and no additional heap memory is required. If 
// there are multiple readers, the data structure will convert
// into a linked list.
//
// Template Parameter <T>: Read node type
template<typename T>
struct ConcurrentReaderSet {

  struct Node {
    Node(T* _value, Node* _next) : value(_value), next(_next), deleted(false) { }
    T* value;
    Node* next;
    bool deleted;  // TODO: Pack this bit into the pointer? :D
  };

  ConcurrentReaderSet() : ptr_value(0) { }

  // Convert between pointers and integer types for bitwise operations
  template<typename P>
  inline uintptr_t as_int(P* ptr) { return reinterpret_cast<uintptr_t>(ptr); }

  inline Node* as_ptr(uintptr_t val) { return reinterpret_cast<Node*>(val); }

  inline Node* clean_ptr(Node* ptr) { return as_ptr(as_int(ptr) & uintptr_t(~1)); }

  // Create a new linked list node, returning a
  // pointer to it with its 1 bit set
  Node* make_node(T* value, Node* next) {
    Node* node = new Node(value, next);
    return as_ptr(as_int(node) | 1);
  }

  // Check whether the set is empty. Will perform all pending
  // lazy deletions and hence is not thread safe with other operations.
  bool empty() {
    for_all([](auto) { });
    return ptr_value.load() == 0;
  }

  // Add an element into the set. Multiple adds can safely happen
  // concurrently with removes, but should not happen concurrently
  // with reads (for_all).
  void insert(T* value) {
    auto curr = ptr_value.load();
    // Reader slot is empty
    if (curr == 0) {
      insert_only(value);
    }
    // Reader slot contains a single reader
    else if ((curr & 1) == 0) {
      T* head_item = single_item;
      Node* node = make_node(head_item, nullptr);
      if (ptr_value.compare_exchange_strong(curr, as_int(node))) {
        insert_another(value);
      }
      else {
        delete clean_ptr(node);
        if ((curr & 1) != 0) {
          insert_another(value);
        }
        else {
          insert_only(value);
        }
      }
    }
    // Reader slot is a linked list
    else {
      insert_another(value);
    }
  }

  void insert_only(T* value) {
    assert(value != nullptr);
    uintptr_t curr = 0;
    if (!ptr_value.compare_exchange_strong(curr, as_int(value))) {
      // Reader slot contains a single reader
      if ((curr & 1) == 0) {
        T* head_item = single_item;
        Node* node = make_node(head_item, nullptr);
        // Someone else converted the single reader to a linked list first
        if (!ptr_value.compare_exchange_strong(curr, as_int(node))) {
          delete clean_ptr(node);
        }
        insert_another(value);
      }
      // Reader slot is a linked list
      else {
        insert_another(value);
      }
    }
  }

  void insert_another(T* value) {
    assert(value != nullptr);
    Node* new_node = make_node(value, head);
    Node*& next = clean_ptr(new_node)->next;
    while (!head.compare_exchange_weak(next, new_node)) { }
  }

  void remove(T* value) {
    assert(value != nullptr);
    auto curr = ptr_value.load();
    assert(curr != 0);
    // Reader slot is a single reader
    if ((curr & 1) == 0) {
      assert(curr == as_int(value));
      // Someone else converted the single reader to a linked list first
      if (!ptr_value.compare_exchange_strong(curr, 0)) {
        remove_another(value);
      }
    }
    // Reader slot is a linked list
    else {
      remove_another(value);
    }
  }

  // Remove the given value (lazily) from the set. The node
  // will actually be removed the next time the list is
  // traversed. This operation is thread safe with add
  // and other removes, but not with reads (for_all).
  void remove_another(T* value) {
    Node* node = clean_ptr(head);
    while (node != nullptr) {
      if (node->value == value) {
        node->deleted = true;
        return;
      }
      else {
        node = clean_ptr(node->next);
      }
    }
    assert(false && "Value not found");
  }

  // Apply the given function to every element of the list.
  // Should not be called concurrently with adds or removes.
  // Performs cleanup of lazily deleted nodes.
  template<typename Function>
  void for_all(Function f) {
    auto curr = ptr_value.load();
    if (curr == 0) {
      return;
    }
    else if ((curr & 1) == 0) {
      f(single_item);
    }
    else {
      Node* node = clean_ptr(head);
      Node* prev = nullptr;
      while (node != nullptr) {
        if (node->deleted) {
          if (prev == nullptr) {
            head = node->next;
            delete node;
            node = clean_ptr(head);
          }
          else {
            prev->next = node->next;
            delete node;
            node = clean_ptr(prev->next);
          }
        }
        else {
          f(node->value);
          prev = node;
          node = clean_ptr(node->next);
        }
      }
    }
  }

  ~ConcurrentReaderSet() {
    auto curr = ptr_value.load();
    if ((curr & 1) != 0) {
      Node* node = clean_ptr(head);
      while (node != nullptr) {
        auto old_node = node;
        node = clean_ptr(node->next);
        delete old_node;
      }
    }
  }

  // Store either a single reader pointer (T*) or a
  // linked list of reader pointers. ptr_value is
  // used for convenient uniform CASing since we
  // are technically CASing between different tpyes
  union {
    std::atomic<uintptr_t> ptr_value;
    T* single_item;
    std::atomic<Node*> head;
  };
};

}  // namespace psac

#endif  // PSAC_CONCURRENT_SET_HPP_
