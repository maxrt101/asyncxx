#pragma once

#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>

namespace async {

template <typename T = std::function<void()>>
class ThreadPool {
  enum Flag {
    FLAG_NONE      = 0,
    FLAG_STOPPED   = (1 << 0),
    FLAG_FINISH    = (1 << 1),
    FLAG_TERMINATE = (1 << 2),
  };

public:
  using Worker = T;

  explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
    : thread_count(thread_count), flags(FLAG_NONE)
  {
    for (size_t i = 0; i < thread_count; ++i) {
      threads.push_back(std::thread(&ThreadPool::worker, this));
    }
  }

  ~ThreadPool() {
    if (!getFlag(FLAG_STOPPED)) {
      waitAll();
    }
  }

  void scheduleTask(const T& task) {
    {
      auto lock = std::unique_lock<std::mutex>(pending.mutex);
      pending.queue.push(task);
    }
    notifier.notify_one();
  }

  void waitAll() {
    setFlag(FLAG_FINISH, true);
    terminate();
  }

  void terminateAll() {
    setFlag(FLAG_TERMINATE, true);
    terminate();
  }

  void clear() {
    auto lock = std::unique_lock<std::mutex>(pending.mutex);

    while (pending.queue.size()) {
      pending.queue.pop();
    }
  }

private:
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

  void terminate() {
    notifier.notify_all();

    for (auto& thread : threads) {
      thread.join();
    }

    threads.clear();
    setFlag(FLAG_STOPPED, true);
  }

  bool getFlag(const uint8_t mask) const {
    uint8_t flags = this->flags.load();
    return (flags & mask) > 0;
  }

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
  size_t thread_count;
  std::vector<std::thread> threads;

  struct {
    std::queue<T> queue;
    std::mutex    mutex;
  } pending;

  std::condition_variable notifier;

  std::atomic<uint8_t> flags;
};

}
