#ifndef PSAC_TYPES_HPP_
#define PSAC_TYPES_HPP_

#include <atomic>
#include <iterator>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

#include <parlay/alloc.h>
#include <parlay/parallel.h>

#include <psac/marked_ptr.h>

// Decide whether to use the tree reader
// set or the linked-list reader set
#ifndef PSAC_USE_TREE_READER_SET
#include <psac/concurrent_set.hpp>
#else
#include <psac/reader_set.hpp>
#endif

// ----------------------------------------------------------------------------
//                                   TYPES
// ----------------------------------------------------------------------------

namespace psac {

// ------------------------------- Nodes ----------------------------------

// Computation tree
struct Computation;

// SP tree nodes
struct SPNode;

struct SNode;
struct PNode;
struct RNode;

template<typename READER_FUNCTION, typename... MOD_TYPES>
struct RTupleNode;

template<typename READER_FUNCTION, typename Iter>
struct RArrayNode;

// ------------------------------- Mods ----------------------------------

// Mods
struct ModBase;

template<typename T>
struct Mod;

struct ModArrayBase;

template<typename T>
struct ModArray;

// ----------------------------- Defintions --------------------------------

// ----------------------------------------------------------------------------
//                        Modifiables
// ----------------------------------------------------------------------------

struct ModBase {
  void add_reader(RNode* reader);
  void remove_reader(RNode* reader);
 
  ModBase() = default;
  ~ModBase();

  // Notify all subscribed readers that an update has happened
  void notify_readers();

  // Set of readers (see ConcurrentReaderSet)
  ConcurrentReaderSet<RNode> readers;

  // Only store the written flag when compiling for debugging
#ifndef DNDEBUG
  bool written = false;
#endif  
};

template<typename T>
struct Mod : public ModBase {
  using value_type = T;
  
  Mod();
  Mod(T);

  // Provide copy & move constructor because we like to keep Mods
  // inside std::vectors which require the type to be movable,
  // or copyable if we want to initialise multiple at once
  // Note: Only unwritten mods can be copied or moved.
  Mod(const Mod<T>&);
  Mod<T>& operator=(const Mod<T>&);
  Mod(Mod<T>&&);
  Mod<T>& operator=(Mod<T>&&);

  void write(T new_value);

  T value;
};

template<typename T>
struct ModArray {
  using value_type = T;

  ModArray(size_t);
  ModArray(size_t, T);
  ~ModArray();

  Mod<T>& operator[](size_t);
  const Mod<T>& operator[](size_t) const;

  Mod<T>& at(size_t);
  const Mod<T>& at(size_t) const;

  Mod<T>* get_array();
  const Mod<T>* get_array() const;

  Mod<T>* begin();
  Mod<T>* end();

  const Mod<T>* begin() const;
  const Mod<T>* end() const;

  // Members
  size_t size;
  Mod<T>* array;
};

// ----------------------------------------------------------------------------
//                        Type-erased "any mod" container
// ----------------------------------------------------------------------------

template<typename T> struct type_tag { using type = T; };

struct AnyModBase {
  virtual ~AnyModBase() = default;
};

template<typename T>
struct AnyModArray : public AnyModBase {
  AnyModArray(size_t size) : mod_array(size) { }

  AnyModArray(size_t size, T initial_val) : mod_array(size, std::move(initial_val)) { }

  ModArray<T>* get() { return &mod_array; }
  const ModArray<T>* get() const { return &mod_array; }

  ModArray<T> mod_array;
};

template<typename T>
struct AnyModInline : public AnyModBase {

  AnyModInline() = default;
  AnyModInline(T initial_val) : mod(std::move(initial_val)) {
      static_assert(sizeof(T) <= 8);
      static_assert(sizeof(Mod<T>) <= 16);
  }

  const Mod<T>* get() const { return &mod; }
  Mod<T>* get() { return &mod; }

  Mod<T> mod;
};

template<typename T>
struct AnyModIndirect : public AnyModBase {
  static_assert(sizeof(T) > 8);
  static_assert(sizeof(Mod<T>) > 16);

