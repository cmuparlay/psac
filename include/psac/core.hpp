
#ifndef PSAC_CORE_HPP_
#define PSAC_CORE_HPP_

#include <cassert>

#include <memory>
#include <tuple>
#include <type_traits>

#include <parlay/parallel.h>

// ----------------------------------------------------------------------------
//                               LANGUAGE CORE
// ----------------------------------------------------------------------------

// Define a self-adjusting function called name that takes the arguments ...
//
// Specifically, this yields a function with the signature
//   void name(args...);
//
#define _PSAC_FUNCTION(_name, ...)                                            \
void _name([[maybe_unused]] std::unique_ptr<psac::SPNode>*& _node,            \
           [[maybe_unused]] psac::SPNode*& _parent,                           \
           ##__VA_ARGS__)                                                     \

// Execute a new self-adjusting computation by running the function func,
// taking the given arguments ...
//
// Returns a psac::Computation object, a handle to the computation.
#define _PSAC_RUN(_func, ...)                                                 \
  [&]() {                                                                     \
    std::unique_ptr<psac::SPNode> _root =                                     \
      std::make_unique<psac::SNode>(nullptr);                                 \
    std::unique_ptr<psac::SPNode>* _node = &_root;                            \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _func(_node, _parent, ##__VA_ARGS__);                                     \
    assert((*_node == nullptr && _parent != nullptr) ||                       \
            (*_node != nullptr &&                                             \
             (_parent == nullptr || _parent == (*_node)->parent)));           \
    return psac::Computation(std::move(_root));                               \
  }();


#define _PSAC_CALL(_func, ...)                                                \
{                                                                             \
  _func(_node, _parent, ##__VA_ARGS__);                                       \
  assert((*_node == nullptr && _parent != nullptr) ||                         \
          (*_node != nullptr &&                                               \
           (_parent == nullptr || _parent == (*_node)->parent)));             \
}

#define _PSAC_ALLOC(type)                                                     \
[&]() {                                                                       \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  return (*_node)->template alloc<type>();                                    \
}()

#define _PSAC_ALLOC_ARRAY(type, size)                                         \
[&]() {                                                                       \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  return (*_node)->template alloc_array<type>(size);                          \
}()

#define _PSAC_PAR(_left_code, _right_code)                                    \
{                                                                             \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  auto _left = (*_node)-> template make_left<psac::PNode>();                  \
  _left-> template make_left<psac::SNode>();                                  \
  _left-> template make_right<psac::SNode>();                                 \
  auto _l_f = [&]([[maybe_unused]] std::unique_ptr<psac::SPNode>* _node) {    \
    assert(_node != nullptr);                                                 \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _left_code ;                                                              \
  };                                                                          \
  auto _r_f = [&]([[maybe_unused]] std::unique_ptr<psac::SPNode>* _node) {    \
    assert(_node != nullptr);                                                 \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _right_code ;                                                             \
  };                                                                          \
  auto left_scope = &(_left->left);                                           \
  auto right_scope = &(_left->right);                                         \
  parlay::par_do(                                                             \
    [&]() { _l_f(left_scope); },                                              \
    [&]() { _r_f(right_scope); }                                              \
  );                                                                          \
  _parent = (*_node).get();                                                   \
  _node = &((*_node)->right);                                                 \
  assert((*_node) == nullptr);                                                \
}

#define _PSAC_PAR_FOR(_var, _lb, _ub, _gran, _body...)                        \
{                                                                             \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  auto _f = [&]([[maybe_unused]] std::unique_ptr<psac::SPNode>* _node,        \
                  _var) {                                                     \
    assert(_node != nullptr);                                                 \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _body ;                                                                   \
  };                                                                          \
  auto _left = psac::_psac_par_for((*_node).get(), _lb, _ub, _f, _gran);      \
  (*_node)->set_left(std::move(_left));                                       \
  _parent = (*_node).get();                                                   \
  _node = &((*_node)->right);                                                 \
  assert((*_node) == nullptr);                                                \
}

#define _UNBRACKET(args...) args

#define _PSAC_READ(_params, _mods, _body...)                                  \
{                                                                             \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  auto _read_f = [=]([[maybe_unused]] std::unique_ptr<psac::SPNode>* _node,   \
                       _UNBRACKET _params) {                                  \
    assert(_node != nullptr);                                                 \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _body ;                                                                   \
  };                                                                          \
  auto _mod_tuple = std::make_tuple _mods;                                    \
  auto _left = (*_node)-> template make_read_tuple_node(_read_f, _mod_tuple); \
  _left->execute((*_node)->left);                                             \
  _parent = (*_node).get();                                                   \
  _node = &((*_node)->right);                                                 \
  assert((*_node) == nullptr);                                                \
}

#define _PSAC_READ_ARRAY(_param, _mod_its, _body...)                          \
{                                                                             \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  auto _read_f = [=]([[maybe_unused]] std::unique_ptr<psac::SPNode>* _node,   \
                       _param) {                                              \
    assert(_node != nullptr);                                                 \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _body ;                                                                   \
  };                                                                          \
  auto _left = (*_node)-> template make_read_array_node(_read_f,_mod_its);    \
  _left->execute((*_node)->left);                                             \
  _parent = (*_node).get();                                                   \
  _node = &((*_node)->right);                                                 \
  assert((*_node) == nullptr);                                                \
}

#define _PSAC_DYNAMIC_CONTEXT(_body...)                                       \
{                                                                             \
  if (*_node == nullptr) {                                                    \
    assert(_parent != nullptr);                                               \
    *_node = std::make_unique<psac::SNode>(_parent);                          \
  }                                                                           \
  auto _read_f = [=]([[maybe_unused]] std::unique_ptr<psac::SPNode>* _node,   \
                       std::vector<psac::ModBase*>& _psac_context_mods) {     \
    assert(_node != nullptr);                                                 \
    [[maybe_unused]] psac::SPNode* _parent = nullptr;                         \
    _body ;                                                                   \
  };                                                                          \
  auto _left = (*_node)-> template make_scope_read_node(_read_f);             \
  _left->execute((*_node)->left);                                             \
  _parent = (*_node).get();                                                   \
  _node = &((*_node)->right);                                                 \
  assert((*_node) == nullptr);                                                \
}

#define _PSAC_DYNAMIC_CONTEXT_READ(_mod)                                      \
[&_psac_context_mods, _mod_ptr = _mod]() {                                    \
  _psac_context_mods.push_back(_mod_ptr);                                     \
  return _mod_ptr->value;                                                     \
}()

#define _PSAC_WRITE(_M, _value)                                               \
  (*(_M)).write(_value)

#define _PSAC_PROPAGATE(_root)                                                \
  (_root).update()
  
#endif  // PSAC_CORE_HPP_

