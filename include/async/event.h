#pragma once

#include <async/task.h>
#include <async/api.h>

namespace async {

/**
 * @brief Event - Asynchronous task synchronization primitive
 *
 * Event can be waited on, or notified by.
 * If an event is waited on - the task saved it's ID into the event &
 * calls `async::wait()`, which transitions it into the WAIT state,
 * suspending it's execution, until it is unblocked (event posted).
 * If an event is notified by - one task calls `notify*()`, which
 * call `async::notify()` on each appropriate task (first one or all).
 *
 * Example:
 * @code{.c}
 *  auto ev = async::Event::create();
 *
 *  auto waiter = async::task([&] {
 *    ev->wait();
 *  });
 *
 *  auto notifier = async::task([&] {
 *    ev->notifyOne();
 *  });
 *
 *  async::gather(waiter, notifier);
 * @endcode
 */
class Event {
  /** @brief Mutex-guarded list of tasks, that are waiting on this event */
  struct {
    std::vector<TaskId> list;
    std::mutex          mutex;
  } waiters;

public:
  Event() = default;
  ~Event() = default;

  /** @brief Shortcut to a make_shared<Event> */
  static std::shared_ptr<Event> create() {
    return std::make_shared<Event>();
  }

  /**
   * @brief Notifies first task from a list
   *
   * @returns @c true if a task was waken, @c false otherwise
   */
  bool notifyOne() {
    auto lock = std::unique_lock(waiters.mutex);

    if (waiters.list.empty()) {
      return false;
    }

    auto task = waiters.list.front();
    waiters.list.erase(waiters.list.begin());

    notify(task);

    return true;
  }

  /** @brief Notifies all waiting tasks */
  void notifyAll() {
    auto lock = std::unique_lock(waiters.mutex);

    for (const auto& task : waiters.list) {
      notify(task);
    }

    waiters.list.clear();
  }

  /** @brief Wait on this event */
  void wait() {
    saveWaiter();
    async::wait();
  }

  /** @brief Guarantees that one task is woken. Does this by calling
   *         `notifyOne()` in a loop while yielding on each pass */
  void ensureNotifyOne() {
    while (!notifyOne()) {
      yield();
    }
  }

private:
  /** @brief Saves current task ID into waiters list */
  void saveWaiter() {
    auto lock = std::unique_lock(waiters.mutex);

    waiters.list.push_back(self()->getId());
  }
};

}
