#pragma once

#include <async/task.h>
#include <async/pool.h>
#include <async/platform.h>

#include <valgrind/valgrind.h>

namespace async {

class EventLoop {
  using TaskList = std::vector<Task *>;

  bool running;

  jmp_buf ctx;
  void * stack;

  TaskList tasks;
  size_t current;

  std::atomic<TaskId> task_id_counter;

  ThreadPool<> pool;

  struct {
    std::mutex          mutex;
    std::vector<TaskId> to_wake;
    std::vector<Task *> to_start;
  } sync;

public:
  EventLoop()
    : running(true),
      ctx(),
      stack(nullptr),
      tasks({}),
      current(0),
      task_id_counter(0) {}

  ~EventLoop() {
    clear();
  }

  void clear() {
    for (const auto task : tasks) {
      delete task;
    }

    tasks.clear();
    stack = nullptr;
    current = 0;
  }

  TaskId addTask(const Task::Worker& fn) {
    const auto id = task_id_counter++;
    auto t = new Task(id, fn);

    std::lock_guard lock(sync.mutex);
    sync.to_start.push_back(t);
    return id;
  }

  Task * getTask(const TaskId id) const {
    for (auto& task : tasks) {
      if (task->getId() == id) {
        return task;
      }
    }

    return nullptr;
  }

  Task * getCurrentTask() const {
    return tasks[current];
  }

  void addThreadedTask(const ThreadPool<>::Worker& worker) {
    pool.scheduleTask(worker);
  }

  void cycle() {
    stack = platform::get_stack();

    processSync();

    if (current == tasks.size()) {
      current = 0;
    }

    const auto task = tasks[current];

    if (task->state == Task::State::INIT) {
      if (setjmp(ctx)) {
        ++current;

        platform::set_stack(stack);
        return;
      }

#if VALGRIND_DEBUG
      task->stack.id = VALGRIND_STACK_REGISTER(task->stack.data, task->stack.data + task->stack.size);
#endif

      uintptr_t stack_top = reinterpret_cast<uintptr_t>(task->stack.data + task->stack.size);
      stack_top = (stack_top & ~0xFL) - 256;
      platform::set_stack(reinterpret_cast<void*>(stack_top));

      task->state = Task::State::EXEC;
      task->worker();
      task->state = Task::State::DONE;

      platform::set_stack(stack);
      return;
    }

    if (task->state == Task::State::DONE) {
#if VALGRIND_DEBUG
      VALGRIND_STACK_DEREGISTER(task->stack.id);
#endif

      delete task;

      tasks.erase(tasks.begin() + current);
      if (tasks.size() == 0) running = false;

      platform::set_stack(stack);
      return;
    }

    if (task->state == Task::State::EXEC) {
      if (!setjmp(ctx)) {
        longjmp(task->ctx, 1);
      }
    }

    ++current;

    platform::set_stack(stack);
  }

  void runUntilCompleted() {
    while (running) {
      cycle();
    }
  }

private:
  TaskId getNextTaskId() {
    const auto id = task_id_counter.load();
    task_id_counter.store(id + 1);
    return id;
  }

  void notifyReady(const TaskId id) {
    std::lock_guard lock(sync.mutex);
    sync.to_wake.push_back(id);
  }

  void processSync() {
    std::lock_guard lock(sync.mutex);

    for (const TaskId id : sync.to_wake) {
      getTask(id)->state = Task::State::EXEC;
    }

    sync.to_wake.clear();

    for (auto task : sync.to_start) {
      tasks.push_back(task);
    }

    sync.to_start.clear();
  }

  friend void exit();
  friend void wait();
  friend void notify(const TaskId id);
  friend void yield();
};

}
