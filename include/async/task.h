#pragma once

#include <functional>
#include <string>
#include <cstdint>
#include <csetjmp>

#ifndef ASYNC_TASK_DEFAULT_STACK_SIZE
#define ASYNC_TASK_DEFAULT_STACK_SIZE 65536
#endif

namespace async {

using TaskId = int64_t;

constexpr TaskId INVALID_TASK_ID = -1UL;

class Task {
public:
  enum class State {
    NONE = 0,
    INIT = 1,
    EXEC = 2,
    WAIT = 3,
    DONE = 4,
  };

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
      stack({.data = new uint8_t[stack_size], .size = stack_size}) {}

  ~Task() {
    delete[] stack.data;
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
