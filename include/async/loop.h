#pragma once

#include <async/defs.h>
#include <async/task.h>
#include <async/pool.h>
#include <async/platform.h>

#if VALGRIND_DEBUG
#include <valgrind/valgrind.h>
#endif

namespace async {

struct MainTaskExitedException final : std::runtime_error {
  MainTaskExitedException()
    : std::runtime_error("Main task exited while leaving other tasks unfinished") {}
};

/**
 * @brief Core class that manages async tasks
 */
class EventLoop {
  using TaskList = std::vector<Task *>;

  /** @brief Flag that controls if cycle() should be ran */
  bool running;

  /** @brief Main (sync) context. Saved by cycle() when execution reaches there
   *         and restored when cycle returns */
  jmp_buf ctx;

  /** @brief Main (sync) stack. Saved by cycle() when execution reaches there
   *         and restored when cycle returns */
  void * stack;

  /** @brief List of currently executing tasks */
  TaskList tasks;

  /** @brief Index into `tasks` for currently executing task */
  size_t current;

  /** @brief Atomic counter for unique task IDs
   * TODO: Make static inline? */
  std::atomic<TaskId> task_id_counter;

  /** @brief Main (parent of all) task id, used to check if main task exited,
   *         leaving other tasks unfinished */
  TaskId main_task_id = INVALID_TASK_ID;

  /** @brief ThreadPool instance for offloading blocking tasks */
  ThreadPool<> pool;

  /** @brief Synchronizing context */
  struct {
    /** @brief Synchronizing mutex (since for now EventLoop is a singleton,
     *         the mutex is needed because EventLoop APIs may be invoked from
     *         separate threads) */
    std::mutex mutex;

    /** @brief List of tasks, that should be started on next cycle (since it's
     *         not safe to start them in the middle of execution) */
    std::vector<TaskId> to_wake;

    /** @brief A list of task IDs that should wake on next cycle (since it's
     *         not safe to wake them in the middle of execution) */
    TaskList to_start;
  } sync;

public:
  /** @brief Default constructor */
  EventLoop()
    : running(true),
      ctx(),
      stack(nullptr),
      tasks({}),
      current(0),
      task_id_counter(0),
      main_task_id(0) {}

  /** @brief Destructor (clears all tasks) */
  ~EventLoop() {
    clear();
  }

  /** @brief Deletes tasks, resets state */
  void clear() {
    for (const auto task : tasks) {
      delete task;
    }

    tasks.clear();
    stack = nullptr;
    current = 0;
    running = true;
    main_task_id = INVALID_TASK_ID;
  }

  /** @brief Add optionally named task to the loop */
  TaskId addTask(const TaskWorker& fn, const std::string& name = "") {
    const auto id = task_id_counter++;
    const auto t = new Task(id, fn, name.empty() ? "Task-" + std::to_string(id) : name);

    std::lock_guard lock(sync.mutex);
    sync.to_start.push_back(t);
    return id;
  }

  /** @brief Retrieve TCB pointer by task ID */
  Task * getTask(const TaskId id) const {
    for (auto& task : tasks) {
      if (task->getId() == id) {
        return task;
      }
    }

    return nullptr;
  }

  /** @brief Retrieve TCB pointer by task name */
  Task * getTask(const std::string& name) const {
    for (auto& task : tasks) {
      if (task->name == name) {
        return task;
      }
    }

    return nullptr;
  }

  /** @brief Retrieve TCB pointer to current task */
  Task * getCurrentTask() const {
    return tasks[current];
  }

  /** @brief Offload a task to a separate thread */
  void addThreadedTask(const ThreadPool<>::Worker& worker) {
    pool.scheduleTask(worker);
  }

  /**
   * @brief Cycle the EventLoop once
   *
   * Stores stack pointer, processes `to_wake` & `to_start` lists, waking
   * and starting, requested in the previous cycle, tasks. Then retrieves
   * current task, check if it was just started, if so - initializes it (
   * creates stack, sets return longjmp point, starts worker), if not -
   * passes execution to the task worker via setjmp/longjmp. Restores
   * stack at the end.
   */
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

      // TODO: Make alignment configurable (per platform)
      uintptr_t stack_top = reinterpret_cast<uintptr_t>(task->stack.data + task->stack.size);
      stack_top = (stack_top & ~0xFL) - 256;
      platform::set_stack(reinterpret_cast<void*>(stack_top));

      task->state = Task::State::EXEC;

      try {
        task->worker();
      } catch (...) {
#if !ASYNC_CONTINUE_ON_TASK_EXC
        task->state = Task::State::DONE;
        platform::set_stack(stack);
        throw;
#endif
      }

      task->state = Task::State::DONE;

      platform::set_stack(stack);

      // Main tasks, spawned by async::run() are considered "main"
      // If they exit, leaving other tasks unfinished - it is an issue
      if (main_task_id != INVALID_TASK_ID && main_task_id == task->id) {
        checkOrphanedTasks();
      }

      return;
    }

    if (task->state == Task::State::DONE) {
      platform::set_stack(stack);

#if VALGRIND_DEBUG
      VALGRIND_STACK_DEREGISTER(task->stack.id);
#endif

      delete task;

      tasks.erase(tasks.begin() + current);
      if (tasks.size() == 0) running = false;

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

  /** @brief Calls `cycle()` in a loop until all tasks are completed */
  void runUntilCompleted() {
    while (running) {
      cycle();
    }
  }

private:
  /** @brief Atomically get+increment task id */
  TaskId getNextTaskId() {
    const auto id = task_id_counter.load();
    task_id_counter.store(id + 1);
    return id;
  }

  /** @brief Puts task id into to-wake list to be waken at the next cycle */
  void notifyReady(const TaskId id) {
    std::lock_guard lock(sync.mutex);
    sync.to_wake.push_back(id);
  }

  /** @brief Specify main task id */
  void setMainTask(const TaskId id) {
    main_task_id = id;
  }

  void checkOrphanedTasks() {
    for (const auto& l : {tasks, sync.to_start}) {
      for (const auto& t : l) {
        if (t->state != Task::State::DONE) {
          throw MainTaskExitedException();
        }
      }
    }
  }

  /** @brief Processes `to_wake` & `to_start` */
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

  friend void run(const TaskWorker& fn);

  template <typename T, typename... Args>
  friend T run(std::shared_ptr<Future<T>> (task)(Args...), Args&&... args);
};

}
