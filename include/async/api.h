#pragma once

#include <async/task.h>
#include <async/loop.h>

namespace async {

template <typename T>
class Future;

inline EventLoop * getGlobalLoop();

inline void run(const Task::Worker& fn);
inline void exit();
inline void wait();
inline void notify(TaskId id);
inline void yield();
inline Task * self();

template <typename... Futures>
void gather(Futures&&... futures);

template <typename T = void, typename F, typename... Args>
std::shared_ptr<Future<T>> task(F fn, Args&&... args);

template <typename T = void, typename... Args>
T run(std::shared_ptr<Future<T>> (task)(Args...), Args&&... args);

}
