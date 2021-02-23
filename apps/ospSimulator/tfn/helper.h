// ======================================================================== //
// Copyright Qi Wu, since 2019                                              //
// Copyright SCI Institute, University of Utah, 2018                        //
// ======================================================================== //
#pragma once

#include <cmath>

namespace tfn {

template <class T>
inline const T &clamp(const T &v, const T &lo, const T &hi)
{
  return (v < lo) ? lo : (hi < v ? hi : v);
}

template <typename T>
inline int find_idx(const T &A, float p, int l = -1, int r = -1)
{
  l = l == -1 ? 0 : l;
  r = r == -1 ? A.size() - 1 : r;
  int m = (r + l) / 2;
  if (A[l].p > p) {
    return l;
  } else if (A[r].p <= p) {
    return r + 1;
  } else if ((m == l) || (m == r)) {
    return m + 1;
  } else {
    if (A[m].p <= p) {
      return find_idx(A, p, m, r);
    } else {
      return find_idx(A, p, l, m);
    }
  }
}

inline float lerp(const float &l, const float &r, const float &pl, const float &pr, const float &p)
{
  const float dl = std::abs(pr - pl) > 0.0001f ? (p - pl) / (pr - pl) : 0.f;
  const float dr = 1.f - dl;
  return l * dr + r * dl;
}

} // namespace tfn
