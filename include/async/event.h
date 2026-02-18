#pragma once

#include <async/task.h>
#include <async/api.h>

namespace async {

class Event {
  struct {
    std::vector<TaskId> list;
    std::mutex          mutex;
  } waiters;

public:
  Event() = default;
  ~Event() = default;

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

  void notifyAll() {
    auto lock = std::unique_lock(waiters.mutex);

    for (const auto& task : waiters.list) {
      notify(task);
    }

    waiters.list.clear();
  }

  void wait() {
    saveWaiter();
    async::wait();
  }

  void ensureNotifyOne() {
    while (!notifyOne()) {
      yield();
    }
  }

private:
  void saveWaiter() {
    auto lock = std::unique_lock(waiters.mutex);

    waiters.list.push_back(self()->getId());
  }
};

}
