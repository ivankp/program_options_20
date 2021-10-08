#ifndef IVANP_PROGRAM_OPTIONS_HH
#define IVANP_PROGRAM_OPTIONS_HH

#include <utility>
#include <type_traits>

namespace ivanp {
namespace po {

template <typename>
class ref;

template <>
class ref<bool> {
  bool& x;
public:
  explicit ref(bool& x): x(x) { }
  void operator()() { x = !x; }
};

template <typename T>
requires std::is_assignable_v<T&,const char*>
class ref<T> {
  T& x;
public:
  explicit ref(T& x): x(x) { }
  void operator()(const char* val) { x = val; }
};

template <typename T>
concept Ref = requires { ref<T>(std::declval<T&>()); };
template <typename T>
concept CRef = Ref<const T>;

template <typename F>
concept OptFcn =
  std::is_invocable_v<F> ||
  std::is_invocable_v<F,const char*>;

template <OptFcn F>
struct opt {
  const char* s;
  F f;

  template <typename T>
  requires std::is_same_v<
    std::remove_cvref_t<F>,
    std::remove_cvref_t<T> >
  opt(const char* s, T&& f): s(s), f(std::forward<T>(f)) { }

  template <Ref T>
  requires std::is_same_v<F,ref<T>>
  opt(const char* s, T& x): s(s), f(ref<T>(x)) { }

  template <CRef T>
  requires std::is_same_v<F,ref<const T>>
  opt(const char* s, const T& x): s(s), f(ref<const T>(x)) { }

  void operator()() requires requires { f(); } { f(); }
  void operator()(const char* arg) requires requires { f(arg); } { f(arg); }
};

template <OptFcn F>
opt(const char*, F&&) -> opt<F>;

template <Ref T>
opt(const char*, T&) -> opt<ref<T>>;

template <CRef T>
opt(const char*, const T&) -> opt<ref<const T>>;

}

template <typename... T>
void program_options(int argc, char** argv, po::opt<T>&&... opts) {
  TEST(__PRETTY_FUNCTION__)
}

}

#endif