  AnyModIndirect() : mod(construct()) { }
  AnyModIndirect(T initial_val) : mod(construct(std::move(initial_val))) { }

  const Mod<T>* get() const { return mod.get(); }
  Mod<T>* get() { return mod.get(); }

  template<typename... Args>
  Mod<T>* construct(Args&&... args) {
    Mod<T>* p = static_cast<Mod<T>*>(parlay::p_malloc(sizeof(Mod<T>)));
    new (p) Mod<T>(std::forward<Args>(args)...);
    return p;
  }

  struct Deleter {
    void operator()(Mod<T>* p) {
      p->~Mod<T>();
      parlay::p_free(p);
    }
  };

  std::unique_ptr<Mod<T>, Deleter> mod;
};

// An AnyMod holds a modifiable of any type, or an array of modifiables of any type
//
// To create a modifiable of a given type, use the type_tag tag, like so:
//    AnyMod mod(type_tag<int>);  // creates a mod of type int
//
// To get a pointer to the underlying modifiable, use the get method:
//    mod.get<T>();  // Returns a Mod<T>*
//
// To create an array of modifiables, specify the size and give the default_array_tag()
//    AnyMod mods(type_tag<int>, 10, default_array_tag());  // create an array of 10 mods of type int
//
// To get a pointer to the underlying array, use get_array:
//    mod.get_array<T>();  // Returns a ModArray<T>*
//
// In most normal use cases, you should just use regular mods. The purpose of this
// type is to make it possible to store modifiables of different types in a common
// container, which is used internally by the implementation.
struct AnyMod {

  struct default_array_tag {};

  // Create a modifiable of type T that is default initialized
  template<typename T>
  AnyMod(type_tag<T>) {
    if constexpr(sizeof(T) <= 8) {
      //static_assert(sizeof(AnyModInline<T>) <= 24);
      new (&storage) AnyModInline<T>();
    }
    else {
      //static_assert(sizeof(AnyModIndirect<T>) <= 24);
      new (&storage) AnyModIndirect<T>();
    }
  }

  // Create a modifiable of type T with the given initial value
  template<typename T>
  AnyMod(type_tag<T>, T initial_val) {
    if constexpr(sizeof(T) <= 8) {
      //static_assert(sizeof(AnyModInline<T>) <= 24);
      new (&storage) AnyModInline<T>(std::move(initial_val));
    }
    else {
      //static_assert(sizeof(AnyModIndirect<T>) <= 24);
      new (&storage) AnyModIndirect<T>(std::move(initial_val));
    }
  }

  // Create an array of modifiables of type T that are default initialized
  template<typename T>
  AnyMod(type_tag<T>, size_t size, default_array_tag) {
    //static_assert(sizeof(AnyModArray<T>) <= 24);
    new (&storage) AnyModArray<T>(size);
  }

  // Create an array of modifiables of type T each with the given initial value
  template<typename T>
  AnyMod(type_tag<T>, size_t size, T initial_val) {
    //static_assert(sizeof(AnyModArray<T>) <= 24);
    new (&storage) AnyModArray<T>(size, std::move(initial_val));
  }

  // Since mods might be stored inline, we do not
  // wish to allow copying or moving them
  AnyMod(const AnyMod&) = delete;
  AnyMod(AnyMod&&) = delete;
  AnyMod& operator=(const AnyMod&) = delete;
  AnyMod& operator=(AnyMod&&) = delete;

  // Return a pointer to the stored modifiable of type T
  //
  // Asserts an error if the stored modifiable is not actually of type T
  template<typename T> Mod<T>* get() {
    if constexpr (sizeof(T) <= 8) {
      assert(is_inline<T>());
      return static_cast<AnyModInline<T>*>(get_base())->get();
    }
    else {
      assert(is_indirect<T>());
      return static_cast<AnyModIndirect<T>*>(get_base())->get();
    }
  }

  // Return a pointer to the stored array of modifiables of type T
  //
  // Asserts an error if the stored type is not actually an array of modifiables of type T
  template<typename T> ModArray<T>* get_array() {
    assert(is_array<T>());
    return static_cast<AnyModArray<T>*>(get_base())->get();
  }

