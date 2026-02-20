#include <async/async.h>
#include <async/io/file.h>
#include <test.h>
#include <unistd.h>
#include <sys/fcntl.h>


#define TEST_FILE_NAME "__async_test.txt"
#define TEST_STR "1\n2\n3\n4\n5\n"
#define TEST_BYTES std::vector<uint8_t> {1, 2, 3, 4, 5}


extern test::TestSuite async_tests;


struct FileTestContext {
  FileTestContext(uint8_t * data, size_t size) {
    const int fd = open(TEST_FILE_NAME, O_CREAT | O_WRONLY, 0666);
    write(fd, data, size);
    close(fd);
  }

  ~FileTestContext() {
    unlink(TEST_FILE_NAME);
  }

  static std::unique_ptr<FileTestContext> string() {
    return std::make_unique<FileTestContext>((uint8_t *) TEST_STR, strlen(TEST_STR));
  }

  static std::unique_ptr<FileTestContext> bytes() {
    auto b = TEST_BYTES;
    return std::make_unique<FileTestContext>((uint8_t *) b.data(), b.size());
  }

  static std::string read() {
    const int fd = open(TEST_FILE_NAME, O_RDONLY);
    char buf[4096];
    size_t sz = ::read(fd, buf, sizeof(buf));
    return std::string(buf, sz);
  }

  static std::vector<uint8_t> readBytes() {
    const int fd = open(TEST_FILE_NAME, O_RDONLY);
    uint8_t buf[4096];
    size_t sz = ::read(fd, buf, sizeof(buf));
    return std::vector(buf, buf + sz);
  }
};


TEST(async_tests, async_io_file_read, "Read 4 chars from file asynchronously") {
  auto _ = FileTestContext::string();

  std::string res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "r")->read(4)->await();
  });

  TEST_ASSERT_EQ(res, "1\n2\n", "File contents didn't match");
}



TEST(async_tests, async_io_file_read_all, "Read file asynchronously") {
  auto _ = FileTestContext::string();

  std::string res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "r")->readAll()->await();
  });

  TEST_ASSERT_EQ(res, TEST_STR, "File contents didn't match");
}


TEST(async_tests, async_io_file_read_line, "Read line from file asynchronously") {
  auto _ = FileTestContext::string();

  std::string res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "r")->readLine()->await();
  });

  TEST_ASSERT_EQ(res, "1", "File contents didn't match");
}


TEST(async_tests, async_io_file_read_bytes, "Read 4 bytes from file asynchronously") {
  auto _ = FileTestContext::bytes();

  std::vector<uint8_t> res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "rb")->readBytes(4)->await();
  });

  const auto expected = std::vector<uint8_t> {1, 2, 3, 4};

  TEST_ASSERT_EQ(res, expected, "File contents didn't match");
}


TEST(async_tests, async_io_file_read_all_bytes, "Read bytes from file asynchronously") {
  auto _ = FileTestContext::bytes();

  std::vector<uint8_t> res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "rb")->readAllBytes()->await();
  });

  const auto expected = TEST_BYTES;

  TEST_ASSERT_EQ(res, expected, "File contents didn't match");
}


TEST(async_tests, async_io_file_write, "Write to file asynchronously") {
  auto _ = FileTestContext::string();

  async::run([] {
    async::io::File::create(TEST_FILE_NAME, "w")->write("test123")->await();
  });

  TEST_ASSERT_EQ(FileTestContext::read(), "test123", "File contents didn't match");
}


TEST(async_tests, async_io_file_write_bytes, "Write bytes to file asynchronously") {
  auto _ = FileTestContext::string();
  auto expected = std::vector<uint8_t>{42, 69, 0xaa, 0x55};

  async::run([expected] {
    async::io::File::create(TEST_FILE_NAME, "wb")->writeBytes(expected)->await();
  });

  TEST_ASSERT_EQ(FileTestContext::readBytes(), expected, "File contents didn't match");
}
