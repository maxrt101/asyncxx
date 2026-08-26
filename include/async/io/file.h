#pragma once

#include <string>
#include <atomic>
#include <cstdio>
#include <cstring>

#include <async/util.h>
#include <async/future.h>
#include <async/api.h>

/**
 * @brief Redefinable size of a buffer, for reading data from pipes in chunks
 */
#ifndef ASYNC_IO_FILE_BUFFERED_READ_SIZE
#define ASYNC_IO_FILE_BUFFERED_READ_SIZE 4096
#endif

namespace async::io {

/** @brief Failure to open file */
struct FileOpenException final : std::runtime_error {
  explicit FileOpenException(const std::string& filename)
    : std::runtime_error("Failed to open file " + filename + " (" + strerror(errno) + ")") {}
};

/** @brief File's supposed to be opened, but it's not */
struct FileNotOpenedException final : std::runtime_error {
  explicit FileNotOpenedException()
    : std::runtime_error("File has to be opened to perform this action") {}
};

/** @brief Failure to read file */
struct FileReadException final : std::runtime_error {
  explicit FileReadException(const std::string& filename)
    : std::runtime_error("Failed to read from " + filename + " (" + strerror(errno) + ")") {}
};

/** @brief Failure to write file */
struct FileWriteException final : std::runtime_error {
  explicit FileWriteException(const std::string& filename)
    : std::runtime_error("Failed to write to " + filename + " (" + strerror(errno) + ")") {}
};

/** @brief File instance is invalid */
struct MalformedFileException final : std::runtime_error {
  explicit MalformedFileException(const std::string& filename)
    : std::runtime_error("Malformed file " + filename + " (" + strerror(errno) + ")") {}
};

/**
 * @brief Asynchronous File Class
 *
 * Exposes standard file operations:
 *  - open
 *  - close
 *  - calculate size
 *  - move/retrieve read/write offset
 *  - read string (fixed amount, all, line)
 *  - read bytes (fixed amount, all)
 *  - write string/bytes
 *
 * Wraps around standard C functions: fopen/fclose/fread/fwrite/etc.
 * Also allows for manipulating pipes, and raw file descriptors (using fdopen)
 */
class File {
  enum Flag {
    FLAG_NONE   = 0,
    FLAG_OWNER  = (1 << 0), /// This instance is the owner of the underlying file handle
    FLAG_OPENED = (1 << 1), /// File is successfully opened
    FLAG_PIPE   = (1 << 2), /// File is a pipe
  };

  std::string          filename;
  FILE *               handle;
  std::atomic<uint8_t> flags;
  std::vector<size_t>  offsets;
public:
  File() : filename(""), handle(nullptr), flags(FLAG_NONE) {}

  /** @brief Wrap already opened file descriptor info File class */
  File(const int fd, const std::string& mode) : filename(std::to_string(fd)), handle(nullptr), flags(FLAG_NONE) {
    handle = fdopen(fd, mode.c_str());
    assertThrow(this->handle, FileOpenException(filename));
    setFlag(FLAG_OWNER, true);
    setFlag(FLAG_OPENED, true);
    setFlag(FLAG_PIPE, true);
  }

  /** @brief Open file */
  File(const std::string& filename, const std::string& mode) {
    open(filename, mode);
  }

  ~File() {
    close();
  }

  /** @brief Shortcut to make_shared<File> by file descriptor */
  static std::shared_ptr<File> fd(int fd, const std::string& mode) {
    return std::make_shared<File>(fd, mode);
  }

  /** @brief Shortcut to make_shared<File> by file path */
  static std::shared_ptr<File> create(const std::string& filename, const std::string& mode) {
    return std::make_shared<File>(filename, mode);
  }

  /** @brief Open the file from path */
  void open(const std::string& filename, const std::string& mode) {
    close();
    this->filename = filename;
    this->handle = fopen(filename.c_str(), mode.c_str());
    assertThrow(this->handle, FileOpenException(filename));
    setFlag(FLAG_OWNER, true);
    setFlag(FLAG_OPENED, true);
  }

  /** @brief Closes the file, if current instance is the owner, and handle is valid */
  void close() {
    if (getFlag(FLAG_OWNER) && handle) {
      fclose(handle);
      handle = nullptr;
    }

    filename = "";

    setFlag(FLAG_OPENED, false);
  }

  /**
   * @brief Disowns the file - close() won't close the handle
   * @warning Can lead to file descriptor leaks
   */
  void disown() {
    setFlag(FLAG_OWNER, false);
  }

  /**
   * @brief If file is not a pipe - compute size
   * @warning Doesn't cache the result, repeated calls may be slow
   * @warning Requires handle to be valid (file to be opened)
   */
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

  /**
   * @brief Perform rewind on an open file
   * @warning Requires handle to be valid (file to be opened)
   */
  void rewind() const {
    assertThrow(handle, FileNotOpenedException());

    ::rewind(handle);
  }