  // Return a base-type pointer to the stored object
  AnyModBase* get_base() { return std::launder(reinterpret_cast<AnyModBase*>(&storage)); }
  const AnyModBase* get_base() const { return std::launder(reinterpret_cast<const AnyModBase*>(&storage)); }

  // Returns true if the stored value is an inline modifiable of type T
  template<typename T>
  bool is_inline() const { return dynamic_cast<const AnyModInline<T>*>(get_base()) != nullptr; }

  // Returns true if the stored value is an indirect (heap-allocated) modifiable of type T
  template<typename T>
  bool is_indirect() const { return dynamic_cast<const AnyModIndirect<T>*>(get_base()) != nullptr; }

  // Returns true if the stored value is an array of modifiables of type T
  template<typename T>
  bool is_array() const { return dynamic_cast<const AnyModArray<T>*>(get_base()) != nullptr; }

  ~AnyMod() { get_base()->~AnyModBase(); }

  std::aligned_storage<24, 8>::type storage;
};

// A dynamic linked list of any modifiables (AnyMods)
struct AnyModList {

  struct Node final {

    static void* operator new([[maybe_unused]] size_t size) {
      assert(size == sizeof(Node));
      return parlay::type_allocator<Node>::alloc();
    }

    static void operator delete(void* p) {
      parlay::type_allocator<Node>::free(static_cast<Node*>(p));
    }

    template<typename T, typename... Args>
    Node(type_tag<T> tag, std::unique_ptr<Node> nxt, Args&&... args) :
      mod(tag, std::forward<Args>(args)...), next(std::move(nxt)) { }

    AnyMod mod;
    std::unique_ptr<Node> next;
  };

  AnyModList() = default;
  AnyModList(const AnyModList&) = delete;
  AnyModList& operator=(const AnyModList&) = delete;
  AnyModList& operator=(AnyModList&& other) = default;
  AnyModList(AnyModList&& other) = default;

  template<typename T, typename... Args>
  Mod<T>* push(Args&&... args) {
    head = std::make_unique<Node>(type_tag<T>(), std::move(head), std::forward<Args>(args)...);
    return head->mod.get<T>();
  }

  template<typename T>
  ModArray<T>* push_array(size_t size) {
    head = std::make_unique<Node>(type_tag<T>(), std::move(head), size, AnyMod::default_array_tag());
    return head->mod.get_array<T>();
  }

  template<typename T>
  ModArray<T>* push_array(size_t size, T initial_val) {
    head = std::make_unique<Node>(type_tag<T>(), std::move(head), size, std::move(initial_val));
    return head->mod.get_array<T>();
  }

  void clear() {
    head = nullptr;
  }

  std::unique_ptr<Node> head;
};

// ----------------------------------------------------------------------------
//                        SP trees and computations
// ----------------------------------------------------------------------------

struct Computation {
  Computation() : root(nullptr) { }
  Computation(std::unique_ptr<SPNode> _root) : root(std::move(_root)) { }

  Computation(const Computation&) = delete;
  Computation& operator=(const Computation&) = delete;

  Computation(Computation&&);
  Computation& operator=(Computation&&);

  ~Computation() = default;

  size_t treesize() const;
  size_t memory() const;
  void update();
  void destroy();

  std::unique_ptr<SPNode> root;
};

struct SPNode {

  SPNode(SPNode* parent_) : dynamic_mods(), parent(parent_) { }
  
  template<typename T>
  Mod<T>* alloc();
 
  template<typename T>
  Mod<T>* alloc_array(size_t);

  template<typename NodeType, typename... Args>
  SPNode* make_left(Args&&... args);
  
  template<typename NodeType, typename... Args>
  SPNode* make_right(Args&&... args);
 
  SPNode* set_left(std::unique_ptr<SPNode> node);

  SPNode* set_right(std::unique_ptr<SPNode> node);

  template<typename READER_FUNCTION, typename... MOD_TYPES>
  RNode* make_read_tuple_node(READER_FUNCTION _f, const std::tuple<MOD_TYPES...>& _mods);
  
