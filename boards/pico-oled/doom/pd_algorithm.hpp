// PCBKit: the three function templates rp2040-doom's renderer takes from <algorithm>, without a
// C++ standard library.
#pragma once
namespace std {
template <class T> constexpr const T &min(const T &a, const T &b) { return b < a ? b : a; }
template <class T> constexpr const T &max(const T &a, const T &b) { return a < b ? b : a; }
template <class T> void swap(T &a, T &b) { T t = a; a = b; b = t; }
}
