#ifndef IVAN_PROGRAM_OPTIONS_HH
#define IVAN_PROGRAM_OPTIONS_HH

#include <cstring>
#include <tuple>
#include <type_traits>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <string>

namespace ivan {
namespace po {

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
  void operator()(const char* str) { x = str; }
};

template <typename T>
requires std::is_arithmetic_v<T>
class ref<T> {
  T& x;
public:
  ref(T& x): x(x) { }
  void operator()(const char* str) {
    const char* end = str + strlen(str);
    const auto [p,e] = std::from_chars(str,end,x);
    switch (e) {
      case std::errc::invalid_argument:
        throw std::runtime_error(cat("invalid value: \"",str,"\""));
      case std::errc::result_out_of_range:
        throw std::runtime_error(cat("value out-of-range: \"",str,"\""));
      default: ;
    }
    if (p != end) throw std::runtime_error(cat(
      "unconverted bytes in argument \"",str,"\""
    ));
  }
};

// factory ----------------------------------------------------------
template <typename T, typename... Args>
class factory {
  T& x;
  std::tuple<Args&&...> args;
public:
  factory(T& x, Args&&... args): x(x), args(static_cast<Args&&>(args)...) { }
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
template <typename F>
struct opt {
  const char* def; // comma-separated
  F f;

  template <typename T>
  requires std::is_same_v<
    std::remove_cvref_t<F>,
    std::remove_cvref_t<T> >
  opt(const char* def, T&& f): def(def), f(static_cast<T&&>(f)) { }

  template <typename T>
  requires std::is_same_v<F,ref<T>>
  opt(const char* def, T& x): def(def), f(ref<T>(x)) { }

  template <typename T, typename... Args>
  requires std::is_same_v<F,factory<T,Args...>>
  opt(const char* def, T& x, Args&&... args)
  : def(def), f(factory<T,Args...>(x,static_cast<Args&&>(args)...)) { }
};

template <callable F>
opt(const char*, F&&) -> opt<F&&>;

template <not_callable T>
opt(const char*, T&) -> opt<ref<T>>;

template <typename T, typename... Args>
opt(const char*, T&, Args&&...) -> opt<factory<T,Args...>>;

// option function type erasure -------------------------------------
typedef bool (*pass_arg_t)(void*,const char*);
typedef void (*no_arg_t)(void*);

template <typename F>
bool pass_arg_f(void* f, const char* arg) {
  if constexpr (std::is_same_v< std::invoke_result_t<F,const char*>, void >) {
    (*reinterpret_cast<F*>(f))(arg);
    return false;
  } else {
    return (*reinterpret_cast<F*>(f))(arg);
  }
}
template <typename F>
void no_arg_f(void* f) {
  (*reinterpret_cast<F*>(f))();
}

// match option with definition -------------------------------------
bool match(const char* def, const char* arg, const char* end) {
  const char* a;
next_opt:
  a = arg;
next_char:
  const char b = *def;
  if (a==end && (b=='\0' || b==',')) return true;
  if (*a==b) {
    ++a;
    ++def;
    goto next_char;
  } else {
    def = strchr(def,',');
    if (!def) return false;
    ++def;
    goto next_opt;
  }
}

// program_options function -----------------------------------------
void program_options(
  int argc,
  const char* const* argv,
  auto&& free_arg,
  opt<auto>&&... opts
) {
  void* f = nullptr;
  pass_arg_t pass_arg = nullptr;
  no_arg_t no_arg = nullptr;
  const char *arg, *end, *prev_arg;
  bool all_dashes;

  auto prev_no_arg = [&]{
    if (no_arg) { // previous option without argument
      (*no_arg)(f);
      no_arg = nullptr;
    } else if (f) {
      throw std::runtime_error(cat(
        "option -", std::string_view( prev_arg-(prev_arg[1]!='\0'), end ),
        " requires an argument"
      ));
    }
  };

  auto find_opt = [&]{
    return ([&](auto&& opt){
      if (!match(opt.def,arg,end)) return false;

      using F = std::remove_reference_t<decltype(opt.f)>;
      if constexpr (callable_arg<F>) { // takes arguments
        pass_arg = pass_arg_f<F>;
        f = reinterpret_cast<void*>(&opt.f);
        if constexpr (callable_no_arg<F>) {
          if (*end == '\0') no_arg = no_arg_f<F>;
        }
      } else { // doesn't take arguments
        // throw only if long option
        // short option tail may consist of additional options
        if (*end && end-arg > 1) throw std::runtime_error(cat(
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
    prev_arg = arg;
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
          if (*end) goto malformed;
        } else if (end-arg < 2) {
          arg -= 2;
malformed:
          throw std::runtime_error(cat(
            "malformed option ", arg
          ));
        }
        if (!find_opt()) throw std::runtime_error(cat(
          "unexpected option -", std::string_view( arg+(all_dashes?1:-1), end )
        ));
        if (*end && pass_arg) { arg = end + 1; goto option_argument; }
      } else { // short options (-)
        for (;;) { // loop over consecutive short options
          const char c = *arg;
          if (c == '\0') { --arg; break; }
          if (c == '-') throw std::runtime_error("invalid option -");
          end = arg + 1;
          if (!find_opt()) throw std::runtime_error(cat(
            "unexpected option -", std::string_view(arg,1)
          ));
          if (*++arg && pass_arg) goto option_argument;
        }
      }
    } else { // not an option
argument: ;
      if (pass_arg) {
option_argument: ;
        if (!(*pass_arg)(f,arg)) {
          pass_arg = nullptr;
          f = nullptr;
        }
        no_arg = nullptr;
      } else {
        free_arg(arg);
      }
    }
  }

  prev_arg = arg;
  prev_no_arg(); // last option without argument
}

} // end namespace po

using po::program_options;

} // end namespace ivan

#endif