  template<typename READER_FUNCTION, typename Iter>
  RNode* make_read_array_node(READER_FUNCTION _f, const std::pair<Iter,Iter> _range);
  
  template<typename READER_FUNCTION, typename Iter>
  RNode* make_dynamic_read_node(READER_FUNCTION _f, const std::pair<Iter,Iter> _range);
  
  template<typename READER_FUNCTION>
  RNode* make_scope_read_node(READER_FUNCTION _f);

  // Propagation flags
  void set_dirty(bool dirty) {
    if (dirty) {
      parent.set_mark(true);
      if (parent && !(parent->is_dirty())) {
        parent->set_dirty(dirty);
      }
    }
    else {
      parent.set_mark(false);
    }
  }
  bool is_dirty() const { return parent.get_mark(); }

  virtual void propagate() = 0;
  virtual size_t size() const = 0;

  virtual ~SPNode() {
    left.reset();
    right.reset();
    dynamic_mods.clear();
  }
  
  // Store managed pointers to all mods allocated in the scope of this node
  // so that they are automatically deleted when this node is deleted
  AnyModList dynamic_mods;

  marked_ptr<SPNode> parent;
  std::unique_ptr<SPNode> left, right;
};


struct SNode final : public SPNode {

  static void* operator new(size_t sz) {
    assert(sz == sizeof(SNode));
    return static_cast<void*>(parlay::type_allocator<SNode>::alloc());
  }

  static void operator delete(void* p) {
    parlay::type_allocator<SNode>::free(static_cast<SNode*>(p));
  }

  SNode(SPNode* parent) : SPNode(parent) { }

  void propagate() override {
    if (is_dirty()) {
      if (left) left->propagate();
      if (right) right->propagate();
      set_dirty(false);
    }
  }

  size_t size() const override { return sizeof(SNode); }

  ~SNode() = default;
};

struct PNode final : public SPNode {

  static void* operator new(size_t sz) {
    assert(sz == sizeof(PNode));
    return static_cast<void*>(parlay::type_allocator<PNode>::alloc());
  }

  static void operator delete(void* p) {
    parlay::type_allocator<PNode>::free(static_cast<PNode*>(p));
  }

  PNode(SPNode* parent) : SPNode(parent) { }

  void propagate() override {
    if (is_dirty()) {
      if (!(left->is_dirty())) right->propagate();
      else if (!(right->is_dirty())) left->propagate();
      else {
        parlay::par_do(
            [&]() { left->propagate(); },
            [&]() { right->propagate(); }
        );
      }
      set_dirty(false);
    }
  }

  size_t size() const override { return sizeof(PNode); }

  ~PNode() {
    if (left && right) {
      parlay::par_do(
          [&]() { left.reset(); },
          [&]() { right.reset(); }
      );
    }
    else {
      left.reset();
      right.reset();
    }
  }
};


struct RNode : public SPNode {

  RNode(SPNode* parent) : SPNode(parent) { }

  void set_modified() {
    pending_update = true;
    set_dirty(true);
  }

  void clear_pending_modification() {
    pending_update = false;
    set_dirty(false);
  }

  bool has_pending_modification() const {
    return pending_update;
  }

  void propagate() override;
  
  virtual void execute(std::unique_ptr<SPNode>& node) = 0;

  bool pending_update = false;
};

template<typename READER_FUNCTION, typename... MOD_TYPES>
struct RTupleNode final : public RNode {

  using r_tuple_type = RTupleNode<READER_FUNCTION, MOD_TYPES...>;

  static void* operator new(size_t sz) {
    assert(sz == sizeof(r_tuple_type));
    return static_cast<void*>(parlay::type_allocator<r_tuple_type>::alloc());
  }

  static void operator delete(void* p) {
    parlay::type_allocator<r_tuple_type>::free(static_cast<r_tuple_type*>(p));
  }

  RTupleNode(SPNode* parent, READER_FUNCTION _f, const std::tuple<MOD_TYPES...>& _mods)
    : RNode(parent), f(std::move(_f)), mods(_mods) {
    // Register this node as a reader of the mods
    std::apply([this](auto... mods) { (mods->add_reader(this), ...); }, mods);
  }

