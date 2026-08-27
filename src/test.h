#pragma once

#include <functional>
#include <format>
#include <format>
#include <cstdint>
#include <string>
#include <async/util.h>

/**
 * @brief ANSI Escape codes + BG/FG colors
 * @defgroup ansi ANSI escape codes
 * @{
 */

#define ANSI_TEXT_RESET       "\x1b[0m"
#define ANSI_TEXT_BOLD        "\x1b[1m"

#define ANSI_COLOR_FG_BLACK   "\x1b[30m"
#define ANSI_COLOR_FG_RED     "\x1b[31m"
#define ANSI_COLOR_FG_GREEN   "\x1b[32m"
#define ANSI_COLOR_FG_YELLOW  "\x1b[33m"
#define ANSI_COLOR_FG_BLUE    "\x1b[34m"
#define ANSI_COLOR_FG_MAGENTA "\x1b[35m"
#define ANSI_COLOR_FG_CYAN    "\x1b[36m"
#define ANSI_COLOR_FG_WHITE   "\x1b[37m"
#define ANSI_COLOR_FG_DEFAULT "\x1b[39m"

#define ANSI_COLOR_BG_BLACK   "\x1b[40m"
#define ANSI_COLOR_BG_RED     "\x1b[41m"
#define ANSI_COLOR_BG_GREEN   "\x1b[42m"
#define ANSI_COLOR_BG_YELLOW  "\x1b[43m"
#define ANSI_COLOR_BG_BLUE    "\x1b[44m"
#define ANSI_COLOR_BG_MAGENTA "\x1b[45m"
#define ANSI_COLOR_BG_CYAN    "\x1b[46m"
#define ANSI_COLOR_BG_WHITE   "\x1b[47m"
#define ANSI_COLOR_BG_DEFAULT "\x1b[49m"

/** @} */

/**
 * @brief std::format-compatible (colored) format string for FILE:LINE
 *
 * @warning For internal use
 */
#define TEST_LOC_FMT                                                           \
  ANSI_COLOR_FG_CYAN "{}" ANSI_TEXT_RESET ":"                                  \
  ANSI_COLOR_FG_MAGENTA "{}" ANSI_TEXT_RESET

/**
 * @brief Shortcut to get test function from test name
 */
#define TEST_FN_NAME(__name) __test_ ## __name

/**
 * @brief Declare a test
 *
 * Example:
 * @code{.c}
 *   auto test_suite = test::TestSuite("test suite");
 *
 *   TEST(test_suite, test_name, "Some test that tests something") {
 *      TEST_ASSERT(true, "Should never fail");
 *   }
 * @endcode
 *
 * @param __suite Test Suite name
 * @param __name  Test name
 * @param __desc  Test description (string literal)
 */
#define TEST(__suite, __name, __desc)                                          \
  void TEST_FN_NAME(__name) (test::TestSuite * suite);                         \
  static const int __test_dummy_ ## __name = test::impl::registerTest(         \
      &__suite, TEST_FN_NAME(__name), #__name, __desc);                        \
  void TEST_FN_NAME(__name) (test::TestSuite * suite)

/**
 * @brief Expands to an equality assertion error common arguments
 *
 * @warning For internal use
 *
 * @param __lhs     LHS of a comparison
 * @param __rhs     RHS of a comparison
 * @param __msg_fmt std::format-compatible string for assertion failure message
 * @param ...       Assertion failure message args
 */
