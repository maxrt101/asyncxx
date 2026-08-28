#pragma once

#include <async/api.h>

#include <atomic>
#include <vector>
#include <mutex>

namespace async {

/**
 * @brief Non-templated, sharable class containing state of the Future
 */
class FutureState {
  /** @brief Atomic flag which signifies the computation completion */
  std::atomic<bool> completed;

  /** @brief Atomic flag which signifies the computation cancellation */
  std::atomic<bool> cancelled;

  /** @brief List of those who wait for this future to finish */
  struct {
    std::vector<TaskId> list;
    std::mutex          mutex;
  } waiters;

public:
  FutureState()
    : completed(false),
      cancelled(false),
      waiters() {}

  ~FutureState() {
    // TODO: Send CancelledException
  }

  static std::unique_ptr<FutureState> create() {
    return std::make_unique<FutureState>();
  }

  bool is_completed() const { return completed.load(); }
  bool is_cancelled() const { return cancelled.load(); }

  void complete() { completed.store(true); notifyAll(); }
  void cancel()   { cancelled.store(true); notifyAll(); }

private:
  /** @brief Saves current task ID into waiters list */
  void saveWaiter(const TaskId id) {
    auto lock = std::unique_lock(waiters.mutex);

    waiters.list.push_back(id);
  }

  /** @brief Notifies all waiting tasks */
  void notifyAll() {
    auto lock = std::unique_lock(waiters.mutex);

    for (const auto& task : waiters.list) {
      notify(task);
    }
  }

  template <typename T>
  friend class Future;
};

using UniqueFutureState = std::unique_ptr<FutureState>;

}