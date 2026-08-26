#include <async/async.h>
#include <async/io/proc.h>
#include <test.h>


extern test::TestSuite async_tests;


TEST(async_tests, async_proc, "Async process runner test") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc = async::io::Process::create("true");
    const auto res = proc->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 0, "Process exit code should be 0");
  TEST_ASSERT_EQ(out, "", "Process stdout should be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}


TEST(async_tests, async_proc_invalid, "Async process non-existent command test") {
  bool ok = false;
  int code = -1;

  async::run([&] {
    const auto proc = async::io::Process::create("abcdef");
    const auto res = proc->await();

    ok = res->ok;
    code = res->exit_code;
  });

  TEST_ASSERT(!ok, "Process shouldn't run successfully");
  TEST_ASSERT_NE(code, 0, "Process exit code should be 0");
}


TEST(async_tests, async_proc_exit, "Async process exit code test") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc = async::io::Process::create("bash -c 'exit 42'");
    const auto res = proc->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 42, "Process exit code should be 42");
  TEST_ASSERT_EQ(out, "", "Process stdout should be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}


TEST(async_tests, async_proc_stdout, "Async process stdout capture test") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc = async::io::Process::create("echo -n 123");
    const auto res = proc->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 0, "Process exit code should be 0");
  TEST_ASSERT_EQ(out, "123", "Process stdout shouldn't be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}


TEST(async_tests, async_proc_stdin, "Async process stdin passthrough test") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc = async::io::Process::create("cat", "123");
    const auto res = proc->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 0, "Process exit code should be 0");
  TEST_ASSERT_EQ(out, "123", "Process stdout shouldn't be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}


TEST(async_tests, async_proc_dyn_stdin, "Async process dynamic stdin test") {
  bool ok = false;
  int code = -1;
  std::string out;

  async::run([&] {
    const auto proc = async::io::Process::create("cat");
    const auto [f, io] = proc->start();

    io->in->write("test123")->await();
    io->in->close();

    out = io->out->readAll()->await();

    const auto res = f->await();

    ok = res->ok;
    code = res->exit_code;
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 0, "Process exit code should be 0");
  TEST_ASSERT_EQ(out, "test123", "Process stdout shouldn't be empty");
}


TEST(async_tests, async_proc_chain, "Async process chain test") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc1 = async::io::Process::create("echo 123");
    const auto proc2 = async::io::Process::create("cat");
    const auto res = async::io::Process::chain(proc1, proc2)->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 0, "Process exit code should be 0");
  TEST_ASSERT_EQ(out, "123\n", "Process stdout shouldn't be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}


TEST(async_tests, async_proc_kill, "Async process kill test") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc = async::io::Process::create("bash -c 'while true; do done'");
    const auto [f, _] = proc->start();
    proc->kill();
    const auto res = f->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 0, "Process exit code should be 0");
  TEST_ASSERT_EQ(out, "", "Process stdout should be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}

static int process_worker() {
  printf("Hello from process!");
  fflush(stdout);
  return 42;
}

TEST(async_tests, async_proc_fn, "Async process running a fuction") {
  bool ok = false;
  int code = -1;
  std::string out, err;

  async::run([&] {
    const auto proc = async::io::Process::create(process_worker);
    const auto res = proc->await();

    ok = res->ok;
    code = res->exit_code;
    out = res->io->out->readAll()->await();
    err = res->io->err->readAll()->await();
  });

  TEST_ASSERT(ok, "Process should run successfully");
  TEST_ASSERT_EQ(code, 42, "Process exit code should be 42");
  TEST_ASSERT_EQ(out, "Hello from process!", "Process stdout should not be empty");
  TEST_ASSERT_EQ(err, "", "Process stderr should be empty");
}