  void execute(std::unique_ptr<SPNode>& node) override {
    assert(node.get() == this);
    std::apply([&](auto... mods) { f(&node, mods->value...); }, mods);
  }
  size_t size() const override { return sizeof(RTupleNode<READER_FUNCTION, MOD_TYPES...>); }
  
  // Remove all reader subscriptions
  void unsubscribe() {
    std::apply([this](auto... mods) { (mods->remove_reader(this), ...); }, mods);
  }

  READER_FUNCTION f;
  std::tuple<MOD_TYPES...> mods;

  ~RTupleNode() { unsubscribe(); }
};

template<typename READER_FUNCTION, typename Iter>
struct RArrayNode final : public RNode {

  using r_array_type = RArrayNode<READER_FUNCTION, Iter>;

  static void* operator new(size_t sz) {
    assert(sz == sizeof(r_array_type));
    return static_cast<void*>(parlay::type_allocator<r_array_type>::alloc());
  }

  static void operator delete(void* p) {
    parlay::type_allocator<r_array_type>::free(static_cast<r_array_type*>(p));
  }

  using value_type = typename std::iterator_traits<Iter>::value_type::value_type;
  
  RArrayNode(SPNode* parent, READER_FUNCTION _f, const std::pair<Iter,Iter> _range)
    : RNode(parent), f(std::move(_f)), range(_range) {
    // Register as a reader of the mods
    for (auto it = range.first; it != range.second; it++) {
      it->add_reader(this);
    }
  }
  
  void execute(std::unique_ptr<SPNode>& node) override {
    assert(node.get() == this);
    std::vector<value_type> values(std::distance(range.first, range.second));
    size_t i = 0;
    for (auto it = range.first; it != range.second; it++, i++) {
      values[i] = it->value;
    }
    f(&node, values);
  }

  size_t size() const override { return sizeof(RArrayNode<READER_FUNCTION, Iter>); }
  
  void unsubscribe() {
    for (auto it = range.first; it != range.second; it++) {
      it->remove_reader(this);
    }
  }

  ~RArrayNode() { unsubscribe(); }
  
  READER_FUNCTION f;
  std::pair<Iter,Iter> range;
};

template<typename READER_FUNCTION>
struct RScopeNode final : public RNode {

  using r_scope_type = RScopeNode<READER_FUNCTION>;

  static void* operator new(size_t sz) {
    assert(sz == sizeof(r_scope_type));
    return static_cast<void*>(parlay::type_allocator<r_scope_type>::alloc());
  }

  static void operator delete(void* p) {
    parlay::type_allocator<r_scope_type>::free(static_cast<r_scope_type*>(p));
  }

  RScopeNode(SPNode* parent, READER_FUNCTION _f) : RNode(parent), f(std::move(_f)) { }
 
  // TODO: Deal with potential race with mark all 
  void execute(std::unique_ptr<SPNode>& node) override {
    assert(node.get() == this);
    std::vector<ModBase*> new_mods;
    f(&node, new_mods);
    
    // Replace mods with new_mods, but first, update the mods that
    // this reader is subscribed to. Rather than unsubscribe to all
    // mods and resubscribe to all new mods, we only unsubsribe from
    // those that are not re-read, and subscribe to ones that were 
    // not previously read.
    std::sort(std::begin(new_mods), std::end(new_mods));
    for (size_t i = 0, j = 0; i < mods.size() || j < new_mods.size(); ) {
      if (i == mods.size()) {
        new_mods[j++]->add_reader(this);
      }
      else if (j == new_mods.size()) {
        mods[i++]->remove_reader(this);
      }
      else {
        if (mods[i] == new_mods[j]) i++, j++;
        else if (mods[i] < new_mods[j]) mods[i++]->remove_reader(this);
        else new_mods[j++]->add_reader(this);
      }
    }
    mods = std::move(new_mods);
  }
  size_t size() const override { return sizeof(RScopeNode<READER_FUNCTION>); }
 
  void unsubscribe() {
    for (ModBase* mod : mods) {
      mod->remove_reader(this);
    }
  }

  ~RScopeNode() { unsubscribe(); }
  
