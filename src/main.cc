#include <test.h>

auto async_tests = test::TestSuite("async");

int main() {
  return test::run(&async_tests);
}
