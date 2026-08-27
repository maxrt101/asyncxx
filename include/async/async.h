/**
 * @brief Provides main building blocks for async APIs
 */
#pragma once

#include <async/macros.h>
#include <async/task.h>
#include <async/pool.h>
#include <async/loop.h>
#include <async/future.h>
#include <async/api.h>

#include "util.h"

namespace async {

namespace impl {
/**
 * @brief Stores global internal EventLoop, which is referenced by every other
 *        api in this file
 *
 * TODO: Making it thread_local may eliminate locks in EventLoop, but this
 *       will require changes and extensive testing, as currently the system
 *       is designed to share a single EventLoop for the whole process
 */
inline static EventLoop * loop = nullptr;
}

/**
 * @brief Retrieve a pointer to global EventLoop
 *
 * @return Global EventLoop Pointer
 */
inline EventLoop * getGlobalLoop() {
  if (!impl::loop) {
    impl::loop = new EventLoop();
    atexit([] { delete impl::loop; impl::loop = nullptr; });
  }

  return impl::loop;
}

/**
 * @brief Exits from an async task
 *
 * @warning Works only in async context
 */
inline void exit() {
  getGlobalLoop()->getCurrentTask()->state = Task::State::DONE;
  yield();
}

/**
 * @brief Puts current task in WAIT state, effectively blocking it, until
 *        something manually unblocks it via @ref notify
 *
 * @warning Works only in async context
 * @warning Use with caution, this is a basic building block for Futures & Events
 *          but using it in user code may result in a deadlock
 */
inline void wait() {
  getGlobalLoop()->getCurrentTask()->state = Task::State::WAIT;
  yield();
}

/**
 * @brief Wakes up the task. Effectively tells EventLoop that the task with
 *        `id` is ready to run effective next cycle
 *
 * @param id Task ID to wake up
 */
inline void notify(const TaskId id) {
  const auto loop = getGlobalLoop();

  loop->notifyReady(id);
}

/**
 * @brief Manually yield execution from current task. Execution is returned
 *        to EventLoop, which will decide what task to run next
 *
 * @warning Works only in async context
 */
inline void yield() {
  const auto loop = getGlobalLoop();

  auto task = loop->getCurrentTask();

  assertThrow(task, std::runtime_error("No current task"));

  if (setjmp(task->ctx)) {
    return;
  }

  longjmp(loop->ctx, 1);
}

/**
 * @brief Return a TCB pointer to currently executing task
 *
 * @warning Works only in async context
 *
 * @return Pointer to TCB of current task
 */
inline Task * self() {
  const auto loop = getGlobalLoop();

  return loop->getCurrentTask();
}

/**
 * @brief Wait until multiple futures are complete. Mirrored from pythons
 *        asyncio.gather()
 *
 * @warning Works only in async context
 *
 * @tparam Futures Futures to wait for
 * @param  futures Futures to wait for
 */
template <typename... Futures>
void gather(Futures&&... futures) {
  (futures->await(), ...);
}

/**
 * @brief Create an async task from a lambda
 *
 * @tparam F    Lambda type
 * @tparam Args Lambda argument types
 * @tparam T    Return type (inferred from `F` and placed into the result Future)
 * @param  fn   Task worker lambda
 * @param  args Task worker lambda arguments
 * @return Future, that when awaited will call `fn`
 */
template <typename F, typename... Args, typename T>
std::shared_ptr<Future<T>> task(F fn, Args&&... args) {
  auto f = std::make_shared<Future<T>>();
  auto loop = getGlobalLoop();

  loop->addTask([f, fn, ...args = std::forward<Args>(args)] mutable {
    if constexpr (std::is_void_v<T>) {
      fn(std::move(args)...);
      f->complete();
    } else {
      f->complete(fn(std::move(args)...));
    }
  });

  return f;
}

/**
 * @brief Offload a task to a different thread using ThreadPool managed by
 *        global EventLoop. Task is any function, accepting any arguments,
 *        returning any value, that is wrapped in a lambda and pushed into
 *        the thread pool
 *
 * @tparam T    Task return type
 * @tparam F    Task function type
 * @tparam Args Task argument types
 * @param  fn   Task function
 * @param  args Task argument
 * @return Task result
 */
template <typename F, typename... Args, typename T>
std::shared_ptr<Future<T>> to_thread(F fn, Args&&... args) {
  auto f = std::make_shared<Future<T>>();
  auto loop = getGlobalLoop();

  loop->addThreadedTask([f, fn, ...args = std::forward<Args>(args)]() mutable {
    if constexpr (std::is_void_v<T>) {
      fn(std::move(args)...);
      f->complete();
    } else {
      f->complete(fn(std::move(args)...));
    }
  });

  return f;
}

/**
 * @brief Run a function in async context from sync context. Basically used
 *        as an entry point into the async runtime
 *
 * Example:
 * @code{.c}
 *   async::run([] {
 *     auto f = async::io::File("test.txt", "w");
 *
 *     f->write("123")->await();
 *   });
 * @endcode
 *
 * @param fn Async worker function
 */
inline void run(const Task::Worker& fn) {
  const auto loop = getGlobalLoop();
  const auto id = loop->addTask(fn, "<run>");

  loop->setMainTask(id);
  loop->runUntilCompleted();
  loop->clear();
}

/**
 * @brief Run an async function in async context from sync context. Basically
 *        used as an entry point into the async runtime
 *
 * Example:
 * @code{.c}
 *   async_task(write_file) {
 *     auto f = async::io::File("test.txt", "w");
 *
 *     f->write("123")->await();
 *   }
 *
 *   async::run(write_file);
 * @endcode
 *
 * TODO: Use `typename F` to accept any function/lambda?
 *
 * @tparam T    Async function return type
 * @tparam Args Async function argument types
 * @param task Async function
 * @param args Async function argument
 * @return Async function result
 */
template <typename T, typename... Args>
T run(std::shared_ptr<Future<T>> (task)(Args...), Args&&... args) {
  const auto loop = getGlobalLoop();

  std::shared_ptr<Future<T>> f;

  const auto id = loop->addTask([&f, task, ...args = std::forward<Args>(args)] {
    f = task(std::move(args)...);
    f->await();
  }, "<run>");

  loop->setMainTask(id);
  loop->runUntilCompleted();
  loop->clear();

  return f->get();
}

}
