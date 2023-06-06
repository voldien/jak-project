#pragma once

template <typename T>
T align(T current, T alignment, T offset = 0) {
  T _align_tmp = current + (alignment - (current % alignment));
  return _align_tmp + offset;
}