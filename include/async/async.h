#pragma once

#include <async/macros.h>
#include <async/task.h>
#include <async/pool.h>
#include <async/loop.h>
#include <async/future.h>
#include <async/event.h>
#include <async/api.h>

#include "util.h"

namespace async {

namespace impl {
inline static EventLoop * loop = nullptr;
}

inline EventLoop * getGlobalLoop() {
  if (!impl::loop) {
    impl::loop = new EventLoop();
    atexit([] { delete impl::loop; impl::loop = nullptr; });
  }

  return impl::loop;
}


inline void exit() {
  getGlobalLoop()->getCurrentTask()->state = Task::State::DONE;
  yield();
}


inline void wait() {
  getGlobalLoop()->getCurrentTask()->state = Task::State::WAIT;
  yield();
}


inline void notify(const TaskId id) {
  const auto loop = getGlobalLoop();

  loop->notifyReady(id);
}


inline void yield() {
  const auto loop = getGlobalLoop();

  auto task = loop->getCurrentTask();

  assertThrow(task, std::runtime_error("No current task"));

  if (setjmp(task->ctx)) {
    return;
  }

  longjmp(loop->ctx, 1);
}


inline Task * self() {
  const auto loop = getGlobalLoop();

  return loop->getCurrentTask();
}


template <typename... Futures>
void gather(Futures&&... futures) {
  (futures->await(), ...);
}


template <typename T, typename F, typename... Args>
std::shared_ptr<Future<T>> task(F fn, Args&&... args) {
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


inline void run(const Task::Worker& fn) {
  const auto loop = getGlobalLoop();

  loop->addTask(fn, "<run>");
  loop->runUntilCompleted();
  loop->clear();
}


template <typename T, typename... Args>
T run(std::shared_ptr<Future<T>> (task)(Args...), Args&&... args) {
  const auto loop = getGlobalLoop();

  std::shared_ptr<Future<T>> f;

  loop->addTask([&f, task, ...args = std::forward<Args>(args)] {
    f = task(std::move(args)...);
    f->await();
  }, "<run>");

  loop->runUntilCompleted();
  loop->clear();

  return f->get();
}

}
