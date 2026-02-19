#pragma once


#define async_task_decl(__fn) \
  std::shared_ptr<async::Future<void>> __fn(auto... args);


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



#define async_task_r_decl(__fn, __ret) \
  std::shared_ptr<async::Future<__ret>> __fn(auto... args);


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



#define async_main()                                              \
  int __main();                                                   \
  int main() {                                                    \
    auto f = std::make_shared<async::Future<int>>();              \
    async::run([f]() { f->complete(__main()); });                 \
    return f->get();                                              \
  }                                                               \
  int __main()


#define async_main_ext()                                          \
  int __main(int argc, char ** argv, char ** envp);               \
  int main(int argc, char ** argv, char ** envp) {                \
    auto f = std::make_shared<async::Future<int>>();              \
    async::run([f]() { f->complete(__main(argc, argv, envp)); }); \
    return f->get();                                              \
  }                                                               \
  int __main(int argc, char ** argv, char ** envp)

