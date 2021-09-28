#ifndef VIDI_BITS_ARRAY_H
#define VIDI_BITS_ARRAY_H

#include <cassert>
#include <cstring>

#ifndef VIDI_BITS_ARRAY__ATOMIC_OR
#define VIDI_BITS_ARRAY__ATOMIC_OR __sync_fetch_and_or
#endif

namespace vidi {

struct BitsArray
{
#if defined(_WIN32)
  using TYPE = LONG;
#else
  using TYPE = uint32_t;
#endif

  BitsArray(size_t s) : size(s)
  {
    assert(s > 0);
  }

  BitsArray() : size(0) {}

  ~BitsArray()
  {
    // dealloc();
  }

  void alloc()
  {
    data = new TYPE[size / nbits + 1]();
  }

  void dealloc()
  {
    delete[] data;
    data = nullptr;
  }

  void reset()
  {
    const size_t n = size / nbits + 1;
    memset(data, 0, n * sizeof(TYPE));
  }

  bool safe_set(size_t i, bool v)
  {
    const size_t block = i / nbits;
    const TYPE bits = TYPE(1) << (i % nbits);
#if defined(_WIN32)
    const TYPE r = InterlockedOr(data + block, bits);
#else
    const TYPE r = VIDI_BITS_ARRAY__ATOMIC_OR(data + block, bits);
#endif
    return r & bits;
  }

  int operator[](size_t i) const
  {
    const size_t block = i / nbits;
    const TYPE bits = TYPE(1) << (i % nbits);
    const TYPE r = *(data + block);
    return (r & bits) ? 1 : 0;
  }

  size_t length() const
  {
    return size;
  }

  void resize(size_t n)
  {
    dealloc();
    size = n;
    alloc();
  }

  void *unsafe_internal_data()
  {
    return data;
  }

  const void *unsafe_internal_data() const
  {
    return data;
  }

  size_t unsafe_internal_size() const
  {
    return sizeof(TYPE) * (size / nbits + 1);
  }

 private:
  const int nbits = sizeof(TYPE) * 8;
  TYPE *data{nullptr};
  size_t size;
};

}  // namespace vidi

#endif  // VIDI_BITS_ARRAY_H
