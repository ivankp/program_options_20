#include <iostream>

#include "macros.hh"
#include "program_options.hh"

int main(int argc, char* argv[]) {
  bool x = false, y = true, z = false;
  std::string s;
  int i = 0;
  unsigned u = 0;
  float f = 0;
  double g = 0;

  try {
    using ivan::po::opt;
    ivan::program_options(argc-1,argv+1,
      [](const char* arg){ TEST(arg) },
      opt("x,b",x),
      opt("bool",x),
      opt("y",y,false),
      opt("z",z,true),
      opt("s,str",s),
      opt("i",i),
      opt("u",u),
      opt("f",f),
      opt("g",[&g](const char* s){ g = atof(s)*2; }),
      opt("--",[]{ puts("double dash"); }),
      opt("---",[]{ puts("triple dash"); })
    );
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  TEST(x)
  TEST(y)
  TEST(z)
  TEST(s)
  TEST(i)
  TEST(u)
  TEST(f)
  TEST(g)
}
