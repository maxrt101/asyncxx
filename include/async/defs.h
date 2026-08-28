/**
 * @brief Provides declarations (some of which are forward) of shared
 *        types/values that are used everywhere
 */
#pragma once

#include <functional>
#include <cstdint>

namespace async {

/** @brief Alias for task ID */
using TaskId = int64_t;

/** @brief Invalid Task ID */
constexpr TaskId INVALID_TASK_ID = -1UL;

/** @brief Task worker function type */
using TaskWorker = std::function<void()>;

class Task;
class EventLoop;
class FutureState;

template <typename T> class Future;

}