  READER_FUNCTION f;
  std::vector<ModBase*> mods;
};

// Garbage collector
struct GarbageCollector {

  GarbageCollector(size_t num_threads) : garbage(num_threads) { }

  ~GarbageCollector() {
#ifndef DNDEBUG
    for ([[maybe_unused]] const auto& pile : garbage) {
      assert(pile.empty() && "GARBAGE COLLECTOR SHOULD BE RAN BEFORE PROGRAM TERMINATION (PREFERABLY AFTER EACH ROUND OF PROPAGATION) TO AVOID THE GARBAGE-COLLECTED SP TREES FROM UNSUBSCRIBING TO DANGLING MOD POINTERS");
    }
#endif
  }
  
  GarbageCollector(const GarbageCollector&) = delete;
  GarbageCollector& operator=(const GarbageCollector&) = delete;

  static GarbageCollector& instance() {
    static GarbageCollector instance(2 * std::thread::hardware_concurrency());
    return instance;
  }

  static void add(std::unique_ptr<SPNode> node) {
    auto& ins = instance();
    ins.garbage[parlay::worker_id()].emplace_back(std::move(node));
  }

  static void run() {
    auto& ins = instance();
    for (auto& pile : ins.garbage) {
      pile.clear();
    }
  }

  // Compute the amount of memory awaiting garbage collection
  static size_t memory() {
    auto& ins = instance();
    size_t answer = 0;
    std::function<size_t(SPNode*)> mem = [&](SPNode* node) -> size_t {
      if (node == nullptr) return 0;
      else return node->size() + mem(node->left.get()) + mem(node->right.get());
    };
    for (const auto& pile : ins.garbage) {
      for (const auto& node : pile) {
        answer += mem(node.get()); 
      }
    }
    return answer;
  }

  static size_t nodes() {
    auto& ins = instance();
    size_t answer = 0;
    std::function<size_t(SPNode*)> size = [&](SPNode* node)-> size_t {
      if (node == nullptr) return 0;
      else return 1 + size(node->left.get()) + size(node->right.get());
    };
    for (const auto& pile : ins.garbage) {
      for (const auto& node: pile) {
        answer += size(node.get());
      }
    }
    return answer;
  }

