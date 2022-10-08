#ifndef IVAN_PROGRAM_OPTIONS_HH
#define IVAN_PROGRAM_OPTIONS_HH

#include <cstring>
#include <tuple>
#include <type_traits>
#include <exception>
#include <string_view>
#include <string>

namespace ivan {
namespace po {

// default callbacks for references ---------------------------------
template <typename>
class ref;

template <>
class ref<bool> {
  bool& x;
public:
  ref(bool& x): x(x) { }
  void operator()() { x = !x; }
};

template <typename T>
requires std::is_assignable_v<T&,const char*>
class ref<T> {
  T& x;
public:
  ref(T& x): x(x) { }
  bool operator()(const char* val) { x = val; return false; }
};

template <typename T>
requires std::is_arithmetic_v<T>
class ref<T> {
  T& x;
public:
  ref(T& x): x(x) { }
  // TODO: conversion function
  bool operator()(const char* val) { /*x = stox<T>(val);*/ return false; }
};

// factory ----------------------------------------------------------
template <typename T, typename... Args>
class factory {
  T& x;
  std::tuple<Args&&...> args;
public:
  explicit factory(T& x, Args&&... args): x(x), args(args...) { }
  bool operator()() { x = std::make_from_tuple<T>(args); return false; }
};

// concepts ---------------------------------------------------------
template <typename T>
concept callable_no_arg = std::is_invocable_v<T>;
template <typename T>
concept callable_arg = std::is_invocable_v<T,const char*>;
template <typename T>
concept callable = callable_no_arg<T> || callable_arg<T>;
template <typename T>
concept not_callable = !callable<T>;

// option definition ------------------------------------------------
/*
template <typename F>
struct opt_pass_arg {
  static void (pass_arg*)(void*,const char*) = nullptr;
};
template <typename F>
requires
struct opt_pass_arg {
  static void (pass_arg*)(void*,const char*)
  = [](void* self, const char* arg){
    reinterpret_cast<opt*>(self)->f(arg);
  };
};
*/

typedef bool (*pass_arg_t)(void*,const char*);
typedef void (*no_arg_t)(void*);

template <typename F>
struct opt_pass_arg {
  static constexpr pass_arg_t pass_arg = nullptr;
};
template <callable_arg F>
struct opt_pass_arg<F> {
  static constexpr pass_arg_t pass_arg = [](void* f, const char* arg){
    return (*reinterpret_cast<F*>(f))(arg);
  };
};

template <typename F>
struct opt_no_arg {
  static constexpr no_arg_t no_arg = nullptr;
};
template <callable_no_arg F>
struct opt_no_arg<F> {
  static constexpr no_arg_t no_arg = [](void* f){
    return (*reinterpret_cast<F*>(f))();
  };
};

template <typename F>
struct opt: opt_pass_arg<F>, opt_no_arg<F> {
  const char* def; // comma-separated
  F f;

  template <typename T>
  requires std::is_same_v<
    std::remove_cvref_t<F>,
    std::remove_cvref_t<T> >
  opt(const char* def, T&& f): def(def), f(std::forward<T>(f)) { }

  template <typename T>
  requires std::is_same_v<F,ref<T>>
  opt(const char* def, T& x): def(def), f(ref<T>(x)) { }

