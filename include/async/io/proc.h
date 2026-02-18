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
  struct Result {
    bool ok = false;

    int exit_code = 0;

    std::shared_ptr<File> out = std::make_shared<File>();
    std::shared_ptr<File> err = std::make_shared<File>();
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

    void close() {
      for (int p : {in[0], in[1], out[0], out[1], err[0], err[1]}) {
        if (p) ::close(p);
      }
    }
  };

  State       state;
  pid_t       pid;
  std::mutex  mutex;
  std::string command;
  std::string input;
  Result      result;

public:
  Process() : state(State::NONE), pid(0) {}

  explicit Process(const std::string& command) : state(State::NONE), pid(0), command(command) {}

  Process(const std::string& command, const std::string& input) : state(State::NONE), pid(0), command(command), input(input), result() {}

  ~Process() {}

  static std::shared_ptr<Process> create(const std::string& command, const std::string& input = "") {
    return std::make_shared<Process>(command, input);
  }

  static std::shared_ptr<Future<Result>> run(const std::string& command, const std::string& input = "") {
    return create(command, input)->start();
  }

  bool isRunning() {
    auto lock = std::unique_lock(mutex);

    return state == State::EXEC;
  }

  std::shared_ptr<Future<Result>> start() {
    auto lock = std::unique_lock(mutex);

    state = State::INIT;

    return async::task<Result>([this] {
      this->execute();
      return this->result;
    });
  }

  void kill(const int sig = SIGKILL) {
    assertThrow(state == State::EXEC, ProcessNotRunningException());

    auto lock = std::unique_lock(mutex);

    ::kill(pid, sig);
  }

  static std::shared_ptr<Future<Result>> chain(const std::shared_ptr<Process>& proc1, const std::shared_ptr<Process>& proc2) {
    const auto r = proc1->start()->await();

    proc2->input = r.out->readAll()->await();

    return proc2->start();
  }

  Result await() {
    return start()->await();
  }

private:
  void execute() {
    const auto pipes = new Pipes {0};

    pipe(pipes->out);
    pipe(pipes->err);

    if (!input.empty()) {
      pipe(pipes->in);
    }

    pid = fork();

    if (pid == 0) {
      dup2(pipes->out[1], STDOUT_FILENO);
      dup2(pipes->err[1], STDERR_FILENO);

      if (!input.empty()) {
        dup2(pipes->in[0], STDIN_FILENO);
      }

      pipes->close();
      const auto argv = createArgv();
      execvp(argv[0], argv);
      ::exit(-1);
    }

    result.ok = true;

    if (pid < 0) {
      auto lock = std::unique_lock(mutex);
      state = State::DONE;
      result.ok = false;
      return;
    }

    close(pipes->out[1]);
    close(pipes->err[1]);

    if (!input.empty()) {
      close(pipes->in[0]);

      write(pipes->in[1], input.c_str(), input.size());
    }

    int status = 0;

    if (waitpid(pid, &status, 0) == -1) {
      result.ok = false;
    }

    if (WIFEXITED(status)) {
      result.exit_code = WEXITSTATUS(status);
    }

    result.out = File::fd(pipes->out[0], "r");
    result.err = File::fd(pipes->err[0], "r");

    delete pipes;
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

