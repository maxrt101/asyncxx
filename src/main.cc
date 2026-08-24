#include <test.h>

auto async_tests = test::TestSuite("async");

int main(int argc, char ** argv) {
  std::vector<std::string> tests;

  for (int i = 1; i < argc; ++i) {
    tests.push_back(argv[i]);
  }

  return test::run(&async_tests, tests);
}
