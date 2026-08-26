#pragma once

#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <async/util.h>
#include <async/future.h>
#include <async/api.h>
#include <async/io/file.h>

namespace async::io {

/** @brief Process's supposed to be running, but it's not */
struct ProcessNotRunningException final : std::runtime_error {
  explicit ProcessNotRunningException()
    : std::runtime_error("Process has to be started to perform this action") {}
};

/**
 * @brief Asynchronous Process Class
 *
 * Allows for process manipulation, exposing operations such as:
 *  - start
 *  - terminate
 *  - send signal
 *  - retrieving stdout & stderr
 *  - dynamically pushing into stdin
 *  - chaining output of one process into another
 *
 * Process can run an arbitrary command (passed as a string, or already split list)
 * or a user-provided runner function
 *
 * Wraps around unix system calls: fork, execvp, kill, pipe, dup2, fcntl, read, write, close
 */
class Process {
public:
  using FunctionType = std::function<int()>;

  /**
   * @brief Exposes stdout/stderr/stdin as async File instances wrapped around piped fd
   */
  struct IO {
    std::shared_ptr<File> in  = std::make_shared<File>();
    std::shared_ptr<File> out = std::make_shared<File>();
    std::shared_ptr<File> err = std::make_shared<File>();
  };

  using IORef = std::shared_ptr<IO>;

  /**
   * @brief Process run result
   *
   * `exit_code` - int return code from process itself
   * `ok`        - flag that shows if the desired process had started at all
   */
  struct Result {
    bool ok = false;

    int exit_code = 0;

    IORef io = std::make_shared<IO>();
  };

  using ResultRef    = std::shared_ptr<Result>;
  using ResultFuture = std::shared_ptr<Future<ResultRef>>;

  /**
   * @brief Compound result of Process::start()
   *
   * Needed to return both the future (for user to have somthing to await for)
   * and reference to IO struct, for IO manipulation to read/write stdin/stdout
   * while the process is running
   */
  struct Started {
    ResultFuture future;
    IORef        io;
  };

private:
  enum class State {
    NONE, /// Uninitialized (0)
    INIT, /// Initialization in progress
    EXEC, /// Executing the process
    DONE, /// Process finished (exited or crashed)
  };

  /**
   * @brief Helper struct for managing IPC between parent & child processes
   *
   * Contains duped file descriptors for stdin, stdout, stderr, and a special
   * service fd. Service fd is used to communicate about a failure of `execvp`
   * by writing a special marker for parent process to read
   */
  struct Pipes {
    int in[2]      = {-1, -1};
    int out[2]     = {-1, -1};
    int err[2]     = {-1, -1};
    int service[2] = {-1, -1};

    const std::string& input;

    /**
     * @brief Create pipes for stdin, stdout, stderr & service
     *
     * Service gets special treatment, by being a NONBLOCK pipe, because
     * normally, child process shouldn't write to it at all, so parent
     * shouldn't block on it
     *
     * @param input If the input for stdin is known beforehand, it is saved here
     */
    explicit Pipes(const std::string& input) : input(input) {
      pipe(out);
      pipe(err);
      pipe(in);

      pipe(service);
      fcntl(service[0], F_SETFL, fcntl(service[0], F_GETFL) | O_NONBLOCK);
    }

    /** @brief Close all pipes (if opened) */
    ~Pipes() {
      for (auto& p : {in, out, err, service}) {
        close_fd(p[0]);
        close_fd(p[1]);
      }
    }

    /** @brief Shortcut for make_shared<Pipes> */
    static std::shared_ptr<Pipes> create(const std::string& input) {
      return std::make_shared<Pipes>(input);
    }

    /** @brief Close an fd, if it's not closed already */
    static void close_fd(int& fd) {
      if (fd >= 0) {
        ::close(fd);
        fd = -1;
      }
    }

    /** @brief Called by the child to dup pipes into stdin/stdout/stderr and
     *         close unused ends */
    void prepareChild() {
      dup2(out[1], STDOUT_FILENO);
      dup2(err[1], STDERR_FILENO);
      dup2(in[0], STDIN_FILENO);

      closeFromChild();
    }

