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

// BUG when p == 1.0 ?
template <typename T>
inline int find_idx(const T &A, float p, int l = -1, int r = -1)
{
  assert(A.size() > 0);
  l = l == -1 ? 0 : l;
  r = r == -1 ? (int)(A.size() - 1) : r;
  tfn::vec2i L{l, l + 1};
  tfn::vec2i R{r - 1, r};
  while (L.y <= R.x) { /* do not overlap */
    tfn::vec2i M;
    M.x = (R.x + L.x) / 2;
    M.y = (R.y + L.y) / 2;
    assert(M.y - M.x == 1);
    if (A[M.y].p < p) {
      L.x = M.x + 1;
      L.y = M.y + 1;
    } else if (p < A[M.x].p) {
      R.x = M.x - 1;
      R.y = M.y - 1;
    } else {
      return M.y;
    }
  }
  assert(L.x == R.x);
  assert(L.y == R.y);
  return R.y;
}

inline float lerp(const float &l, const float &r, const float &pl, const float &pr, const float &p)
{
  const float dl = std::abs(pr - pl) > 0.0001f ? (p - pl) / (pr - pl) : 0.f;
  const float dr = 1.f - dl;
  return l * dr + r * dl;
}

} // namespace tfn
