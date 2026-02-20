#include <async/async.h>
#include <test.h>


extern test::TestSuite async_tests;

static bool unlocked_through_event = false;
static bool unlocked_through_event1 = false;
static bool unlocked_through_event2 = false;


async_task_n(async_event_waiter, "waiter", std::shared_ptr<async::Event> ev) {
  ev->wait();
  unlocked_through_event = true;
}

async_task(async_event_notifier, std::shared_ptr<async::Event> ev) {
  TEST_ASSERT(!unlocked_through_event, "Waiter should be blocked");
  TEST_ASSERT(async::getGlobalLoop()->getTask("waiter")->getState() == async::Task::State::WAIT, "Waiter task should be waiting");
  ev->notifyOne();
}

TEST(async_tests, async_event, "Async event wait/notifyOne test") {
  unlocked_through_event = false;


  try {
    async::run([] {
      auto ev = async::Event::create();

      auto f1 = async_event_waiter(ev);
      auto f2 = async_event_notifier(ev);

      async::gather(f1, f2);
    });

    TEST_ASSERT(unlocked_through_event, "Task didn't finish");
  } catch (test::AssertionError e) {
    async::getGlobalLoop()->clear();
    throw;
  }
}


async_task_n(async_event_waiter1, "waiter1", std::shared_ptr<async::Event> ev) {
  ev->wait();
  unlocked_through_event1 = true;
}

async_task_n(async_event_waiter2, "waiter2", std::shared_ptr<async::Event> ev) {
  ev->wait();
  unlocked_through_event2 = true;
}

async_task(async_event_notifier_multi, std::shared_ptr<async::Event> ev) {
  TEST_ASSERT(!unlocked_through_event1, "Waiter1 should be blocked");
  TEST_ASSERT(!unlocked_through_event2, "Waiter2 should be blocked");
  TEST_ASSERT(async::getGlobalLoop()->getTask("waiter1")->getState() == async::Task::State::WAIT, "Waiter1 task should be waiting");
  TEST_ASSERT(async::getGlobalLoop()->getTask("waiter2")->getState() == async::Task::State::WAIT, "Waiter2 task should be waiting");
  ev->notifyAll();
}

TEST(async_tests, async_event_multi, "Async event wait/notifyAll test") {
  try {
    async::run([] {
      auto ev = async::Event::create();

      auto f1 = async_event_waiter1(ev);
      auto f2 = async_event_waiter2(ev);
      auto f3 = async_event_notifier_multi(ev);

      async::gather(f1, f2, f3);
    });

    TEST_ASSERT(unlocked_through_event1, "Task 1 didn't finish");
    TEST_ASSERT(unlocked_through_event1, "Task 2 didn't finish");
  } catch (test::AssertionError e) {
    async::getGlobalLoop()->clear();
    throw;
  }
}


async_task_n(async_event_waiter_ensure, "waiter", std::shared_ptr<async::Event> ev) {
  async::yield();
  ev->wait();
  unlocked_through_event = true;
}

async_task(async_event_notifier_ensure, std::shared_ptr<async::Event> ev) {
  TEST_ASSERT(!unlocked_through_event, "Waiter should be blocked");
  TEST_ASSERT(async::getGlobalLoop()->getTask("waiter")->getState() == async::Task::State::EXEC, "Waiter task should not be waiting");
  ev->ensureNotifyOne();
}

TEST(async_tests, async_event_ensure, "Async event wait/ensureNotifyOne test") {
  unlocked_through_event = false;

  try {
    async::run([] {
      auto ev = async::Event::create();

      auto f1 = async_event_waiter_ensure(ev);
      auto f2 = async_event_notifier_ensure(ev);

      async::gather(f1, f2);
    });

    TEST_ASSERT(unlocked_through_event, "Task didn't finish");
  } catch (test::AssertionError e) {
    async::getGlobalLoop()->clear();
    throw;
  }
}
