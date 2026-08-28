#pragma once

#include <queue>
#include <vector>

#include <async/future.h>
#include <async/task.h>
#include <async/api.h>

namespace async {

/**
 * @brief Queue - Asynchronous message passing primitive
 *
 * Wraps around std::queue, allowing for tasks to block until data is available
 * Data is distributed strictly by FIFO principle - whoever called `.get()`
 * first - will get the data first
 * Consumers call `.get()` which returns an awaitable `Future<T>`, which is
 * also saved into the internal waiter list, along with the task ID. If the
 * queue is not empty, and nobody else is waiting on data - data is popped
 * immediately, without creating a Future
 * Producers call `.put()` which pushes data into the queue and checks if
 * any waiters exist, if so - first waiter in the list is retrieved and
 * it's future is completed with data, popped from queue
 *
 * Example:
 * @code{.c}
 *  auto q = async::Queue<int>::create();
 *
 *  auto consumer = async::task([&] {
 *    auto result = q->get()->await();
 *  });
 *
 *  auto producer = async::task([&] {
 *    q->put(42);
 *  });
 *
 *  async::gather(consumer, producer);
 *
 * @endcode
 *
 * @tparam T Element type
 */
template <typename T>
class Queue {
  struct Waiter {
    // TODO: Is task id needed?
    TaskId          task;
    SharedFuture<T> future;
  };

  std::mutex          mutex;
  std::queue<T>       queue;
  std::vector<Waiter> waiters;

public:
  Queue() = default;

  ~Queue() {
    clear();
  }

  static std::shared_ptr<Queue> create() {
    return std::make_shared<Queue>();
  }

  /**
   * @brief Clears the internal queue and waiter list
   *
   * @todo Waiters must be notified by a CancelledException
   */
  void clear() {
    auto lock = std::unique_lock(mutex);

    queue = std::queue<T>();

    for (auto& waiter : waiters) {
      waiter.future->cancel();
    }

    waiters.clear();
  }

  /** @brief Check if internal queue is empty */
  bool empty() {
    auto lock = std::unique_lock(mutex);

    return queue.empty();
   }

  /**
   * @brief Put data into the queue
   *
   * Data gets pushed onto the internal queue, and first waiter in the list
   * gets it's future completed with data, popped from the queue
   *
   * @param value Data to put
   */
  void put(T&& value) {
    auto lock = std::unique_lock(mutex);

    queue.push(value);

    // TODO: What if, hypothetically, there are N waiters, and N+1 available data
    //       could this result in data being stuck? Or maybe not "stuck", just
    //       requiring more pushes to get "unstuck"
    if (!waiters.empty()) {
      auto waiter = waiters.front();
      waiters.erase(waiters.begin());

      waiter.future->complete(popFront());
    }
  }

  /**
   * @brief Retrieve data from the queue
   *
   * If data is available and nobody is waiting, it's returned wrapped into
   * a already completed future, in other case - the future is created
   * for this data request, this future is saved into an internal waiters
   * list and returned to the user
   *
   * @return Awaitable future, producing next `T` element
   */
  SharedFuture<T> get() {
    auto lock = std::unique_lock(mutex);
    auto f = Future<T>::create();

    if (!queue.empty() && waiters.empty()) {
      f->complete(popFront());
      return f;
    }

    waiters.push_back(Waiter {
      .task = async::self()->getId(),
      .future = f
    });

    return f;
  }

private:
  /** @brief Helper to retrieve an element from std::queue */
  T popFront() {
    auto val = queue.front();
    queue.pop();
    return val;
  }
};

}
