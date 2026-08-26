#pragma once

#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>

namespace async {

/**
 * @brief Implements a simple thread pool, with a fixed number of threads,
 *        which all block on a cv that is released when a task should be
 *        executed.
 *
 * @tparam T Worker function type
 */
template <typename T = std::function<void()>>
class ThreadPool {
  /** @brief Used to control the ThreadPool */
  enum Flag {
    FLAG_NONE      = 0,
    FLAG_STOPPED   = (1 << 0), /// ThreadPool is stopped
    FLAG_FINISH    = (1 << 1), /// Workers should finish current tasks & exit
    FLAG_TERMINATE = (1 << 2), /// Workers should exit immediately
  };

public:
  using Worker = T;

  /** @brief Creates all threads */
  explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
    : thread_count(thread_count), flags(FLAG_NONE)
  {
    for (size_t i = 0; i < thread_count; ++i) {
      threads.push_back(std::thread(&ThreadPool::worker, this));
    }
  }

  /** @brief Waits for threads to stop */
  ~ThreadPool() {
    if (!getFlag(FLAG_STOPPED)) {
      waitAll();
    }
  }

  /** @brief Pushed task into the queue and notifies workers */
  void scheduleTask(const T& task) {
    {
      auto lock = std::unique_lock<std::mutex>(pending.mutex);
      pending.queue.push(task);
    }
    notifier.notify_one();
  }

  /** @brief Sets finish (graceful stop) flag and calls `terminate()` */
  void waitAll() {
    setFlag(FLAG_FINISH, true);
    terminate();
  }

  /** @brief Sets terminate (abrupt stop) flag and calls `terminate()` */
  void terminateAll() {
    setFlag(FLAG_TERMINATE, true);
    terminate();
  }

  /** @brief Clear pending tasks */
  void clear() {
    auto lock = std::unique_lock<std::mutex>(pending.mutex);

    while (pending.queue.size()) {
      pending.queue.pop();
    }
  }

private:
  /**
   * @brief Worker that is ran by every thread managed by the pool
   *
   * In a loop, blocks on a condition_variable, waiting for a queue or a
   * terminate/finish flag. When a task is pushed into the queue, one
   * of the workers gets notified, unblocks, pops a task and executes it.
   * If a finish flag is present, the worker will try to execute remaining
   * pending tasks and then exit. If a terminate flag is present, the
   * worker will exit on the spot.
   */
  void worker() {
    while (true) {
      T task;
      {
        auto lock = std::unique_lock<std::mutex>(pending.mutex);

        notifier.wait(lock, [&] {
          return !pending.queue.empty() || getFlag(FLAG_TERMINATE | FLAG_FINISH);
        });

        if (getFlag(FLAG_TERMINATE)) return;
        if (getFlag(FLAG_FINISH) && pending.queue.empty()) return;

        task = pending.queue.front();
        pending.queue.pop();
      }
      task();
    }
  }

  /** @brief Wakes all threads and joins them one by one  */
  void terminate() {
    notifier.notify_all();

    for (auto& thread : threads) {
      thread.join();
    }

    threads.clear();
    setFlag(FLAG_STOPPED, true);
  }

  /** @brief Wrapper for atomic flag getter */
  bool getFlag(const uint8_t mask) const {
    uint8_t flags = this->flags.load();
    return (flags & mask) > 0;
  }

  /** @brief Wrapper for atomic flag setter */
  void setFlag(const uint8_t mask, const bool state) {
    uint8_t flags = this->flags.load();
    if (state) {
      flags |= mask;
    } else {
      flags &= ~mask;
    }
    this->flags.store(flags);
  }

private:
  /** @brief Thread count (may be unneeded) */
  size_t thread_count;

  /** @brief Thread objects */
  std::vector<std::thread> threads;

  /** @brief Queue for pending tasks, guarded by a mutex */
  struct {
    std::queue<T> queue;
    std::mutex    mutex;
  } pending;

  /** @brief Condition variable for worker notifications */
  std::condition_variable notifier;

  /** @brief Atomic ThreadPool flags */
  std::atomic<uint8_t> flags;
};

}
