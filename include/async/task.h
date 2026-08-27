#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <cstdint>
#include <csetjmp>

/**
 * @brief Default task stack size (redefinable)
 * TODO: Set per-platform?
 */
#ifndef ASYNC_TASK_DEFAULT_STACK_SIZE
#define ASYNC_TASK_DEFAULT_STACK_SIZE 65536
#endif

namespace async {

/** @brief Alias for task ID */
using TaskId = int64_t;

/** @brief Invalid Task ID */
constexpr TaskId INVALID_TASK_ID = -1UL;

/**
 * @brief Singleton that manges allocated stacks, reusing them instead of
 *        freeing and re-allocating each time
 */
class StackPool {
  struct {
    std::unordered_map<uint8_t *, size_t> free;
    std::unordered_map<uint8_t *, size_t> used;
  } stacks;

  static inline std::shared_ptr<StackPool> instance;

public:
  StackPool() = default;

  /**
   * @brief Returns memory to the OS
   */
  ~StackPool() {
    for (const auto & map : {stacks.free, stacks.used}) {
      for (const auto& item : map) {
        delete[] item.first;
      }
    }

    stacks.free.clear();
    stacks.used.clear();
  }

  /** @brief Retrieve (and lazy-initialize if needed) StackPool instance */
  static std::shared_ptr<StackPool> get() {
    if (!instance) {
      instance = std::make_shared<StackPool>();
    }

    return instance;
  }

  /**
   * @brief Allocation request. Will search `stacks.free` before resorting to
   *        allocating new memory
   */
  uint8_t * alloc(const size_t size) {
    uint8_t * ptr = nullptr;

    for (const auto& item : stacks.free) {
      if (item.second == size) {
        ptr = item.first;
        stacks.free.erase(ptr);
        break;
      }
    }

    if (!ptr) {
      ptr = new uint8_t[size];
    }

    stacks.used[ptr] = size;

    return ptr;
  }

  /**
   * @brief Move allocated `ptr` from `stacks.used` to `stacks.free`. Won't
   *        return memory to the system
   */
  void free(uint8_t * ptr) {
    const auto size = stacks.used[ptr];
    stacks.used.erase(ptr);
    stacks.free[ptr] = size;
  }
};

/**
 * @brief Task Control Block (or Task Context) for an asynchronous task
 */
class Task {
public:
  /** @brief Task State */
  enum class State {
    NONE = 0, /// Uninitialized
    INIT = 1, /// Initialized, waits for startup
    EXEC = 2, /// Executing/Ready for execution
    WAIT = 3, /// Waiting for wakeup
    DONE = 4, /// Finished
  };

  /** @brief Task worker function type */
  using Worker = std::function<void()>;

private:
  State       state;
  TaskId      id;
  std::string name;
  Worker      worker;
  jmp_buf     ctx;

  struct {
    uint8_t * data;
    size_t    size;
#if VALGRIND_DEBUG
    int id;
#endif
  } stack;

public:
  Task(const TaskId id, Worker worker, const std::string& name = "<?>", const size_t stack_size = ASYNC_TASK_DEFAULT_STACK_SIZE)
    : state(State::INIT),
      id(id),
      name(name),
      worker(std::move(worker)),
      ctx(),
      stack({.data = StackPool::get()->alloc(stack_size), .size = stack_size}) {}

  ~Task() {
    StackPool::get()->free(stack.data);
    stack.data = nullptr;
    stack.size = 0;
  }

  void setName(const std::string& name) {
    this->name = name;
  }

  [[nodiscard]] std::string getName() const {
    return name;
  }

  [[nodiscard]] TaskId getId() const {
    return id;
  }

  [[nodiscard]] State getState() const {
    return state;
  }

private:
  friend class EventLoop;

  friend void exit();
  friend void wait();
  friend void notify(TaskId id);
  friend void yield();
};

}
