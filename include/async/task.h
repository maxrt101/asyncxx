#pragma once

#include <async/defs.h>
#include <async/internal/stack_pool.h>
#include <async/internal/future_state.h>

#include <functional>
#include <memory>
#include <string>
#include <csetjmp>

/**
 * @brief Default task stack size (redefinable)
 * TODO: Set per-platform?
 */
#ifndef ASYNC_TASK_DEFAULT_STACK_SIZE
#define ASYNC_TASK_DEFAULT_STACK_SIZE 65536
#endif

namespace async {

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

private:
  State       state;
  TaskId      id;
  std::string name;
  TaskWorker  worker;
  jmp_buf     ctx;

  struct {
    uint8_t * data;
    size_t    size;
#if VALGRIND_DEBUG
    int id;
#endif
  } stack;

  /** @brief Optional FutureState of the Future, which manages the result of this task */
  FutureState * result_future;

public:
  Task(const TaskId id, TaskWorker worker, const std::string& name = "<?>", const size_t stack_size = ASYNC_TASK_DEFAULT_STACK_SIZE)
    : state(State::INIT),
      id(id),
      name(name),
      worker(std::move(worker)),
      ctx(),
      stack({.data = StackPool::get()->alloc(stack_size), .size = stack_size}),
      result_future(nullptr) {}

  ~Task() {
    StackPool::get()->free(stack.data);
    stack.data = nullptr;
    stack.size = 0;
    result_future = nullptr;
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
