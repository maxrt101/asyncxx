#pragma once

#include <string>
#include <vector>
#include <format>

namespace async {

/**
 * Helper function to throw a specific exception if expr is false
 *
 * @tparam E Exception type. Usually inferred by the compiler from `ex`
 * @param expr true - OK, false - will throw
 * @param ex Exception to throw if check fails
 */
template <typename E>
void assertThrow(bool expr, const E& ex) {
  if (!expr) {
    throw ex;
  }
}

/**
 * Helper function to throw a specific exception if expr is NULL
 *
 * @tparam T Type of expr. Usually inferred by the compiler from `expr`
 * @tparam E Exception type. Usually inferred by the compiler from `ex`
 * @param expr Value to check for being NULL
 * @param ex Exception to throw if check fails
 * @return expr, if it's not NULL
 */
template <typename T, typename E>
T throwIfNull(T expr, const E& ex) {
  if (!expr) {
    throw ex;
  }

  return expr;
}

/* Checks of value is in any of args */
template <typename T, typename... Args>
bool oneOf(T&& value, Args&&... args) {
  return ((value == args) || ...);
}

namespace str {

inline std::vector<std::string> split(const std::string& str, const std::string& delimiter = " ") {
  std::vector<std::string> result;
  size_t last = 0, next = 0;

  while ((next = str.find(delimiter, last)) != std::string::npos) {
    result.push_back(str.substr(last, next-last));
    last = next + 1;
  }

  result.push_back(str.substr(last));
  return result;
}

inline std::vector<std::string> splitQuoted(const std::string& str, const char delimiter = ' ') {
  std::vector<std::string> result;
  std::string current;
  bool inQuotes = false;
  char quoteChar = 0;

  for (size_t i = 0; i < str.size(); ++i) {
    const char c = str[i];

    if (!inQuotes) {
      if (c == '\'' || c == '"' || c == '`') {
        inQuotes = true;
        quoteChar = c;
      } else if (c == delimiter) {
        if (!current.empty()) {
          result.push_back(current);
          current.clear();
        }
      } else {
        current += c;
      }
    } else {
      if (c == quoteChar) {
        inQuotes = false;
        quoteChar = 0;
      } else {
        current += c;
      }
    }
  }

  if (!current.empty()) {
    result.push_back(current);
  }

  return result;
}


inline std::string escape(const std::string& str) {
  std::string out;
  out.reserve(str.size());
  for (unsigned char c : str) {
    switch (c) {
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\\': out += "\\\\"; break;
      case '\"': out += "\\\""; break;
      default:
        if (c < 32 || c > 126) {
          // For non-printable chars, use hex format
          out += std::format("\\x{:02x}", (int)c);
        } else {
          out += c;
        }
    }
  }
  return out;
}


}

}
