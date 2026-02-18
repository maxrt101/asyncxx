#pragma once

#include <string>
#include <atomic>
#include <cstdio>
#include <cstring>

#include <async/util.h>
#include <async/future.h>
#include <async/api.h>

namespace async::io {

struct FileOpenException final : std::runtime_error {
  explicit FileOpenException(const std::string& filename)
    : std::runtime_error("Failed to open file " + filename + " (" + strerror(errno) + ")") {}
};

struct FileNotOpenedException final : std::runtime_error {
  explicit FileNotOpenedException()
    : std::runtime_error("File has to be opened to perform this action") {}
};

struct FileReadException final : std::runtime_error {
  explicit FileReadException(const std::string& filename)
    : std::runtime_error("Failed to read from " + filename + " (" + strerror(errno) + ")") {}
};

struct FileWriteException final : std::runtime_error {
  explicit FileWriteException(const std::string& filename)
    : std::runtime_error("Failed to write to " + filename + " (" + strerror(errno) + ")") {}
};

struct MalformedFileException final : std::runtime_error {
  explicit MalformedFileException(const std::string& filename)
    : std::runtime_error("Malformed file " + filename + " (" + strerror(errno) + ")") {}
};

class File {
  enum Flag {
    FLAG_NONE   = 0,
    FLAG_OWNER  = (1 << 0),
    FLAG_OPENED = (1 << 1),
    FLAG_PIPE   = (1 << 2),
  };

  std::string          filename;
  FILE *               handle;
  std::atomic<uint8_t> flags;
  std::vector<size_t>  offsets;
public:
  File() : filename(""), handle(nullptr), flags(FLAG_NONE) {}

  File(int fd, const std::string& mode) : filename(std::to_string(fd)), handle(nullptr), flags(FLAG_NONE) {
    handle = fdopen(fd, mode.c_str());
    assertThrow(this->handle, FileOpenException(filename));
    setFlag(FLAG_OWNER, true);
    setFlag(FLAG_OPENED, true);
    setFlag(FLAG_PIPE, true);
  }

  File(const std::string& filename, const std::string& mode) {
    open(filename, mode);
  }

  ~File() {
    close();
  }

  static std::shared_ptr<File> fd(int fd, const std::string& mode) {
    return std::make_shared<File>(fd, mode);
  }

  static std::shared_ptr<File> create(const std::string& filename, const std::string& mode) {
    return std::make_shared<File>(filename, mode);
  }

  void open(const std::string& filename, const std::string& mode) {
    close();
    this->filename = filename;
    this->handle = fopen(filename.c_str(), mode.c_str());
    assertThrow(this->handle, FileOpenException(filename));
    setFlag(FLAG_OWNER, true);
    setFlag(FLAG_OPENED, true);
  }

  void close() {
    if (getFlag(FLAG_OWNER) && handle) {
      fclose(handle);
      handle = nullptr;
    }

    filename = "";

    setFlag(FLAG_OPENED, false);
  }

  void disown() {
    setFlag(FLAG_OWNER, false);
  }

  size_t size() const {
    assertThrow(handle, FileNotOpenedException());

    if (getFlag(FLAG_PIPE)) {
      return 0;
    }

    fseek(handle, 0, SEEK_END);
    size_t s = ftell(handle);
    ::rewind(handle);

    assertThrow(s != -1UL, MalformedFileException(filename));

    return s;
  }

  void rewind() const {
    assertThrow(handle, FileNotOpenedException());

    ::rewind(handle);
  }

  bool end() const {
    assertThrow(handle, FileNotOpenedException());
    return feof(handle);
  }

  size_t tell() const {
    assertThrow(handle, FileNotOpenedException());
    return ftell(handle);
  }

  void seek(const ssize_t ofs) const {
    assertThrow(handle, FileNotOpenedException());
    if (ofs >= 0) {
      fseek(handle, ofs, SEEK_SET);
    } else {
      fseek(handle, ofs, SEEK_END);
    }
  }

  void push() {
    offsets.push_back(tell());
  }

  void pop() {
    seek(offsets.back());
    offsets.pop_back();
  }

  std::shared_ptr<Future<std::string>> read(const size_t n) const {
    assertThrow(handle, FileNotOpenedException());
    return readInner<std::string>(n, true);
  }

  std::shared_ptr<Future<std::string>> readAll() const {
    assertThrow(handle, FileNotOpenedException());
    return getFlag(FLAG_PIPE)
      ? readInnerBuffered<std::string>()
      : readInner<std::string>(size(), false);
  }

  std::shared_ptr<Future<std::string>> readLine() const {
    assertThrow(handle, FileNotOpenedException());

    return async::task<std::string>([this] {
      std::string buf;

      while (true) {
        const int c = fgetc(this->handle);

        if (c == '\n' || c == EOF) {
          break;
        }

        buf.push_back(static_cast<char>(c));
      }

      return buf;
    });
  }

  std::shared_ptr<Future<std::vector<uint8_t>>> readBytes(const size_t n) const {
    assertThrow(handle, FileNotOpenedException());
    return readInner<std::vector<uint8_t>>(n, true);
  }

  std::shared_ptr<Future<std::vector<uint8_t>>> readAllBytes() const {
    assertThrow(handle, FileNotOpenedException());
    return getFlag(FLAG_PIPE)
      ? readInnerBuffered<std::vector<uint8_t>>()
      : readInner<std::vector<uint8_t>>(size(), false);
  }

  std::shared_ptr<Future<>> write(std::string s) const {
    assertThrow(handle, FileNotOpenedException());
    return async::task([this, s] {
      assertThrow(fwrite(s.c_str(), s.size(), 1, this->handle) == 1, FileWriteException(filename));
    });
  }

  std::shared_ptr<Future<>> writeBytes(const uint8_t * buf, size_t n) const {
    assertThrow(handle, FileNotOpenedException());
    return async::task([this, buf, n] {
      assertThrow(fwrite(buf, n, 1, this->handle) == 1, FileWriteException(filename));
    });
  }

private:
  bool getFlag(const uint8_t mask) const {
    uint8_t flags = this->flags.load();
    return (flags & mask) > 0;
  }

  void setFlag(const uint8_t mask, const bool state) {
    uint8_t flags = this->flags.load();
    if (state) {
      flags |= mask;
    } else {
      flags &= ~mask;
    }
    this->flags.store(flags);
  }

  template <typename T>
  std::shared_ptr<Future<T>> readInner(const size_t n, bool check) const {
    return async::task<T>([this, n, check] {
      if (n == 0) return T {};

      T buf;
      buf.resize(n);

      const size_t sz = fread(buf.data(), 1, n, this->handle);

      if (check) assertThrow(sz == n, FileReadException(filename));
      if (sz < n) buf.resize(sz);

      return buf;
    });
  }

  template <typename T>
  std::shared_ptr<Future<T>> readInnerBuffered() const {
    return async::task<T>([this] {
      T result;

      while (true) {
        uint8_t buffer[4096];
        const size_t sz = fread(buffer, 1, sizeof(buffer), this->handle);

        if (sz > 0) {
          if constexpr (std::is_same_v<T, std::string>) {
            result.append((const char*)buffer, sz);
          } else {
            result.insert(result.begin(), buffer, buffer + sz);
          }
        }

        if (sz < sizeof(buffer)) {
          if (feof(this->handle)) break;
          assertThrow(!ferror(this->handle), FileReadException(this->filename));
        }
      }

      return result;
    });
  }
};

}
