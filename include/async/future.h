#pragma once

#include <mutex>

#include <async/task.h>
#include <async/exc.h>
#include <async/api.h>

namespace async {

struct FutureNotReadyException final : std::runtime_error {
  FutureNotReadyException() : std::runtime_error("Future must be completed to perform this action") {}
};

struct FutureFailedToCompleteException final : std::runtime_error {
  FutureFailedToCompleteException() : std::runtime_error("Future failed to complete") {}
};

/**
 * @brief Represents a result of as asynchronous task, which is yet to be
 *        computed. Future can also represent a computation, which produces
 *        no result, then it's just a marker for the end of that computation.
 *        Future can also be waited on, than task(s) which wait for it are
 *        blocked, until the computation yields a result.
 *
 * @tparam T Computation result type
 */
template <typename T = void>
class Future {
  struct Empty {};

  /** @brief Alias for computation result, resolved to `Empty` if T=void */
  using Result = std::conditional_t<std::is_void_v<T>, Empty, T>;

  /** @brief Atomic flag which signifies the computation completion */
  std::atomic<bool> completed;

  /** @brief Atomic flag which signifies the computation cancellation */
  std::atomic<bool> cancelled;

  /** @brief List of those who wait for this future to finish */
  struct {
    std::vector<TaskId> list;
    std::mutex          mutex;
  } waiters;

  /** @brief Stores the result of the computation */
  Result result;

public:
  Future()
    : completed(false),
      cancelled(false),
      waiters(),
      result() {}

  /** @brief Shortcut for a make_shared<Future> */
  static std::shared_ptr<Future> create() {
    return std::make_shared<Future>();
  }

  bool is_completed() const {
    return completed.load();
  }

  bool is_cancelled() const {
    return cancelled.load();
  }

  /** @brief Returns result, if T!=void, if future is completed, otherwise throws */
  auto get() {
    if (is_cancelled()) throw CancelledException();
    if (!is_completed()) throw FutureNotReadyException();

    if constexpr (!std::is_void_v<T>) {
      return result;
    }
  }

  /** @brief Completes the future, saving the result and notifying waiters */
  template<typename U = T>
  void complete(std::enable_if_t<!std::is_void_v<U>, U> val) {
    result = val;
    completed.store(true);
    notifyAll();
  }

  /** @brief Completes the future, notifying waiters */
  void complete() {
    static_assert(std::is_void_v<T>, "complete() without args is only for Future<void>");
    completed.store(true);
    notifyAll();
  }

  /** @brief Cancels the future, notifying waiters */
  void cancel() {
    cancelled.store(true);
    notifyAll();
  }

  /** @brief Blocks caller task until the future is completed */
  auto await() {
    if (is_completed()) {
      return get();
    }

    saveWaiter();
    wait();

    if (is_cancelled()) throw CancelledException();
    if (!is_completed()) throw FutureFailedToCompleteException();

    if constexpr (!std::is_void_v<T>) {
      return result;
    }
  }

private:
  /** @brief Saves current task ID into waiters list */
  void saveWaiter() {
    auto lock = std::unique_lock(waiters.mutex);

    waiters.list.push_back(self()->getId());
  }

  /** @brief Notifies all waiting tasks */
  void notifyAll() {
    auto lock = std::unique_lock(waiters.mutex);

    for (auto& task : waiters.list) {
      notify(task);
    }
  }
};

template <typename T = void>
using SharedFuture = std::shared_ptr<Future<T>>;

template <typename T = void>
using UniqueFuture = std::unique_ptr<Future<T>>;

}