  /**
   * @brief Returns @c true if file offset has reached EOF
   * @warning Requires handle to be valid (file to be opened)
   */
  bool end() const {
    assertThrow(handle, FileNotOpenedException());
    return feof(handle);
  }

  /**
   * @brief Returns current read/write offset
   * @warning Requires handle to be valid (file to be opened)
   */
  size_t tell() const {
    assertThrow(handle, FileNotOpenedException());
    return ftell(handle);
  }

  /**
   * @brief Set file read/write offset to `ofs`
   * @warning Requires handle to be valid (file to be opened)
   */
  void seek(const ssize_t ofs) const {
    assertThrow(handle, FileNotOpenedException());
    if (ofs >= 0) {
      fseek(handle, ofs, SEEK_SET);
    } else {
      fseek(handle, ofs, SEEK_END);
    }
  }

  /** @brief Pushes current read/write offset into offset stack */
  void push() {
    offsets.push_back(tell());
  }

  /** @brief Pops current read/write offset from the offset stack */
  void pop() {
    seek(offsets.back());
    offsets.pop_back();
  }

  /** @brief Read `n` ascii chars from file into a string */
  std::shared_ptr<Future<std::string>> read(const size_t n) const {
    assertThrow(handle, FileNotOpenedException());
    return readInner<std::string>(n, true);
  }

  /** @brief Read all data from file into a string */
  std::shared_ptr<Future<std::string>> readAll() const {
    assertThrow(handle, FileNotOpenedException());
    return getFlag(FLAG_PIPE)
      ? readInnerBuffered<std::string>()
      : readInner<std::string>(size(), false);
  }

  /** @brief Read a line (up to a '\n' or an EOF) from file into a string */
  std::shared_ptr<Future<std::string>> readLine() const {
    assertThrow(handle, FileNotOpenedException());

    return async::to_thread<std::string>([this] {
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

  /** @brief Read `n` bytes from file into a byte vector */
  std::shared_ptr<Future<std::vector<uint8_t>>> readBytes(const size_t n) const {
    assertThrow(handle, FileNotOpenedException());
    return readInner<std::vector<uint8_t>>(n, true);
  }

  /** @brief Read all data from file into a byte vector */
  std::shared_ptr<Future<std::vector<uint8_t>>> readAllBytes() const {
    assertThrow(handle, FileNotOpenedException());
    return getFlag(FLAG_PIPE)
      ? readInnerBuffered<std::vector<uint8_t>>()
      : readInner<std::vector<uint8_t>>(size(), false);
  }

  /** @brief Read contents of string `s` into a file */
  std::shared_ptr<Future<>> write(std::string s) const {
    assertThrow(handle, FileNotOpenedException());
    return async::to_thread([this, s] {
      assertThrow(fwrite(s.c_str(), s.size(), 1, this->handle) == 1, FileWriteException(filename));
    });
  }

  /** @brief Write contents of byte vector `buf` into a file */
  std::shared_ptr<Future<>> writeBytes(const std::vector<uint8_t>& buf) const {
    assertThrow(handle, FileNotOpenedException());
    return async::to_thread([this, buf] {
      assertThrow(fwrite(buf.data(), buf.size(), 1, this->handle) == 1, FileWriteException(filename));
    });
  }

private:
  /** @brief Returns true if current flags state intersects with `mask` */
  bool getFlag(const uint8_t mask) const {
    uint8_t flags = this->flags.load();
    return (flags & mask) > 0;
  }

  /** @brief Set `mask` flags to 0 or 1, depending on `state` */
  void setFlag(const uint8_t mask, const bool state) {
    uint8_t flags = this->flags.load();
    if (state) {
      flags |= mask;
    } else {
      flags &= ~mask;
    }
    this->flags.store(flags);
  }

  /**
   * @brief Base implementation for read* methods (for non-pipe files)
   *
   * @tparam T     Type of container to put the result into (vector or string)
   * @param  n     Size to read
   * @param  check Check that fread actually read requested number of bytes
   */
  template <typename T>
  std::shared_ptr<Future<T>> readInner(const size_t n, bool check) const {
    return async::to_thread<T>([this, n, check] {
      if (n == 0) return T {};

      T buf;
      buf.resize(n);

      const size_t sz = fread(buf.data(), 1, n, this->handle);

      if (check) assertThrow(sz == n, FileReadException(filename));
      if (sz < n) buf.resize(sz);

      return buf;
    });
  }

  /**
   * @brief Base implementation for readAll* when the file is a pipe
   *
   * Needed, since pipes don't have size, so read is done in chunks of
   * `ASYNC_IO_FILE_BUFFERED_READ_SIZE` bytes, one chunk at a time
   *
   * @tparam T Type of container to put the result into (vector or string)
   */
  template <typename T>
  std::shared_ptr<Future<T>> readInnerBuffered() const {
    return async::to_thread<T>([this] {
      T result;

      while (true) {
        uint8_t buffer[ASYNC_IO_FILE_BUFFERED_READ_SIZE];
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
