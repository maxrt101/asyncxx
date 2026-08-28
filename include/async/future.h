#pragma once

#include <async/api.h>
#include <async/exc.h>
#include <async/internal/future_state.h>

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

  UniqueFutureState state;

  /** @brief Stores the result of the computation */
  Result result;

public:
  Future() : state(FutureState::create()), result() {}

  /** @brief Shortcut for a make_shared<Future> */
  static std::shared_ptr<Future> create() {
    return std::make_shared<Future>();
  }

  bool is_completed() const { return state->is_completed(); }
  bool is_cancelled() const { return state->is_cancelled(); }

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
    state->complete();
  }

  /** @brief Completes the future, notifying waiters */
  void complete() const {
    static_assert(std::is_void_v<T>, "complete() without args is only for Future<void>");
    state->complete();
  }

  /** @brief Cancels the future, notifying waiters */
  void cancel() const {
    state->cancel();
  }

  /** @brief Blocks caller task until the future is completed */
  auto await() {
    if (is_completed()) {
      return get();
    }

    state->saveWaiter(self()->getId());
    wait();

    if (is_cancelled()) throw CancelledException();
    if (!is_completed()) throw FutureFailedToCompleteException();

    if constexpr (!std::is_void_v<T>) {
      return result;
    }
  }

  /**
   * @brief Returns pointer to FutureState for this instance
   *
   * @warning Use with care
   */
  [[nodiscard]] FutureState * getState() const {
    return state.get();
  }
};

template <typename T = void>
using SharedFuture = std::shared_ptr<Future<T>>;

template <typename T = void>
using UniqueFuture = std::unique_ptr<Future<T>>;

}
