#include <iostream>

#include "macros.hh"
#include "program_options.hh"

int main(int argc, char* argv[]) {
  bool x = false, y = true, z = false;
  std::string s;
  int i = 0;
  unsigned u = 0;
  double f = 0;

  { using ivan::po::opt;
    ivan::program_options(argc-1,argv+1,
      opt("x",x),
      opt("bool",x),
      opt("y",y,false),
      opt("z",z,true),
      opt("s",s),
      opt("str",s),
      opt("i",i),
      opt("u",u),
      opt("f",f)
    );
  }

  TEST(x)
  TEST(y)
  TEST(z)
  TEST(s)
  TEST(i)
  TEST(u)
  TEST(f)
}
