#ifndef MAKRA_H_
#define MAKRA_H_

#include <iostream>
#include <type_traits>
#include <utility>

#define sim template < class c
#define ris return * this
#define dor > debug & operator <<
#define eni(x) sim > typename \
  std::enable_if<sizeof dud<c>(0) x 1, debug&>::type operator<<(c i) {
sim > struct rge { c b, e; };
sim > rge<c> range(c i, c j) { return rge<c>{i, j}; }
sim > auto dud(c* x) -> decltype(std::cerr << *x, 0);
sim > char dud(...);
struct debug {
#ifdef LOCAL
~debug() { std::cerr << std::endl; }
eni(!=) std::cerr << std::boolalpha << i; ris; }
eni(==) ris << range(std::begin(i), std::end(i)); }
sim, class b dor(std::pair < b, c > d) {
  ris << "(" << d.first << ", " << d.second << ")";
}
sim dor(rge<c> d) {
  *this << "[";
  for (auto it = d.b; it != d.e; ++it)
    *this << ", " + 2 * (it == d.b) << *it;
  ris << "]";
}
#else
sim dor(const c&) { ris; }
#endif
};
#define imie(...) " [" << #__VA_ARGS__ ": " << (__VA_ARGS__) << "] "

#endif  // MAKRA_H_
