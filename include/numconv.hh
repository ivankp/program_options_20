#ifndef IVAN_NUMCONV_HH
#define IVAN_NUMCONV_HH

#include <type_traits>
#include <stdexcept>
#include <charconv>

#include "string.hh"

namespace ivan {

template <typename>
struct ntos;

template <typename T>
requires std::is_arithmetic_v<T>
struct ntos<T> {
  unsigned char n;
  char s[
    std::is_integral_v<T>
    ? ((616*sizeof(T)) >> 8) + 1 + std::is_signed_v<T>
    : sizeof(T)*2 + 8
  ];
  ntos(T x) noexcept: n(std::to_chars(s,s+sizeof(s),x).ptr-s) { }
  operator std::string_view() const noexcept { return { s, n }; }
};

template <>
struct ntos<bool> {
  bool x;
  ntos(bool x) noexcept: x(x) { }
  operator std::string_view() const noexcept { return x ? "true" : "false"; }
};

template <typename T>
ntos(T x) -> ntos<T>;

// ------------------------------------------------------------------

template <typename T>
requires std::is_arithmetic_v<T>
T ston(std::string_view s) {
  T x;
  const char* end = s.data() + s.size();
  const auto [p,e] = std::from_chars(s.data(),end,x);
  switch (e) {
    case std::errc::invalid_argument:
      throw std::runtime_error(cat("invalid value: \"",s,'"'));
    case std::errc::result_out_of_range:
      throw std::runtime_error(cat("value out-of-range: \"",s,'"'));
    default: ;
  }
  if (const auto r = end-p)
    throw std::runtime_error(cat(ntos(r)," bytes unconverted: \"",s,'"'));
  return x;
}

}
#endif
