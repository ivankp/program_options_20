#include <iostream>

#include "macros.hh"
#include "program_options.hh"

struct test  {
  void operator()() { }
};
struct test2 {
  void operator()() { }
};
struct test3 {
};

namespace ivanp::po {

template <> class ref<test> {
  test& x;
public:
  explicit ref(test& x): x(x) { }
  void operator()() { }
};

template <> class ref<const test3> {
  const test3& x;
public:
  explicit ref(const test3& x): x(x) { }
  void operator()() { }
};

}

int main(int argc, char* argv[]) {
  bool b;
  test t;
  test2 t2;
  std::string s;

  namespace po = ivanp::po;
  ivanp::program_options(argc,argv,
    po::opt("a",[](){ TEST(__PRETTY_FUNCTION__) }),
    po::opt("a",b),
    po::opt("t",t),
    po::opt("t",std::move(t)),
    po::opt("t",test{}),
    po::opt("t",test2{}),
    po::opt("t",test3{}),
    po::opt("s",s),
    po::opt("t",t2)
  );
}
