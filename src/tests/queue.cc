#include <async/async.h>
#include <async/queue.h>
#include <test.h>


extern test::TestSuite async_tests;

TEST(async_tests, async_q_single_consumer_wait, "Async queue with single consumer test (waiting for value)") {
  int result = 0;

  async::run([&] {
    auto q = async::Queue<int>::create();

    auto consumer = async::task([&q, &result] {
      TEST_ASSERT(q->empty(), "Queue must be empty at this point");
      result = q->get()->await();
    });

    auto producer = async::task([&q] {
      // Make sure that consumer call .get() before value is put here
      async::yield();
      q->put(42);
    });

    async::gather(consumer, producer);
  });

  TEST_ASSERT_EQ(result, 42, "Consumer must receive '42' from producer");
}


TEST(async_tests, async_q_single_consumer_nowait, "Async queue with single consumer test (value already there)") {
  int result = 0;

  async::run([&] {
    auto q = async::Queue<int>::create();

    auto consumer = async::task([&q, &result] {
      // Make sure that produces puts value into queue, before it is retrieved
      async::yield();
      TEST_ASSERT(!q->empty(), "Queue must NOT be empty at this point");
      result = q->get()->await();
    });

    auto producer = async::task([&q] {
      q->put(42);
    });

    async::gather(consumer, producer);
  });

  TEST_ASSERT_EQ(result, 42, "Consumer must receive '42' from producer");
}


TEST(async_tests, async_q_multi_consumer, "Async queue with many consumers test") {
  int result1 = 0;
  int result2 = 0;

  async::run([&] {
    auto q = async::Queue<int>::create();

    auto consumer1 = async::task([&q, &result1] {
      result1 = q->get()->await();
    });

    auto consumer2 = async::task([&q, &result2] {
      result2 = q->get()->await();
    });

    auto producer = async::task([&q] {
      q->put(42);
      q->put(69);
    });

    async::gather(consumer1, consumer2, producer);
  });

  TEST_ASSERT_EQ(result1, 42, "Result1 must be 42");
  TEST_ASSERT_EQ(result2, 69, "Result2 must be 69");
}
