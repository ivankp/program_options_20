#include <iostream>

#include "macros.hh"
#define IVANP_BOOL_OPT_TOGGLE
#include "program_options.hh"

int main(int argc, char* argv[]) {
  bool b = false;
  std::string s;

  { using ivanp::po::opt;
    ivanp::program_options(argc,argv,
      opt("b",b),
      opt("s",s)
    );
  }

  TEST(b)
  TEST(s)
}
