#include <test.h>
#include <signal.h>

auto async_tests = test::TestSuite("async");

int main() {
  // signal(SIGCHLD, SIG_IGN);
  // signal(SIGPIPE, SIG_IGN);

  return test::run(&async_tests);
}
