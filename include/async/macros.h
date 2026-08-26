/**
 * @brief Contains macros for easily creating async tasks
 */
#pragma once

/**
 * @brief Expands to a forward declaration of a task
 *
 * @param __fn Async function name
 */
#define async_task_decl(__fn) \
  std::shared_ptr<async::Future<void>> __fn(auto... args);

/**
 * @brief Creates an async task wrapper, which calls user-defined
 *        worker function, wrapping it's result into a future and
 *        starting the worker as an EventLoop task
 *
 * @code{.c}
 *   async_task(write_file, std::string contents) {
 *     auto f = async::io::File("test.txt", "w");
 *
 *     f->write(contents)->await();
 *   }
 * @endcode
 *
 * @param __fn Function name
 * @param ...  Argument declarations
 */
#define async_task(__fn, ...)                                     \
  void __fn ## _worker (__VA_ARGS__);                             \
  std::shared_ptr<async::Future<void>> __fn(auto... args) {       \
    auto f = std::make_shared<async::Future<void>>();             \
    auto loop = async::getGlobalLoop();                           \
    loop->addTask([f, args...]() {                                \
      __fn ## _worker(args...); f->complete();                    \
    });                                                           \
    return f;                                                     \
  }                                                               \
  void __fn ## _worker (__VA_ARGS__)

/**
 * @brief Same as @ref async_task, but pins a provided name to this task
 *
 * @code{.c}
 *   async_task_n(write_file, "File Writer") {
 *     auto f = async::io::File("test.txt", "w");
 *
 *     f->write("123")->await();
 *   }
 * @endcode
 *
 * @param __fn   Function name
 * @param __name Task name
 * @param ...    Argument declarations
 */
#define async_task_n(__fn, __name, ...)                           \
  void __fn ## _worker (__VA_ARGS__);                             \
  std::shared_ptr<async::Future<void>> __fn(auto... args) {       \
    auto f = std::make_shared<async::Future<void>>();             \
    auto loop = async::getGlobalLoop();                           \
    loop->addTask([f, args...]() {                                \
      __fn ## _worker(args...); f->complete();                    \
    }, __name);                                                   \
    return f;                                                     \
  }                                                               \
  void __fn ## _worker (__VA_ARGS__)

/**
 * @brief Same as @ref async_task_decl, but adds a custom return tyoe
 *
 * @param __fn  Function name
 * @param __ret Return type
 */
#define async_task_r_decl(__fn, __ret) \
  std::shared_ptr<async::Future<__ret>> __fn(auto... args);

/**
 * @brief Same as @ref async_task, but adds a custom return type
 *
 * @code{.c}
 *   async_task_r(read_file_int, int) {
 *     auto f = async::io::File("test.txt", "r");
 *
 *     auto line = f->readLine()->await();
 *     return std::stoi(line);
 *   }
 * @endcode
 *
 * @param __fn  Function name
 * @param __ret Return type
 * @param ...   Argument declarations
 */
#define async_task_r(__fn, __ret, ...)                            \
  __ret __fn ## _worker (__VA_ARGS__);                            \
  std::shared_ptr<async::Future<__ret>> __fn(auto... args) {      \
    auto f = std::make_shared<async::Future<__ret>>();            \
    auto loop = async::getGlobalLoop();                           \
    loop->addTask([f, args...]() {                                \
      f->complete(__fn ## _worker(args...));                      \
    });                                                           \
    return f;                                                     \
  }                                                               \
  __ret __fn ## _worker (__VA_ARGS__)

/**
 * @brief Combines @ref async_task_r, and @ref async_task_n
 *
 * @code{.c}
 *   async_task_n_r(read_file_int, "File Reader", int) {
 *     auto f = async::io::File("test.txt", "r");
 *
 *     auto line = f->readLine()->await();
 *     return std::stoi(line);
 *   }
 * @endcode
 *
 * @param __fn  Function name
 * @param __name Task name
 * @param __ret Return type
 * @param ...   Argument declarations
 */
#define async_task_n_r(__fn, __name, __ret, ...)                  \
  __ret __fn ## _worker (__VA_ARGS__);                            \
  std::shared_ptr<async::Future<__ret>> __fn(auto... args) {      \
    auto f = std::make_shared<async::Future<__ret>>();            \
    auto loop = async::getGlobalLoop();                           \
    loop->addTask([f, args...]() {                                \
      f->complete(__fn ## _worker(args...));                      \
    }, __name);                                                   \
    return f;                                                     \
  }                                                               \
  __ret __fn ## _worker (__VA_ARGS__)

/**
 * @brief Shorthand for creating a main() function and calling
 *        async::run() on a provided function
 *
 * @code{.c}
 *  async_main() {
 *    auto f = async::io::File("test.txt", "w");
 *
 *     f->write("123")->await();
 *  }
 * @endcode
 *
 * Same as:
 *
 * @code{.c}
 *   async_task_r(write_file, int) {
 *     auto f = async::io::File("test.txt", "w");
 *
 *     f->write("123")->await();
 *
 *     return 0;
 *   }
 *
 *   int main() {
 *      return async::run(write_file);
 *   }
 * @endcode
 */
#define async_main()                                              \
  int __main();                                                   \
  int main() {                                                    \
    auto f = std::make_shared<async::Future<int>>();              \
    async::run([f]() { f->complete(__main()); });                 \
    return f->get();                                              \
  }                                                               \
  int __main()

/**
 * @brief Same as @ref async_main but includes argc, argv & envp
 *
 * @code{.c}
 *  async_main_ext() {
 *    auto f = async::io::File("test.txt", "w");
 *
 *    f->write(argv[0])->await();
 *
 *    return 0;
 *  }
 * @endcode
 *
 * Same as:
 *
 * @code{.c}
 *   async_task_r(write_file, int, int argc, char ** argv, char ** envp) {
 *     auto f = async::io::File("test.txt", "w");
 *
 *     f->write("123")->await();
 *
 *     return 0
 *   }
 *
 *   int main(int argc, char ** argv, char ** envp) {
 *      return async::run(write_file, argc, argv, envp);
 *   }
 * @endcode
 */
#define async_main_ext()                                          \
  int __main(int argc, char ** argv, char ** envp);               \
  int main(int argc, char ** argv, char ** envp) {                \
    auto f = std::make_shared<async::Future<int>>();              \
    async::run([f]() { f->complete(__main(argc, argv, envp)); }); \
    return f->get();                                              \
  }                                                               \
  int __main(int argc, char ** argv, char ** envp)

