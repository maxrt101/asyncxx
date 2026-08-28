#include <async/async.h>
#include <async/macros.h>
#include <test.h>


extern test::TestSuite async_tests;

static bool has_async_task_ran = false;
static int async_result = 0;
static int yield_counter = 0;
static bool has_yielded = false;
static bool task1_ran = false;
static bool task2_ran = false;
static bool after_exit = false;
static bool waiter_finished = false;

template <>
std::string test::impl::to_string(const async::Task::State& val) {
  switch (val) {
    case async::Task::State::NONE: return "NONE";
    case async::Task::State::INIT: return "INIT";
    case async::Task::State::EXEC: return "EXEC";
    case async::Task::State::WAIT: return "WAIT";
    case async::Task::State::DONE: return "DONE";
    default:                       return "<?>";
  }
}


TEST(async_tests, async_task, "Async task test") {
  bool has_ran = false;

  async::run([&has_ran] {
    has_ran = true;
  });

  TEST_ASSERT(has_ran, "Test didn't run");
}


async_task(async_task_fn) {
  has_async_task_ran = true;
}

TEST(async_tests, async_fn, "Async task function test") {
  has_async_task_ran = false;

  async::run(async_task_fn);

  TEST_ASSERT(has_async_task_ran, "Test function didn't run");
}


async_task_r(async_task_ret_fn, int) {
  return 42;
}

TEST(async_tests, async_fn_ret, "Async task function that returns test") {
  int res = 0;

  res = async::run<int>(async_task_ret_fn);

  TEST_ASSERT_EQ(res, 42, "Test function didn't run");
}


async_task(async_task_arg_fn, int a) {
  async_result = a;
}

TEST(async_tests, async_fn_arg, "Async task function that has args test") {
  async_result = 0;

  async::run<void>(async_task_arg_fn, 42);

  TEST_ASSERT_EQ(async_result, 42, "Test function didn't run");
}


async_task_r(async_task_arg_ret_fn, int, int a) {
  return a;
}

TEST(async_tests, async_fn_arg_ret, "Async task function that has args and returns test") {
  int res = 0;

  res = async::run<int>(async_task_arg_ret_fn, 42);

  TEST_ASSERT_EQ(res, 42, "Test function didn't run");
}


TEST(async_tests, async_await_void, "Async task await test (void)") {
  has_async_task_ran = false;

  async::run(async_task_fn);

  TEST_ASSERT(has_async_task_ran, "Test function didn't run");
}


TEST(async_tests, async_await_val, "Async task await test (value)") {
  int res = 0;

  res = async::run<int>(async_task_ret_fn);

  TEST_ASSERT_EQ(res, 42, "Test function didn't run");
}


TEST(async_tests, async_yield, "Async task yield") {
  int res = 0;

  async::run([&res] {  async::yield(); res = 42; });

  TEST_ASSERT_EQ(res, 42, "Test function didn't run");
}


async_task_n(async_yielder, "yielder") {
  yield_counter = 1;
  async::yield();
  yield_counter = 2;
  async::yield();
  yield_counter = 3;
}

async_task_n(async_checker, "checker") {
  has_yielded = yield_counter == 1;
}

TEST(async_tests, async_yield_2, "Async task yield (multi-stage)") {
  yield_counter = 0;
  has_yielded = false;

  async::run([] {
    auto f1 = async_yielder();
    auto f2 = async_checker();
    async::gather(f1, f2);
  });

  TEST_ASSERT(has_yielded, "Test hasn't yielded");
}


async_task(async_task1) {
  task1_ran = true;
}

async_task(async_task2) {
  task2_ran = true;
}

TEST(async_tests, async_gather, "Async gather test") {
  task1_ran = false;
  task2_ran = false;

  async::run([] {
    async::gather(async_task1(), async_task2());
  });

  TEST_ASSERT(task1_ran, "Test 1 didn't run");
  TEST_ASSERT(task2_ran, "Test 2 didn't run");
}


async_task(async_exit) {
  async::exit();
  after_exit = true;
}

TEST(async_tests, async_exit, "Async exit test") {
  after_exit = false;

  async::run([] {
    async_exit();
    // Needed, because if async_exit is not awaited - MainTaskExitedException
    // will be thrown, if run() won't await it - the future will never get
    // completed, and a deadlock will happen
    // TODO: Throw CancelledException
    async::yield();
  });

  TEST_ASSERT(!after_exit, "Task didn't exit");
}


async_task_n(async_wait, "waiter") {
  async::wait();
  waiter_finished = true;
}

async_task(async_notify) {
  TEST_ASSERT(!waiter_finished, "Task shouldn't be finished before being woken");
  TEST_ASSERT_EQ(async::getGlobalLoop()->getTask("waiter")->getState(), async::Task::State::WAIT, "Waiter task should be waiting");
  async::notify(async::getGlobalLoop()->getTask("waiter")->getId());
}

TEST(async_tests, async_wait_notify, "Async wait/notify test") {
  after_exit = false;

  try {
    async::run([] {
      auto f1 = async_wait();
      auto f2 = async_notify();
      async::gather(f1, f2);
    });

    TEST_ASSERT(waiter_finished, "Task didn't finish");
  } catch (test::AssertionError&) {
    async::getGlobalLoop()->clear();
    throw;
  }
}
