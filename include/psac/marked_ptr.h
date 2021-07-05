
#ifndef PSAC_MARKED_PTR_H
#define PSAC_MARKED_PTR_H

namespace psac {

template<typename T>
class marked_ptr {

  static constexpr uintptr_t ONE_BIT = 1;
  static constexpr uintptr_t TWO_BIT = 1 << 1;

 public:
  marked_ptr() : ptr(0) {}

  marked_ptr(std::nullptr_t) : ptr(0) {}

  /* implicit */ marked_ptr(T *new_ptr) : ptr(reinterpret_cast<uintptr_t>(new_ptr)) {}

  operator T *() const { return get_ptr(); }
  operator bool() const { return get_ptr() != nullptr; }

  typename std::add_lvalue_reference_t<T> operator*() const { return *(get_ptr()); }

  T *operator->() { return get_ptr(); }

  const T *operator->() const { return get_ptr(); }

  T *get_ptr() const { return reinterpret_cast<T *>(ptr & ~(ONE_BIT | TWO_BIT)); }

  void set_ptr(T *new_ptr) { ptr = reinterpret_cast<uintptr_t>(new_ptr) | get_mark(); }

  uintptr_t get_mark() const { return ptr & (ONE_BIT | TWO_BIT); }

  void clear_mark() { ptr = ptr & ~(ONE_BIT | TWO_BIT); }

  void set_mark(uintptr_t mark) {
    assert(mark < (1 << 2));  // Marks should only occupy the bottom two bits
    clear_mark();
    ptr |= mark;
  }

  void set_mark_bit(int bit) {
    assert(bit == 1 || bit == 2);
    ptr |= (1 << (bit - 1));
  }

  bool get_mark_bit(int bit) const {
    assert(bit == 1 || bit == 2);
    return ptr & (1 << (bit - 1));
  }

 private:
  uintptr_t ptr;
};

}  // namespace psac

#endif //PSAC_MARKED_PTR_H