#define __ASSERTION_EQ_ERROR_COMMON_ARGS(__lhs, __rhs, __msg_fmt, ...)         \
    __FILE__, __LINE__,                                                        \
    test::impl::to_string(__lhs),                                              \
    test::impl::to_string(__rhs),                                              \
    std::format(__msg_fmt, ## __VA_ARGS__)

/**
 * @brief Constructs & throws AssertionEqualityError
 *
 * @warning For internal use
 *
 * @param __op @c true - equals; @c false - not-equals
 * For the rest of the arguments see @ref __ASSERTION_EQ_ERROR_COMMON_ARGS
 */
#define __THROW_ASSERTION_EQ_ERROR(__op, __lhs, __rhs, __msg_fmt, ...)         \
  throw test::AssertionEqualityError(                                          \
    __op,                                                                      \
    __ASSERTION_EQ_ERROR_COMMON_ARGS(__lhs, __rhs, __msg_fmt, ## __VA_ARGS__)  \
  )

/**
 * @brief Constructs & throws AssertionOneOfError
 *
 * @warning For internal use
 *
 * For the arguments see @ref __ASSERTION_EQ_ERROR_COMMON_ARGS
 */
#define __THROW_ASSERTION_ONE_OF_ERROR(__lhs, __rhs, __msg_fmt, ...)           \
  throw test::AssertionOneOfError(                                             \
    __ASSERTION_EQ_ERROR_COMMON_ARGS(__lhs, __rhs, __msg_fmt, ## __VA_ARGS__)  \
  )

/**
 * @brief Perform an assertion check (is __expr true?) and throw an
 *        AssertionError with formatted __msg_fmt if it fails
 *
 * @param __expr    Expression to check
 * @param __msg_fmt std::format-compatible string for assertion failure message
 * @param ...       Assertion failure message args
 */
#define TEST_ASSERT(__expr, __msg_fmt, ...)                                    \
  do {                                                                         \
    if (!(__expr)) {                                                           \
      throw test::AssertionError(                                              \
        __FILE__, __LINE__,                                                    \
        #__expr,                                                               \
        std::format(__msg_fmt, ## __VA_ARGS__)                                 \
      );                                                                       \
    }                                                                          \
  } while (0)

/**
 * @brief Perform an equality check (does __lhs == __rhs?) and throw an
 *        AssertionEqualityError with formatted __msg_fmt if it fails
 *
 * @param __lhs     Left Hand Side of the equality check
 * @param __rhs     Right Hand Side of the equality check
 * @param __msg_fmt std::format-compatible string for assertion failure message
 * @param ...       Assertion failure message args
 */
#define TEST_ASSERT_EQ(__lhs, __rhs, __msg_fmt, ...)                           \
  do {                                                                         \
    if ((__lhs) != (__rhs)) {                                                  \
      __THROW_ASSERTION_EQ_ERROR(                                              \
          true, __lhs, __rhs,                                                  \
          __msg_fmt, ## __VA_ARGS__                                            \
      );                                                                       \
    }                                                                          \
  } while (0)

/**
 * @brief Perform a non-equality check (does __lhs != __rhs?) and throw an
 *        AssertionEqualityError with formatted __msg_fmt if it fails
 *
 * @param __lhs     Left Hand Side of the non-equality check
 * @param __rhs     Right Hand Side of the non-equality check
 * @param __msg_fmt std::format-compatible string for assertion failure message
 * @param ...       Assertion failure message args
 */
#define TEST_ASSERT_NE(__lhs, __rhs, __msg_fmt, ...)                           \
  do {                                                                         \
    if ((__lhs) == (__rhs)) {                                                  \
      __THROW_ASSERTION_EQ_ERROR(                                              \
          false, __lhs, __rhs,                                                 \
          __msg_fmt, ## __VA_ARGS__                                            \
      );                                                                       \
    }                                                                          \
  } while (0)

/**
 * @brief Test if __lhs is in one of __rhs values (__rhs contains __lhs?) and
 *        throw an AssertionOneOfError with formatted __msg_fmt if it fails
 *
 * @param __lhs     LHS, Element to check
 * @param __rhs     RHS, List of elements, where __lhs must be in
 * @param __msg_fmt std::format-compatible string for assertion failure message
 * @param ...       Assertion failure message args
 */
#define TEST_ASSERT_ONE_OF(__lhs, __rhs, __msg_fmt, ...)                       \
  do {                                                                         \
    bool found = false;                                                        \
    for (const auto& element : (__rhs)) {                                      \
      if (element == (__lhs)) {                                                \
        found = true;                                                          \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    if (!found) {                                                              \
      __THROW_ASSERTION_ONE_OF_ERROR(__lhs, __rhs, __msg_fmt, ## __VA_ARGS__); \
    }                                                                          \
  } while (0)

namespace test {

/**
 * @brief Basic Assertion Error
 */
struct AssertionError final : std::runtime_error {
  explicit AssertionError(
    const std::string& file,
    int                line,
    const std::string& expr,
    const std::string& message
  ) : std::runtime_error(
        std::format(
          "'" ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          "' failed at " TEST_LOC_FMT " with message '"
          ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET "'",
          expr, file, line, message
        )
      ) {}
};

/**
 * @brief Equality Assertion Error
 */
struct AssertionEqualityError final : std::runtime_error {
  explicit AssertionEqualityError(
    bool               eq,
    const std::string& file,
    int                line,
    const std::string& lhs,
    const std::string& rhs,
    const std::string& message
  ) : std::runtime_error(
        std::format(
          "'" ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          "' {} '"
          ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          " at " TEST_LOC_FMT ". Message: '"
          ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET "'",
          lhs, eq ? "!=" : "==", rhs, file, line, message
        )
      ) {}
};

/**
 * @brief "Contains" or "One Of" Assertion Error
 */
struct AssertionOneOfError final : std::runtime_error {
  explicit AssertionOneOfError(
    const std::string& file,
    int                line,
    const std::string& lhs,
    const std::string& rhs,
    const std::string& message
  ) : std::runtime_error(
        std::format(
          "'" ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          "' was expected to be one of '"
          ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          " at " TEST_LOC_FMT ". Message: '"
          ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET "'",
          lhs, rhs, file, line, message
        )
      ) {}
};

struct TestSuite;

using TestFunction = std::function<void(TestSuite*)>;

/**
 * @brief Represents a single test
 */
struct Test {
  std::string name;
  std::string desc;
  TestFunction fn;
};

/**
 * @brief Represents a test suite of singular tests
 */
struct TestSuite {
  std::string name;

  std::vector<Test> tests;
  size_t current;

  explicit TestSuite(std::string name) : name(name), current(0) {}
};

namespace impl {

template<typename T>
concept StandardFormattable = requires(T v) {
  std::format("{}", v);
};

template<typename T>
concept IsIterable = requires(T t) {
  std::begin(t);
  std::end(t);
} && !std::is_convertible_v<T, std::string>;

template <typename T>
std::string to_string(const T& val) {
  if constexpr (std::is_convertible_v<T, std::string>) {
    return async::str::escape(static_cast<std::string>(val));
  } else if constexpr (std::is_integral_v<decltype(val)> && sizeof(val) == 1) {
    return std::format("0x{:02x}", (uint8_t) val);
  } else if constexpr (IsIterable<T>) {
    std::string out = "[";
    bool first = true;
    for (const auto& item : val) {
      if (!first) out += ", ";
      out += to_string(item);
      first = false;
    }
    return out + ']';
  } else if constexpr (StandardFormattable<T>) {
    return std::format("{}", val);
  } else {
    return "<?>";
  }
}

template <typename... Args>
void print(const std::format_string<Args...> fmt, Args&&... args) {
  auto string = std::format(fmt, std::forward<Args>(args)...);
  fputs(string.c_str(), stdout);
}

struct Statistics {
  size_t passed = 0;
  size_t failed = 0;
  size_t errors = 0;
};

inline int registerTest(TestSuite * suite, TestFunction fn, std::string name, std::string desc) {
  suite->tests.push_back({name, desc, fn});
  return 0;
}

inline void reportSuccess(Statistics& stat, Test * test, size_t name_sz, size_t desc_sz) {
  impl::print("[  " ANSI_COLOR_FG_GREEN "OK" ANSI_TEXT_RESET "  ] {:<{}} - {:<{}}\n", test->name, name_sz, test->desc, desc_sz);
  stat.passed++;
}

inline void reportFailure(Statistics& stat, Test * test, size_t name_sz, size_t desc_sz) {
  impl::print("[ " ANSI_COLOR_FG_RED "FAIL" ANSI_TEXT_RESET " ] {:<{}} - {:<{}}\n", test->name, name_sz, test->desc, desc_sz);
  stat.failed++;
}

inline void reportError(Statistics& stat, Test * test, size_t name_sz, size_t desc_sz) {
  impl::print("[ " ANSI_COLOR_FG_YELLOW "FAIL" ANSI_TEXT_RESET " ] {:<{}} - {:<{}}\n", test->name, name_sz, test->desc, desc_sz);
  stat.errors++;
}

}

/**
 * @brief Run a select tests (from `to_run`) or all of them (`to_run={}`) from
 *        a specific test suite
 *
 * @param suite  Test Suite to run tests from
 * @param to_run List of tests to run (empty for all)
 * @return Return code. All additional info will be printed into stdout
 */
inline int run(TestSuite * suite, std::vector<std::string> to_run = {}) {
  auto has_to_run = [&to_run](const std::string& name) {
    return std::find(to_run.begin(), to_run.end(), name) != to_run.end();
  };

  if (to_run.empty()) {
    std::transform(
      suite->tests.begin(),
      suite->tests.end(),
      std::back_inserter(to_run),
      [](auto& v) { return v.name; }
    );
  }

  impl::print("Running {}/{} tests from suite {}:\n", to_run.size(), suite->tests.size(), suite->name);

  size_t max_name_size = 0;
  size_t max_desc_size = 0;

  for (auto& test : suite->tests) {
    if (!has_to_run(test.name)) continue;
    max_name_size = std::max(max_name_size, test.name.size());
    max_desc_size = std::max(max_desc_size, test.desc.size());
  }

  auto stat = impl::Statistics();

  for (size_t i = 0; i < suite->tests.size(); ++i) {
    auto test = &suite->tests[i];
    if (!has_to_run(test->name)) continue;
    suite->current = i;
    try {
      test->fn(suite);
      impl::reportSuccess(stat, test, max_name_size, max_desc_size);
    } catch (AssertionError& e) {
      impl::print("[" ANSI_COLOR_BG_RED "ASSERT" ANSI_TEXT_RESET "] {}\n", e.what());
      impl::reportFailure(stat, test, max_name_size, max_desc_size);
    } catch (AssertionEqualityError& e) {
      impl::print("[" ANSI_COLOR_BG_RED "ASSERT" ANSI_TEXT_RESET "] {}\n", e.what());
      impl::reportFailure(stat, test, max_name_size, max_desc_size);
    } catch (std::exception& e) {
      impl::print("[" ANSI_COLOR_FG_BLACK ANSI_COLOR_BG_YELLOW " EXCN " ANSI_TEXT_RESET "] {}\n", e.what());
      impl::reportError(stat, test, max_name_size, max_desc_size);
    } catch (...) {
      impl::reportError(stat, test, max_name_size, max_desc_size);
    }
  }

  impl::print("Passed: " ANSI_COLOR_FG_GREEN  "{}" ANSI_TEXT_RESET "\n", stat.passed);
  impl::print("Failed: " ANSI_COLOR_FG_RED    "{}" ANSI_TEXT_RESET "\n", stat.failed);
  impl::print("Errors: " ANSI_COLOR_FG_YELLOW "{}" ANSI_TEXT_RESET "\n", stat.errors);

  return stat.failed > 0 ? 1 : 0;
}

}
