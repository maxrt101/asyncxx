#pragma once

#include <functional>
#include <format>
#include <print>
#include <string>
#include <async/util.h>

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

#define TEST_LOC_FMT                                                          \
  ANSI_COLOR_FG_CYAN "{}" ANSI_TEXT_RESET ":"                                 \
  ANSI_COLOR_FG_MAGENTA "{}" ANSI_TEXT_RESET

#define TEST_FN_NAME(__name) __test_ ## __name

#define TEST(__suite, __name, __desc)                                         \
  void TEST_FN_NAME(__name) (test::TestSuite * suite);                        \
  static const int __test_dummy_ ## __name = test::impl::registerTest(        \
      &__suite, TEST_FN_NAME(__name), #__name, __desc);                       \
  void TEST_FN_NAME(__name) (test::TestSuite * suite)

#define TEST_ASSERT(__expr, __msg_fmt, ...)                                   \
  do {                                                                        \
    if (!(__expr)) {                                                          \
      throw test::AssertionError(                                             \
        __FILE__, __LINE__,                                                   \
        #__expr,                                                              \
        std::format(__msg_fmt, ## __VA_ARGS__)                                \
      );                                                                      \
    }                                                                         \
  } while (0)

#define TEST_ASSERT_EQ(__lhs, __rhs, __msg_fmt, ...)                          \
  do {                                                                        \
    if ((__lhs) != (__rhs)) {                                                 \
      throw test::AssertionEqualityError(                                     \
        __FILE__, __LINE__,                                                   \
        test::impl::to_string(__lhs),                                                               \
        test::impl::to_string(__rhs),                                                               \
        std::format(__msg_fmt, ## __VA_ARGS__)                                \
      );                                                                      \
    }                                                                         \
  } while (0)

namespace test {

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

struct AssertionEqualityError final : std::runtime_error {
  explicit AssertionEqualityError(
    const std::string& file,
    int                line,
    const std::string& lhs,
    const std::string& rhs,
    const std::string& message
  ) : std::runtime_error(
        std::format(
          "'" ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          "' != '" ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET
          " at " TEST_LOC_FMT ". Message: '"
          ANSI_TEXT_BOLD "{}" ANSI_TEXT_RESET "'",
          lhs, rhs, file, line, message
        )
      ) {}
};

struct TestSuite;

using TestFunction = std::function<void(TestSuite*)>;

struct Test {
  std::string name;
  std::string desc;
  TestFunction fn;
};

struct TestSuite {
  std::string name;

  std::vector<Test> tests;
  size_t current;

  explicit TestSuite(std::string name) : name(name), current(0) {}
};

namespace impl {

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
  std::print("[  " ANSI_COLOR_FG_GREEN "OK" ANSI_TEXT_RESET "  ] {:<{}} - {:<{}}\n", test->name, name_sz, test->desc, desc_sz);
  stat.passed++;
}

inline void reportFailure(Statistics& stat, Test * test, size_t name_sz, size_t desc_sz) {
  std::print("[ " ANSI_COLOR_FG_RED "FAIL" ANSI_TEXT_RESET " ] {:<{}} - {:<{}}\n", test->name, name_sz, test->desc, desc_sz);
  stat.failed++;
}

inline void reportError(Statistics& stat, Test * test, size_t name_sz, size_t desc_sz) {
  std::print("[ " ANSI_COLOR_FG_YELLOW "FAIL" ANSI_TEXT_RESET " ] {:<{}} - {:<{}}\n", test->name, name_sz, test->desc, desc_sz);
  stat.errors++;
}

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
    out += "]";
    return out;
  } else if constexpr (StandardFormattable<T>) {
    return std::format("{}", val);
  } else {
    return "<?>";
  }
}

}

inline int run(TestSuite * suite) {
  std::print("Running {} tests from suite {}:\n", suite->tests.size(), suite->name);

  size_t max_name_size = 0;
  size_t max_desc_size = 0;

  for (auto& test : suite->tests) {
    max_name_size = std::max(max_name_size, test.name.size());
    max_desc_size = std::max(max_desc_size, test.desc.size());
  }

  auto stat = impl::Statistics();

  for (size_t i = 0; i < suite->tests.size(); ++i) {
    suite->current = i;
    auto test = &suite->tests[i];
    try {
      test->fn(suite);
      impl::reportSuccess(stat, test, max_name_size, max_desc_size);
    } catch (AssertionError& e) {
      std::print("[" ANSI_COLOR_BG_RED "ASSERT" ANSI_TEXT_RESET "] {}\n", e.what());
      impl::reportFailure(stat, test, max_name_size, max_desc_size);
    } catch (AssertionEqualityError& e) {
      std::print("[" ANSI_COLOR_BG_RED "ASSERT" ANSI_TEXT_RESET "] {}\n", e.what());
      impl::reportFailure(stat, test, max_name_size, max_desc_size);
    } catch (std::exception& e) {
      std::print("[" ANSI_COLOR_FG_BLACK ANSI_COLOR_BG_YELLOW " EXCN " ANSI_TEXT_RESET "] {}\n", e.what());
      impl::reportError(stat, test, max_name_size, max_desc_size);
    } catch (...) {
      impl::reportError(stat, test, max_name_size, max_desc_size);
    }
  }

  std::print("Passed: " ANSI_COLOR_FG_GREEN  "{}" ANSI_TEXT_RESET "\n", stat.passed);
  std::print("Failed: " ANSI_COLOR_FG_RED    "{}" ANSI_TEXT_RESET "\n", stat.failed);
  std::print("Errors: " ANSI_COLOR_FG_YELLOW "{}" ANSI_TEXT_RESET "\n", stat.errors);

  return stat.failed > 0 ? 1 : 0;
}

}
