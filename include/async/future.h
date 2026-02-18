#pragma once

#include <mutex>

#include <async/task.h>
#include <async/api.h>

namespace async {

template <typename T = void>
class Future {
  struct Empty {};

  using Result = std::conditional_t<std::is_void_v<T>, Empty, T>;

  std::atomic<bool> completed;

  struct {
    std::vector<TaskId> list;
    std::mutex          mutex;
  } waiters;

  Result result;

public:
  Future()
    : completed(false),
      waiters(),
      result() {}

  bool is_completed() const {
    return completed.load();
  }

  auto get() {
    if constexpr (!std::is_void_v<T>) {
      return result;
    }
  }

  template<typename U = T>
  void complete(std::enable_if_t<!std::is_void_v<U>, U> val) {
    result = val;
    completed.store(true);
    notifyAll();
  }

  void complete() {
    static_assert(std::is_void_v<T>, "complete() without args is only for Future<void>");
    completed.store(true);
    notifyAll();
  }

  auto await() {
    if (is_completed()) {
      return get();
    }

    saveWaiter();
    wait();

    if (!is_completed()) throw std::runtime_error("failed");

    if constexpr (!std::is_void_v<T>) {
      return result;
    }
  }

private:
  void saveWaiter() {
    auto lock = std::unique_lock(waiters.mutex);

    waiters.list.push_back(self()->getId());
  }

  void notifyAll() {
    auto lock = std::unique_lock(waiters.mutex);

    for (auto& task : waiters.list) {
      notify(task);
    }
  }
};


}
