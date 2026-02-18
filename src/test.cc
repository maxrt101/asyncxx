#include <async/async.h>
#include <cstdio>

async_task_r(task42, int) {
  printf("task42 spawned\n");
  async::yield();
  printf("task42 completed\n");
  return 42;
}

void threaded_task() {
  using namespace std::chrono_literals;

  printf("threaded task waiting\n");
  std::this_thread::sleep_for(2s);
  printf("threaded task end\n");
}

int threaded_task_that_returns() {
  using namespace std::chrono_literals;

  printf("threaded task ret waiting\n");
  std::this_thread::sleep_for(1s);
  printf("threaded task ret end\n");

  return 69;
}

int threaded_task_that_returns_and_has_args(const int a, const int b) {
  using namespace std::chrono_literals;

  printf("threaded task ret arg waiting\n");
  std::this_thread::sleep_for(500ms);
  printf("threaded task ret arg end\n");

  return a + b;
}

async_task(task1) {
  printf("task1 start\n");
  async::yield();
  printf("task1 working\n");
  const int a = task42()->await();
  printf("task1 end (%d)\n", a);
}

async_task(task2) {
  printf("task2 start\n");
  async::yield();
  printf("task2 working\n");
  async::task(threaded_task)->await();
  printf("task2 end\n");
}

async_task(task3) {
  printf("task3 start\n");
  async::yield();

  for (int i = 0; i < 10; ++i) {
    printf("task3 working %d\n", i);
    async::yield();
  }

  const int ret = async::task<int>(threaded_task_that_returns)->await();

  printf("task3 end (%d)\n", ret);
}

async_task(task4, int a, int b) {
  printf("task4 start\n");
  async::yield();
  printf("task4 working\n");

  const int ret = async::task<int>(threaded_task_that_returns_and_has_args, a, b)->await();

  printf("task4 end (%d)\n", ret);
}

async_task(task5) {
  printf("task5 start\n");
  async::yield();

  const auto f1 = async::task<int>(threaded_task_that_returns);
  const auto f2 = async::task<int>(threaded_task_that_returns_and_has_args, 350, 1);

  async::gather(f1, f2);

  printf("task5 end (%d)\n", f1->get() + f2->get());
}

// async_main() {
//   printf("async test\n");
//
//   async::gather(
//     task1(),
//     task2(),
//     task3(),
//     task4(10, 20),
//     task5()
//   );
//
//   printf("async test ended\n");
//
//   return 0;
// }
