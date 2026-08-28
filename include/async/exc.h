#pragma once

#include <exception>
#include <stdexcept>
#include <string>

namespace async {

class CancelledException : public std::runtime_error {
public:
  explicit CancelledException(const std::string& msg = "")
    : std::runtime_error("The operation was cancelled" + (msg.empty() ? "" : ": " + msg)) {}
};

}
