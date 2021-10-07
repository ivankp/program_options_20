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

template <> class default_f<test> {
  test& x;
public:
  explicit default_f(test& x): x(x) { }
  void operator()() { }
};

template <> class default_f<const test3> {
  const test3& x;
public:
  explicit default_f(const test3& x): x(x) { }
  void operator()() { }
};

}

int main(int argc, char* argv[]) {
  bool b;
  test t;
  test2 t2;

  namespace po = ivanp::po;
  ivanp::program_options(argc,argv,
    po::opt("a",[](){ TEST(__PRETTY_FUNCTION__) }),
    po::opt("a",b),
    po::opt("t",t),
    po::opt("t",std::move(t)),
    po::opt("t",test{}),
    po::opt("t",test2{}),
    po::opt("t",test3{})
    // po::opt("t",t2)
  );
}
