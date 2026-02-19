#include <async/async.h>
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

  async::run([] { async_task_fn(); });

  TEST_ASSERT(has_async_task_ran, "Test function didn't run");
}


async_task_r(async_task_ret_fn, int) {
  return 42;
}

TEST(async_tests, async_fn_ret, "Async task function that returns test") {
  int res = 0;

  async::run([&res] { res = async_task_ret_fn()->await(); });

  TEST_ASSERT(res == 42, "Test function didn't run");
}


async_task(async_task_arg_fn, int a) {
  async_result = a;
}

TEST(async_tests, async_fn_arg, "Async task function that has args test") {
  async_result = 0;

  async::run([] { async_task_arg_fn(42)->await(); });

  TEST_ASSERT(async_result == 42, "Test function didn't run");
}


async_task_r(async_task_arg_ret_fn, int, int a) {
  return a;
}

TEST(async_tests, async_fn_arg_ret, "Async task function that has args and returns test") {
  int res = 0;

  async::run([&res] { res = async_task_arg_ret_fn(42)->await(); });

  TEST_ASSERT(res == 42, "Test function didn't run");
}


TEST(async_tests, async_await_void, "Async task await test (void)") {
  has_async_task_ran = false;

  async::run([] { async_task_fn()->await(); });

  TEST_ASSERT(has_async_task_ran, "Test function didn't run");
}


TEST(async_tests, async_await_val, "Async task await test (value)") {
  int res = 0;

  async::run([&res] { res = async_task_ret_fn()->await(); });

  TEST_ASSERT(res == 42, "Test function didn't run");
}


TEST(async_tests, async_yield, "Async task yield") {
  int res = 0;

  async::run([&res] {  async::yield(); res = 42; });

  TEST_ASSERT(res == 42, "Test function didn't run");
}


async_task(async_yielder) {
  yield_counter = 1;
  async::yield();
  yield_counter = 2;
  async::yield();
  yield_counter = 3;
}

async_task(async_checker) {
  has_yielded = yield_counter == 1;
}

TEST(async_tests, async_yield_2, "Async task yield (multi-stage)") {
  yield_counter = 0;
  has_yielded = false;

  async::run([] {
    async::gather(async_yielder(), async_checker());
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
  });

  TEST_ASSERT(!after_exit, "Task didn't exit");
}


async_task_n(async_wait, "waiter") {
  async::wait();
  waiter_finished = true;
}

async_task(async_notify) {
  TEST_ASSERT(!waiter_finished, "Task shouldn't be finished before being woken");
  TEST_ASSERT(async::getGlobalLoop()->getTask("waiter")->getState() == async::Task::State::WAIT, "Waiter task should be waiting");
  async::notify(async::getGlobalLoop()->getTask("waiter")->getId());
}

TEST(async_tests, async_wait_notify, "Async wait/notify test") {
  after_exit = false;

  try {
    async::run([] {
      async::gather(
        async_wait(),
        async_notify()
      );
    });

    TEST_ASSERT(waiter_finished, "Task didn't finish");
  } catch (test::AssertionError e) {
    async::getGlobalLoop()->clear();
    throw;
  }
}