  std::vector<std::vector<std::unique_ptr<SPNode>>> garbage;
};


// ----------------------------------------------------------------------------
//                                IMPLEMENTATION
// ----------------------------------------------------------------------------

// Parallel for loop -- generates a binary tree of P nodes with (hi - lo + 1)
// leaves. More efficient than manually doing divide and conquer since this
// would result in an unnecessary S node in between each pair of P nodes.
// It also does dynamic granularity control, which is not possible with
// manual divide and conquer.
template<typename T, typename Function>
std::unique_ptr<SPNode> _psac_par_for(SPNode* parent, T lo, T hi, Function f, int granularity) {
  if (hi - lo <= static_cast<T>(granularity)) {
    return _psac_seq_for(parent, lo, hi, f);
  }
  else {
    auto node = std::make_unique<PNode>(parent);
    auto mid = lo + (hi - lo) / 2;
    parlay::par_do(
      [&]() { node->left = std::move(_psac_par_for(node.get(), lo, mid, f, granularity)); },
      [&]() { node->right = std::move(_psac_par_for(node.get(), mid, hi, f, granularity)); }
    );
    return node;
  }
}

template<typename T, typename Function>
std::unique_ptr<SPNode> _psac_seq_for(SPNode* parent, T lo, T hi, Function f) {
  if (lo == hi - 1) {
    std::unique_ptr<SPNode> node = std::make_unique<SNode>(parent);
    std::unique_ptr<SPNode>* _node = &node;
    f(_node, lo);
    return node;
  }
  else {
    auto node = std::make_unique<SNode>(parent);
    auto mid = lo + (hi - lo) / 2;
    node->left = std::move(_psac_seq_for(node.get(), lo, mid, f));
    node->right = std::move(_psac_seq_for(node.get(), mid, hi, f));
    return node;
  }
}

// ---------------------------------- Mod ----------------------------------

ModBase::~ModBase() {
  assert(readers.empty() && "READER SET OF A MOD MUST BE EMPTY WHEN THE MOD IS DESTROYED");
}

void ModBase::notify_readers() {
  readers.for_all([](RNode* node) {
    node->set_modified();
  });
}

void ModBase::add_reader(RNode* r) {
#ifndef DNDEBUG
  assert(written);
#endif
  assert(r != nullptr);
  readers.insert(r);
}

void ModBase::remove_reader(RNode* r) {
#ifndef DNDEBUG
  assert(written);
#endif
  assert(r != nullptr);
  readers.remove(r);
}

template<typename T>
Mod<T>::Mod() : value(T{}) { }

template<typename T>
Mod<T>::Mod(T initial_val) : value(std::move(initial_val)) { }

// Copying only allowed for unused Mods, otherwise
// there will be pointers to them which could dangle
template<typename T>
Mod<T>::Mod(const Mod<T>&) {
#ifndef DNDEBUG
  assert(!written);
#endif
}

template<typename T>
Mod<T>& Mod<T>::operator=(const Mod<T>&) {
#ifndef DNDEBUG
  assert(!written);
#endif
}

// Movement only allowed for unused Mods, otherwise
// there will be pointers to them which could dangle
template<typename T>
Mod<T>::Mod(Mod<T>&&) {
#ifndef DNDEBUG
  assert(!written);
#endif
}

template<typename T>
Mod<T>& Mod<T>::operator=(Mod<T>&&) {
#ifndef DNDEBUG
  assert(!written);
#endif
}
  
template<typename T>
void Mod<T>::write(T new_value) {
#ifndef DNDEBUG
  if (!written || value != new_value) {
    value = std::move(new_value);
    notify_readers();
    written = true;
  }
#else
  if (value != new_value) {
    value = std::move(new_value);
    notify_readers();
  }
#endif
}

// Initialize an array of modifiables. Initialization is done in parallel
template<typename T>
ModArray<T>::ModArray(size_t _size) : size(_size) {
  array = (Mod<T>*) parlay::p_malloc(size * sizeof(Mod<T>));
  parlay::parallel_for(0, size, [&](auto i) {
    new (&(at(i))) Mod<T>();
  });
}

template<typename T>
ModArray<T>::ModArray(size_t _size, T initial_val) : size(_size) {
  array = (Mod<T>*) parlay::p_malloc(size * sizeof(Mod<T>));
  parlay::parallel_for(0, size, [&](auto i) {
    new (&(at(i))) Mod<T>(initial_val);
  });
}

template<typename T>
ModArray<T>::~ModArray() {
  parlay::parallel_for(0, size, [&](auto i) {
    at(i).~Mod<T>();
  });
  parlay::p_free(array);
}

template<typename T>
Mod<T>& ModArray<T>::at(size_t i) {
  assert(i < size);
  return *(array + i);
}

template<typename T>
const Mod<T>& ModArray<T>::at(size_t i) const {
  assert(i < size);
  return *(array + i);
}

template<typename T>
Mod<T>& ModArray<T>::operator[](size_t i) {
  return at(i);
}

template<typename T>
const Mod<T>& ModArray<T>::operator[](size_t i) const {
  return at(i);
}

template<typename T>
Mod<T>* ModArray<T>::get_array() {
  return array;
}

template<typename T>
const Mod<T>* ModArray<T>::get_array() const {
  return array;
}

template<typename T>
Mod<T>* ModArray<T>::begin() {
  return array;
}

template<typename T>
Mod<T>* ModArray<T>::end() {
  return array + size;
}

template<typename T>
const Mod<T>* ModArray<T>::begin() const {
  return array;
}

template<typename T>
const Mod<T>* ModArray<T>::end() const {
  return array + size;
}

// ---------------------------------- Computation ----------------------------------

Computation::Computation(Computation&& other) : root(std::move(other.root)) {

}

Computation& Computation::operator=(Computation&& other) {
  root = std::move(other.root);
  return *this;
}

// Compute the number of nodes in the SP tree
size_t Computation::treesize() const {
  std::function<int(SPNode*)> size = [&](SPNode* node)-> size_t {
    if (node == nullptr) return 0;
    else return 1 + size(node->left.get()) + size(node->right.get());
  };
  return size(root.get());
}

// Compute the memory usage of the SP tree
size_t Computation::memory() const {
  std::function<size_t(SPNode*)> mem = [&](SPNode* node) -> size_t {
    if (node == nullptr) return 0;
    else return node->size() + mem(node->left.get()) + mem(node->right.get());
  };
  return mem(root.get());
}

// Run all pending updates
void Computation::update() {
  assert(root);
  root->propagate();
}

// Manually destroy the SP tree
void Computation::destroy() {
  root.reset();
}

// ---------------------------------- SPNode ----------------------------------

template<typename T>
Mod<T>* SPNode::alloc() {
  return dynamic_mods.push<T>();
}

template<typename T>
Mod<T>* SPNode::alloc_array(size_t size) {
  return dynamic_mods.push_array<T>(size)->begin();
}

template<typename NodeType, typename... Args>
SPNode* SPNode::make_left(Args&&... args) {
  if (left) GarbageCollector::add(std::move(left));
  left = std::make_unique<NodeType>(this, std::forward<Args>(args)...);
  return left.get();
}

template<typename NodeType, typename... Args>
SPNode* SPNode::make_right(Args&&... args) {
  if (right) GarbageCollector::add(std::move(right));
  right = std::make_unique<NodeType>(this, std::forward<Args>(args)...);
  return right.get();
}

SPNode* SPNode::set_left(std::unique_ptr<SPNode> node) {
  if (left) GarbageCollector::add(std::move(left));
  left = std::move(node);
  return left.get();
}

SPNode* SPNode::set_right(std::unique_ptr<SPNode> node) {
  if (right) GarbageCollector::add(std::move(right));
  right = std::move(node);
  return right.get();
}

template<typename READER_FUNCTION, typename... MOD_TYPES>
RNode* SPNode::make_read_tuple_node(READER_FUNCTION _f, const std::tuple<MOD_TYPES...>& _mods) {
  if (left) GarbageCollector::add(std::move(left));
  left = std::make_unique<RTupleNode<READER_FUNCTION, MOD_TYPES...>>(this, std::move(_f), _mods);
  return static_cast<RNode*>(left.get());
}

template<typename READER_FUNCTION, typename Iter>
RNode* SPNode::make_read_array_node(READER_FUNCTION _f, const std::pair<Iter,Iter> _range) {
  if (left) GarbageCollector::add(std::move(left));
  left = std::make_unique<RArrayNode<READER_FUNCTION, Iter>>(this, std::move(_f), _range);
  return static_cast<RNode*>(left.get());
}

template<typename READER_FUNCTION>
RNode* SPNode::make_scope_read_node(READER_FUNCTION _f) {
  if (left) GarbageCollector::add(std::move(left));
  left = std::make_unique<RScopeNode<READER_FUNCTION>>(this, std::move(_f));
  return static_cast<RNode*>(left.get());
}

// ---------------------------------- RNode ----------------------------------

void RNode::propagate() {

  if (has_pending_modification()) {
    // Reset the children and any allocated dynamic dynamic_mods
    // This is a little bit hacky currently. We move the children
    // and the list of dynamic dynamic_mods over to a dummy copy
    // of the current node and then GC that node. This is easier
    // than actually GCing the current node, and has the same
    // effect
    auto dummy_node = std::make_unique<SNode>(nullptr);
    dummy_node->set_left(std::move(left));
    dummy_node->set_right(std::move(right));
    dummy_node->dynamic_mods = std::move(dynamic_mods);
    GarbageCollector::add(std::move(dummy_node));

    // We need to pass down a unique_ptr to ourself, and we know that
    // we are always our parent's left child, so we can obtain that
    // by doing the following
    std::unique_ptr<SPNode>& scope = parent->left;

    execute(scope);
    clear_pending_modification();
  }
  else if (is_dirty()) {
    if (left) left->propagate();
    if (right) right->propagate();
    set_dirty(false);
  }
}


}  // namespace psac

#endif  // PSAC_TYPES_HPP_
