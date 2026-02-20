#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

#include <async/util.h>
#include <async/future.h>
#include <async/api.h>
#include <async/io/file.h>

namespace async::io {

struct ProcessNotRunningException final : std::runtime_error {
  explicit ProcessNotRunningException()
    : std::runtime_error("Process has to be started to perform this action") {}
};

class Process {
public:
  struct IO {
    std::shared_ptr<File> in  = std::make_shared<File>();
    std::shared_ptr<File> out = std::make_shared<File>();
    std::shared_ptr<File> err = std::make_shared<File>();
  };

  using IORef = std::shared_ptr<IO>;

  struct Result {
    bool ok = false;

    int exit_code = 0;

    std::shared_ptr<IO> io = std::make_shared<IO>();
  };

  using ResultRef    = std::shared_ptr<Result>;
  using ResultFuture = std::shared_ptr<Future<ResultRef>>;

  struct Started {
    ResultFuture future;
    IORef        io;
  };

private:
  enum class State {
    NONE,
    INIT,
    EXEC,
    DONE,
  };

  struct Pipes {
    int in[2];
    int out[2];
    int err[2];
    int service[2];

    const std::string& input;

    explicit Pipes(const std::string& input) : input(input) {
      pipe(out);
      pipe(err);
      pipe(in);

      pipe(service);
      fcntl(service[0], F_SETFL, fcntl(service[0], F_GETFL) | O_NONBLOCK);
    }

    ~Pipes() {
      closeFromParent();
    }

    static std::shared_ptr<Pipes> create(const std::string& input) {
      return std::make_shared<Pipes>(input);
    }

    static void close(int& fd) {
      if (fd) {
        ::close(fd);
        fd = 0;
      }
    }

    void prepareChild() {
      dup2(out[1], STDOUT_FILENO);
      dup2(err[1], STDERR_FILENO);
      dup2(in[0], STDIN_FILENO);

      closeFromChild();
    }

    void prepareParent() {
      close(out[1]);
      close(err[1]);
      close(in[0]);

      if (!input.empty()) {
        write(in[1], input.c_str(), input.size());
        close(in[1]);
      }
    }

    bool hadMarker() const {
      uint8_t marker = 0;

      if (read(service[0], &marker, 1) == 1) {
        return marker == '1';
      }

      return false;
    }

    void closeFromChild() {
      // Close all, except for write end of service pipe
      close(in[0]);
      close(in[1]);
      close(out[0]);
      close(out[1]);
      close(err[0]);
      close(err[1]);
      close(service[0]);
    }

    void closeFromParent() {
      // Close all, except for the ones used in IO struct
      // those descriptors will be owned by File, and will
      // get closed once the user discards Result/Process
      close(in[0]);
      close(out[1]);
      close(err[1]);
      close(service[0]);
      close(service[1]);
    }
  };

  using PipesRef = std::shared_ptr<Pipes>;

  State       state;
  pid_t       pid;
  std::mutex  mutex;
  std::string command;
  std::string input;
  ResultRef   result;

public:
  Process()
    : state(State::NONE),
      pid(0),
      result(std::make_shared<Result>()) {}

  explicit Process(const std::string& command)
    : state(State::NONE),
      pid(0),
      command(command),
      result(std::make_shared<Result>()) {}

  Process(const std::string& command, const std::string& input)
    : state(State::NONE),
      pid(0),
      command(command),
      input(input),
      result(std::make_shared<Result>()) {}

  ~Process() {}

  static std::shared_ptr<Process> create(const std::string& command, const std::string& input = "") {
    return std::make_shared<Process>(command, input);
  }

  bool isRunning() {
    auto lock = std::unique_lock(mutex);

    return state == State::EXEC;
  }

  Started start() {
    auto lock = std::unique_lock(mutex);

    state = State::INIT;

    auto pipes = Pipes::create(input);
    this->execute(pipes);

    return {
      async::task<ResultRef>([this, pipes] {
        this->wait(pipes);
        return this->result;
      }),
      result->io
    };
  }

  ResultFuture run() {
    auto lock = std::unique_lock(mutex);

    state = State::INIT;

    return async::task<ResultRef>([this] {
      const auto pipes = Pipes::create(input);

      this->execute(pipes);
      this->wait(pipes);

      return this->result;
    });
  }

  void kill(const int sig = SIGKILL) {
    assertThrow(state == State::EXEC, ProcessNotRunningException());

    auto lock = std::unique_lock(mutex);

    ::kill(pid, sig);
  }

  static ResultFuture chain(
    const std::shared_ptr<Process>& proc1,
    const std::shared_ptr<Process>& proc2
  ) {
    const auto r = proc1->await();
  
    proc2->input = r->io->out->readAll()->await();
  
    return proc2->run();
  }

  ResultRef await() {
    return run()->await();
  }

private:
  void execute(PipesRef pipes) {
    pid = fork();

    if (pid == 0) {
      pipes->prepareChild();

      const auto argv = createArgv();
      execvp(argv[0], argv);

      write(pipes->service[1], "1", 1);

      ::exit(-1);
    }

    result->ok = true;

    if (pid < 0) {
      auto lock = std::unique_lock(mutex);
      state = State::DONE;
      result->ok = false;
      return;
    }

    pipes->prepareParent();

    if (pipes->in[1]) {
      result->io->in = File::fd(pipes->in[1], "w");
    }

    result->io->out = File::fd(pipes->out[0], "r");
    result->io->err = File::fd(pipes->err[0], "r");

    state = State::EXEC;
  }

  void wait(PipesRef pipes) {
    int status = 0;

    int res = waitpid(pid, &status, 0);

    auto lock = std::unique_lock(mutex);

    if (res == -1) {
      result->ok = false;
    }

    result->ok = !pipes->hadMarker();

    if (WIFEXITED(status)) {
      result->exit_code = WEXITSTATUS(status);
    }
  }

  char ** createArgv() const {
    const auto args = str::splitQuoted(command);
    const auto argv = new char*[args.size()+1];

    for (size_t i = 0; i < args.size(); i++) {
      argv[i] = strdup(args[i].c_str());
    }

    argv[args.size()] = nullptr;

    return argv;
  }
};

}

