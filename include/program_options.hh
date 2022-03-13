#ifndef IVAN_PROGRAM_OPTIONS_HH
#define IVAN_PROGRAM_OPTIONS_HH

#include <utility> // declval
#include <tuple>
#include "numconv.hh"

namespace ivan {
namespace po {

template <typename>
class ref;

// ref specializations ----------------------------------------------
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
requires std::is_arithmetic_v<T>
class ref<T> {
  T& x;
public:
  explicit ref(T& x): x(x) { }
  void operator()(const char* val) { x = stox<T>(val); }
};

// ref concepts -----------------------------------------------------
template <typename T>
concept Ref = requires { ref<T>(std::declval<T&>()); };
template <typename T>
concept CRef = Ref<const T>;

template <typename F>
concept OptFcn =
  std::is_invocable_v<F> ||
  std::is_invocable_v<F,const char*>;

// factory ----------------------------------------------------------
template <typename T, typename... Args>
class factory {
  T& x;
  std::tuple<Args&&...> args;
public:
  explicit factory(T& x, Args&&... args)
  : x(x), args(std::forward<Args>(args)...) { }
  void operator()() { x = std::make_from_tuple<T>(args); }
};

// opt --------------------------------------------------------------
template <OptFcn F>
struct opt {
  const char* s;
  F f;

  template <typename T>
  requires std::is_same_v<
    std::remove_cvref_t<F>,
    std::remove_cvref_t<T> >
  opt(const char* s, T&& f): s(s), f(std::forward<T>(f)) { }

  template <CRef T>
  requires std::is_same_v<F,ref<const T>>
  opt(const char* s, const T& x): s(s), f(ref<const T>(x)) { }

  template <Ref T>
  requires std::is_same_v<F,ref<T>>
  opt(const char* s, T& x): s(s), f(ref<T>(x)) { }

  template <typename T, typename... Args>
  requires std::is_same_v<F,factory<T,Args...>>
  opt(const char* s, T& x, Args&&... args)
  : s(s), f(factory<T,Args...>(x,std::forward<Args>(args)...)) { }

  void operator()() requires requires { f(); } { f(); }
  void operator()(const char* arg) requires requires { f(arg); } { f(arg); }
};

template <OptFcn F>
opt(const char*, F&&) -> opt<F>;

template <CRef T>
opt(const char*, const T&) -> opt<ref<const T>>;

template <Ref T>
opt(const char*, T&) -> opt<ref<T>>;

template <typename T, typename... Args>
opt(const char*, T&, Args&&...) -> opt<factory<T,Args...>>;

}

// NOTE: short option must not be '-', long option must not contain =

// program_options function -----------------------------------------
template <typename... F>
void program_options(int argc, char** argv, po::opt<F>&&... opts) {
  for (int i=0; i<argc; ++i) {
    char* arg = argv[i];
    if (*arg == '-') { // option
      ++arg;
      if (*arg == '-') { // long option (--)
        ++arg;
        if (*arg=='\0') { // --
          // TODO: find a way to elegantly handle --
          throw std::runtime_error("unexpected option --");
        } else {
          if (!([&](auto& o){
            static constexpr bool val[2] {
              requires { o(); },
              requires { o(arg); }
            }; // OptFcn guarantees at least one is true
            const size_t n = strcspn(arg,"=");
            const bool eq = arg[n];
            if (memcmp(arg,o.s,n)) // skip other opts
              return false;
            if constexpr(!val[1]) {
              if (eq) throw std::runtime_error(cat(
                "option --",o.s," does not accept a value"));
              o();
            } else if (eq) {
              o(arg+n+1);
            } else if (i+1 < argc && argv[i+1][0]!='-') {
              // TODO
              throw std::runtime_error(
                "consuming next argument not implemented");
            } else {
              if constexpr (val[0]) o();
              else throw std::runtime_error(cat(
                "option --",o.s," requires a value"));
            }
            return true;
          }(opts) || ... )) throw std::runtime_error(cat(
            "unexpected option --",arg));
        }
      } else { // short option (-)
        if (*arg=='\0') { // -
          // TODO: find a way to elegantly handle -
          throw std::runtime_error("unexpected option -");
        } else {
          for (;;) {
            const char c = *arg++;
            if (!([&](auto& o){
              static constexpr bool val[2] {
                requires { o(); },
                requires { o(arg); }
              }; // OptFcn guarantees at least one is true
              if (!(o.s[0]==c && o.s[1]=='\0')) // skip other opts
                return false;
              if constexpr(!val[1]) o();
              else if (*arg!='\0') { // not last option in arg
                o(arg);
                arg = nullptr;
              } else if (i+1 < argc && argv[i+1][0]!='-') {
                // TODO
                throw std::runtime_error(
                  "consuming next argument not implemented");
              } else {
                if constexpr (val[0]) o();
                else throw std::runtime_error(cat(
                  "option -",c," requires a value"));
              }
              return true;
            }(opts) || ... )) throw std::runtime_error(cat(
              "unexpected option -",c));
            if (!arg || !*arg) break;
          }
        }
      }
    } else { // not an option
    }
  }
}

}

#endif