  template <typename T, typename... Args>
  requires std::is_same_v<F,factory<T,Args...>>
  opt(const char* def, T& x, Args&&... args)
  : def(def), f(factory<T,Args...>(x,args...)) { }
};

template <callable F>
opt(const char*, F&&) -> opt<F>;

template <not_callable T>
opt(const char*, T&) -> opt<ref<T>>;

template <typename T, typename... Args>
opt(const char*, T&, Args&&...) -> opt<factory<T,Args...>>;

// match option with definition -------------------------------------
bool match(const char* def, const char* arg, const char* end) {
  const char* _a;
next_opt:
  _a = arg;
next_char:
  const char a = *_a;
  const char b = *def;
  if (_a==end && (b=='\0' || b==',')) return true;
  if (a==b) {
    ++_a;
    ++def;
    goto next_char;
  } else {
    def = strchr(def,',');
    if (!def) return false;
    ++def;
    goto next_opt;
  }
}

// cat --------------------------------------------------------------
template <typename... T>
requires ((std::is_same_v<T,std::string_view> && ...))
[[nodiscard,gnu::always_inline]]
inline std::string cat(T... x) {
  std::string s((x.size() + ...),{});
  char* p = s.data();
  ( ( memcpy(p, x.data(), x.size()), p += x.size() ), ...);
  return s;
}

template <typename... T>
requires (!(std::is_same_v<T,std::string_view> && ...))
[[nodiscard,gnu::always_inline]]
inline std::string cat(T... x) {
  return cat(std::string_view(x)...);
}

// program_options function -----------------------------------------
// template <typename Argv, typename... F>
// requires
//   std::is_pointer_v<Argv> &&
//   std::is_pointer_v<std::remove_pointer_t<Argv>> &&
//   std::is_same_v<
//     std::remove_cv_t<std::remove_pointer_t<std::remove_pointer_t<Argv>>>,
//     char
//   >
void program_options(
  int argc,
  const char* const* argv,
  auto&& free_arg,
  opt<auto>&&... opts
) {
  void* f = nullptr;
  pass_arg_t pass_arg = nullptr;
  no_arg_t no_arg = nullptr;
  const char *arg, *end;
  bool all_dashes;

  auto prev_no_arg = [&]{
    if (no_arg) { // previous option without argument
      (*no_arg)(f);
      no_arg = nullptr;
    } else if (f) throw std::runtime_error(cat(
      "option -", std::string_view(arg,end), " requires an argument"
    ));
  };

  auto find_opt = [&]{
    return ([&](auto&& opt){
      if (!match(opt.def,arg,end)) return false;

      if constexpr (opt.pass_arg) { // takes arguments
        pass_arg = opt.pass_arg;
        f = reinterpret_cast<void*>(&opt.f);
        if constexpr (opt.no_arg) {
          if (*end == '\0') no_arg = opt.no_arg;
        }
      } else { // doesn't take arguments
        // throw only if long option
        // short option tail may consist of additional options
        if (end-arg > 1) throw std::runtime_error(cat(
          "option -", std::string_view( arg+(all_dashes?1:-1), end ),
          " does not take arguments"
        ));
        pass_arg = nullptr;
        f = nullptr;
        opt.f();
      }
      return true;
    }(opts) || ... );
  };

  for (int i=0; i<argc; ++i) {
    arg = argv[i];
    if (*arg == '-') { // option
      ++arg;
      if (*arg == '\0') { // isolated dash is not an option
        --arg;
        goto argument;
      }

      prev_no_arg();

      if (*arg == '-') { // long option (--)
        all_dashes = true;
        end = ++arg;
        for (;; ++end) {
          const char c = *end;
          if (c=='\0' || c=='=') break;
          // if (c!='-') all_dashes = false;
          all_dashes &= c=='-';
        }
        if (all_dashes) {
          arg -= 2;
          if (*end) throw std::runtime_error(cat(
            "malformed option ", arg
          ));
        }
        if (!find_opt()) goto unexpected;
        if (*end && pass_arg) { // *end == '='
          arg = end + 1;
          goto argument;
        }
      } else { // short options (-)
        for (;;) { // loop over consecutive short options
          const char c = *arg;
          if (c == '\0') break;
          if (c == '-') throw std::runtime_error("invalid option -");
          end = arg + 1;
          if (!find_opt()) goto unexpected;
          if (*++arg && pass_arg) goto argument;
        }
      }
    } else { // not an option
      if (pass_arg) {
argument: ;
        if (!(*pass_arg)(f,arg))
          pass_arg = nullptr;
      } else {
        free_arg(arg);
      }
    }
  }

  prev_no_arg(); // last option without argument

  return;

unexpected:
  throw std::runtime_error(cat(
    "unexpected option -", std::string_view(arg,end)
  ));
}

} // end namespace po

using po::program_options;

} // end namespace ivan

#endif
