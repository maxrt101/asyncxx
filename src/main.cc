#include <async/async.h>
#include <async/io/file.h>
#include <async/io/proc.h>
#include <cstdio>

static async::Event ev[2] = {async::Event(), async::Event()};

async_task(file_read_task) {
  printf("Reading file...\n");

  const auto f = async::io::File::create("test.txt", "r");
  const auto s = f->readAll()->await();

  printf("=== File Contents ===\n%s\n", s.c_str());

  ev[0].notifyOne();
}

async_task(file_read_line_task) {
  printf("Reading file...\n");

  const auto f = async::io::File::create("test.txt", "r");

  while (!f->end()) {
    auto line = f->readLine()->await();
    printf("Line: '%s'\n", line.c_str());
  }

  ev[0].notifyOne();
}

async_task(wait_task) {
  for (int i = 0; i < 10; ++i) {
    printf("Waiting... (cycle %d)\n", i);
    async::yield();
  }

  ev[0].wait();

  printf("Exiting wait task...\n");
}

async_task(echo_task) {
  printf("Starting echo process...\n");

  const auto p = async::io::Process::create("echo -n 123");
  const auto [ok, exit_code, out, err] = p->await();

  printf("Echo process exited\n");
  printf("  ok:        %s\n", ok ? "yes" : "no");
  printf("  exit_code: %d\n", exit_code);
  printf("  out:       '%s'\n", out->readAll()->await().c_str());
  printf("  err:       '%s'\n", err->readAll()->await().c_str());

  ev[1].ensureNotifyOne();
}

async_task(python_task) {
  printf("Starting py process...\n");

  const auto p = async::io::Process::create("python3 -c 'print(input(), end=\"\")'", "123\n");
  const auto [ok, exit_code, out, err] = p->await();

  ev[1].wait();

  printf("Py process exited\n");
  printf("  ok:        %s\n", ok ? "yes" : "no");
  printf("  exit_code: %d\n", exit_code);
  printf("  out:       '%s'\n", out->readAll()->await().c_str());
  printf("  err:       '%s'\n", err->readAll()->await().c_str());
}

async_main() {
  printf("async test\n");

  async::gather(
    file_read_line_task(),
    wait_task(),
    echo_task(),
    python_task()
  );

  printf("async test ended\n");

  return 0;
}