    /** @brief Closes unused (by the parent) pipe ends and write input
     *         (if present) into stdin */
    void prepareParent() {
      close_fd(out[1]);
      close_fd(err[1]);
      close_fd(in[0]);

      if (!input.empty()) {
        write(in[1], input.c_str(), input.size());
        close_fd(in[1]);
      }
    }

    /**
     * @brief Returns @c true if a special marker was sent by the child
     *
     * The marker indicates that the execvp failed to execute
     *
     * @warning A one-time operation; reading from service pipe clears
     *          it out, so next calls will allways return @c false
     */
    bool hadMarker() const {
      uint8_t marker = 0;

      if (read(service[0], &marker, 1) == 1) {
        return marker == '1';
      }

      return false;
    }

    /** @brief Closes unused (by the child) ends of pipes */
    void closeFromChild() {
      for (auto& p : {in, out, err}) {
        close_fd(p[0]);
        close_fd(p[1]);
      }

      close_fd(service[0]);
    }
  };

  using PipesRef = std::shared_ptr<Pipes>;

  /**
   * @brief Base class for an Execution Target
   *
   * Execution Target represents what to execute in a spawned process
   */
  struct ExecutionTarget {
    enum Type {
      COMMAND,
      FUNCTION,
    };

    Type type;

    ExecutionTarget(Type type) : type(type) {}
    virtual ~ExecutionTarget() {}

    /**
     * @brief Ran after `fork()` in child process
     */
    virtual void execute(PipesRef pipes) = 0;
  };

  /**
   * @brief Command Execution Target - executes a shell command (runs an executable)
   */
  struct CommandTarget : ExecutionTarget {
    std::vector<std::string> args;

    CommandTarget(const std::vector<std::string>& args)
      : ExecutionTarget(COMMAND), args(args) {}

    /**
     * @brief Creates an argv and passes it to execvp. Has safeguards if execvp fails
     */
    void execute(PipesRef pipes) override {
      // It's ok to not free argv buffer, since execvp will perform
      // typical cleanup for current process before loading new process
      const auto argv = createArgv();
      execvp(argv[0], argv);

      // Will only reach here if execvp fails
      // If reached, will put a special marker into service pipe
      // to let the parent process know that execvp failed
      write(pipes->service[1], "1", 1);

      _exit(-1);
    }

    /** @brief Simple helper to create a typical argv from args */
    char ** createArgv() const {
      const auto argv = new char*[args.size()+1];

      for (size_t i = 0; i < args.size(); i++) {
        argv[i] = strdup(args[i].c_str());
      }

      argv[args.size()] = nullptr;

      return argv;
    }
  };

  /**
   * @brief Function Execution Target - runs a user-provided function
   */
  struct FunctionTarget : ExecutionTarget {
    FunctionType worker;

    FunctionTarget(FunctionType worker)
      : ExecutionTarget(FUNCTION), worker(std::move(worker)) {}

    /**
     * @brief Runs user function, which must return an int, which is then used as a result code
     */
    void execute(PipesRef pipes) override {
      _exit(worker());
    }
  };

  State       state;
  pid_t       pid;
  std::mutex  mutex;
  std::shared_ptr<ExecutionTarget> target; // TODO: unique_ptr?
  std::string input;
  ResultRef   result;

public:
  Process()
    : state(State::NONE),
      pid(0),
      result(std::make_shared<Result>()) {}

  /**
   * @brief Create a process instance with an already split vector of
   *        individual arguments + optional input for stdin
   */
  explicit Process(const std::vector<std::string>& args, const std::string& input = "") : Process()
  {
    this->target = std::make_shared<CommandTarget>(args);
    this->input = input;
  }

  /**
   * @brief Create a process instance with a command string, which will get
   *        split (with quotes taken into account) + optional input for stdin
   */
  explicit Process(const std::string& command, const std::string& input = "")
    : Process(str::splitQuoted(command), input) {}

  /**
   * @brief Create a process instance with a user-provided function, that will
   *        run in child process + optional input for stdin
   */
  Process(FunctionType fn, const std::string& input = "") : Process() {
    this->target = std::make_shared<FunctionTarget>(fn);
    this->input = input;
  }

