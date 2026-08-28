#include <async/async.h>
#include <test.h>


extern test::TestSuite async_tests;
static int future_value = 0;


TEST(async_tests, async_future_void, "Async future happy flow (void)") {
  auto f = async::Future<>();

  TEST_ASSERT(!f.is_completed(), "Future shouldn't be completed");
  f.complete();
  TEST_ASSERT(f.is_completed(), "Future should be completed");
}


TEST(async_tests, async_future_val, "Async future happy flow (value)") {
  auto f = async::Future<int>();

  TEST_ASSERT(!f.is_completed(), "Future shouldn't be completed");
  f.complete(42);
  TEST_ASSERT(f.is_completed(), "Future should be completed");
  TEST_ASSERT(f.get() == 42, "Future values doesn't match");
}


TEST(async_tests, async_future_get_not_completed, "Async future .get() on a non-completed value") {
  auto f = async::Future<int>();

  try {
    f.get();
    TEST_ASSERT(false, "Future should not be completed");
  } catch (async::FutureNotReadyException) {}
}


async_task_n(future_waiter, "waiter", std::shared_ptr<async::Future<int>> f) {
  f->await();
}

async_task(fake_future_completer) {
  async::notify(async::getGlobalLoop()->getTask("waiter")->getId());
}

TEST(async_tests, async_future_await_failed, "Async future .await() failed to complete") {
  auto f = async::Future<int>::create();

  try {
    async::run([&f] {
      auto t1 = future_waiter(f);

      async::yield();

      auto t2 = fake_future_completer();

      async::gather(t1, t2);
    });

    TEST_ASSERT(false, "Future should not be completed");
  } catch (async::FutureFailedToCompleteException&) {}

  async::getGlobalLoop()->clear();
}


async_task(async_future_waiter, std::shared_ptr<async::Future<int>> f) {
  future_value = f->await();
}

async_task(async_future_completer, std::shared_ptr<async::Future<int>> f) {
  f->complete(42);
}

TEST(async_tests, async_future_await, "Async future .await() test") {
  auto f = async::Future<int>::create();

  async::run([&f] {
    async::gather(async_future_waiter(f), async_future_completer(f));
  });

  TEST_ASSERT(future_value == 42, "Future value is invalid");
}

TEST(async_tests, async_future_cancel, "Canceled future throws an exception, if awaited") {
  auto f = async::Future<int>::create();
  bool cancel_exc_happened_await = false;
  bool cancel_exc_happened_get = false;

  async::run([&] {
    auto worker = async::task([&] {
      try {
        f->await();
      } catch (async::CancelledException&) {
        cancel_exc_happened_await = true;
      }

      try {
        f->get();
      } catch (async::CancelledException&) {
        cancel_exc_happened_get = true;
      }
    });

    auto canceller = async::task([&f] {
      // Make sure worker awaits
      async::yield();
      f->cancel();
    });

    async::gather(worker, canceller);
  });

  TEST_ASSERT(cancel_exc_happened_await, "Cancel exception should've been triggered on f->await()");
  TEST_ASSERT(cancel_exc_happened_get, "Cancel exception should've been triggered on f->get()");
  TEST_ASSERT(f->is_cancelled(), "Future must have cancel flag set");
}
