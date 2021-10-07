#ifndef IVANP_PROGRAM_OPTIONS_HH
#define IVANP_PROGRAM_OPTIONS_HH

#include <string_view>
#include <type_traits>

namespace ivanp {
namespace po {

template <typename> class default_f;
template <> class default_f<bool> {
  bool& x;
public:
  explicit default_f(bool& x): x(x) { }
  void operator()() { x = !x; }
};

template <typename F>
struct opt {
  const char* s;
  F f;

  template <typename T>
  requires std::is_invocable_v<T> || std::is_invocable_v<T,const char*>
  opt(const char* s, T&& f): s(s), f(std::forward<T>(f)) { }

  template <typename T>
  // requires std::is_constructible_v<default_f<T>,T&>
  // requires requires { sizeof(default_f<T>); }
  requires requires { default_f<T>(std::declval<T&>()); }
  opt(const char* s, T& x): s(s), f(default_f<T>(x)) { }

  template <typename T>
  // requires std::is_constructible_v<default_f<T>,T&>
  // requires requires { sizeof(default_f<T>); }
  requires requires { default_f<const T>(std::declval<const T&>()); }
  opt(const char* s, const T& x): s(s), f(default_f<const T>(x)) { }

  void operator()() requires requires { f(); } { f(); }
  void operator()(char* arg) requires requires { f(arg); } { f(arg); }
};

template <typename F>
requires std::is_invocable_v<F> || std::is_invocable_v<F,const char*>
opt(const char*, F&&) -> opt<F>;

template <typename T>
// requires requires { sizeof(default_f<T>); }
requires requires { default_f<T>(std::declval<T&>()); }
opt(const char*, T&) -> opt<default_f<T>>;

template <typename T>
// requires requires { sizeof(default_f<T>); }
requires requires { default_f<const T>(std::declval<const T&>()); }
opt(const char*, const T&) -> opt<default_f<const T>>;

}

template <typename... T>
void program_options(int argc, char** argv, po::opt<T>&&... opts) {
  TEST(__PRETTY_FUNCTION__)
}

}

#endif