  ~Process() {}

  static std::shared_ptr<Process> create(const std::vector<std::string>& args, const std::string& input = "") {
    return std::make_shared<Process>(args, input);
  }

  static std::shared_ptr<Process> create(const std::string& command, const std::string& input = "") {
    return std::make_shared<Process>(command, input);
  }

  static std::shared_ptr<Process> create(FunctionType fn, const std::string& input = "") {
    return std::make_shared<Process>(fn, input);
  }

  bool isRunning() {
    auto lock = std::unique_lock(mutex);

    return state == State::EXEC;
  }

  bool isCommand() const {
    return target->type == ExecutionTarget::COMMAND;
  }

  bool isFunction() const {
    return target->type == ExecutionTarget::FUNCTION;
  }

  /**
   * @brief Starts the process, returning the awaitable future and a IORef
   *        Call this when stdin/stdout/stderr must by manipulated while
   *        the process runs
   *
   * @return Awaitable future (.await() will call waitpid) and IORef for
   *         stdin/stdout/stderr manipulation
   */
  Started start() {
    auto lock = std::unique_lock(mutex);

    state = State::INIT;

    auto pipes = Pipes::create(input);
    this->execute(pipes);

    return {
      .future = async::to_thread([this, pipes] {
        this->wait(pipes);
        return this->result;
      }),
      .io = result->io
    };
  }

  /**
   * @brief Starts the process, returning an awaitable future
   *        Call this, when stdin/stdout/stderr isn't needed to be
   *        manipulated while the process runs (you're only interested
   *        in the output after it finishes)
   *
   * @return Awaitable future (.await() will call waitpid)
   */
  ResultFuture run() {
    auto lock = std::unique_lock(mutex);

    state = State::INIT;

    return async::to_thread([this] {
      const auto pipes = Pipes::create(input);

      this->execute(pipes);
      this->wait(pipes);

      return this->result;
    });
  }

  /**
   * @brief Send a signal to this process
   *
   * @param sig Any POSIX signal (SIGKILL by default)
   */
  void kill(const int sig = SIGKILL) {
    assertThrow(state == State::EXEC, ProcessNotRunningException());

    auto lock = std::unique_lock(mutex);

    ::kill(pid, sig);
  }

  /**
   * @brief Chain output from `proc1` as an input for `proc1`
   *
   * @param proc1 Process, output of which will be passed to `proc2`
   * @param proc2 Process, which receives output of `proc1`
   * @return Future that will return result for `proc2` of success and for `proc1` on it's failure
   */
  static ResultFuture chain(
    const std::shared_ptr<Process>& proc1,
    const std::shared_ptr<Process>& proc2
  ) {
    return async::task([&] {
      auto r = proc1->await();

      if (!r->ok || !r->exit_code) {
        return r;
      }

      proc2->input = r->io->out->readAll()->await();

      return proc2->await();
    });
  }

  /** @brief Shortcut for run().await() */
  ResultRef await() {
    return run()->await();
  }

private:
  /**
   * @brief Actual runner/executor of new process
   *
   * Will fork current process, running desired command in the child
   * and preparing IO and closing pipes in the parent
   *
   * @param pipes Valid/initialized Pipes reference
   */
  void execute(PipesRef pipes) {
    pid = fork();

    if (pid == 0) {
      pipes->prepareChild();

      // Run target-specific executor
      target->execute(pipes);

      // Last resort :)
      _exit(-1);
    }

    result->ok = true;

    if (pid < 0) {
      auto lock = std::unique_lock(mutex);
      state = State::DONE;
      result->ok = false;
      return;
    }

    pipes->prepareParent();

    if (pipes->in[1] >= 0) {
      result->io->in = File::fd(pipes->in[1], "w");
      pipes->in[1] = -1;
    }

    result->io->out = File::fd(pipes->out[0], "r");
    pipes->out[0] = -1;

    result->io->err = File::fd(pipes->err[0], "r");
    pipes->err[0] = -1;

    state = State::EXEC;
  }

  /**
   * @brief Used by the process result future, to wait for the process to finish
   *
   * @param pipes Valid/Initialized pipes reference
   */
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
};

}

