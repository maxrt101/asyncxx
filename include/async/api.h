/**
 * @brief Provides forward declarations to basic async APIs to avoid circular
 *        includes with async.h
 */
#pragma once

#include <async/defs.h>
#include <memory>

namespace async {

inline EventLoop * getGlobalLoop();

inline void run(const TaskWorker& fn);
inline void exit();
inline void wait();
inline void notify(TaskId id);
inline void yield();
inline Task * self();

template <typename... Futures>
void gather(Futures&&... futures);

template <typename F, typename... Args, typename T = std::invoke_result_t<F, Args...>>
std::shared_ptr<Future<T>> task(F fn, Args&&... args);

template <typename F, typename... Args, typename T = std::invoke_result_t<F, Args...>>
std::shared_ptr<Future<T>> to_thread(F fn, Args&&... args);

template <typename T = void, typename... Args>
T run(std::shared_ptr<Future<T>> (task)(Args...), Args&&... args);

}
