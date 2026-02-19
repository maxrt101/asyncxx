#include <async/async.h>
#include <async/io/file.h>
#include <test.h>
#include <unistd.h>
#include <sys/fcntl.h>

#define TEST_FILE_NAME "__async_test.txt"
#define TEST_STR "1\n2\n3\n4\n5\n"

extern test::TestSuite async_tests;


struct FileTestContext {
  FileTestContext() {
    const int fd = open(TEST_FILE_NAME, O_CREAT | O_WRONLY, 0666);
    write(fd, TEST_STR, strlen(TEST_STR));
    close(fd);
  }

  static std::string read() {
    const int fd = open(TEST_FILE_NAME, O_RDONLY);
    char buf[4096];
    size_t sz = ::read(fd, buf, sizeof(buf));
    return std::string(buf, sz);
  }

  ~FileTestContext() {
    unlink(TEST_FILE_NAME);
  }
};


TEST(async_tests, async_io_file_read_all, "Read file asynchronously") {
  auto _ = FileTestContext();

  std::string res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "r")->readAll()->await();
  });

  TEST_ASSERT_EQ(res, TEST_STR, "File contents didn't match");
}


TEST(async_tests, async_io_file_read_line, "Read line from file asynchronously") {
  auto _ = FileTestContext();

  std::string res;

  async::run([&res] {
    res = async::io::File::create(TEST_FILE_NAME, "r")->readLine()->await();
  });

  TEST_ASSERT_EQ(res, "1", "File contents didn't match");
}


TEST(async_tests, async_io_file_write, "Write to file asynchronously") {
  auto _ = FileTestContext();

  async::run([] {
    async::io::File::create(TEST_FILE_NAME, "w")->write("test123")->await();
  });

  TEST_ASSERT_EQ(FileTestContext::read(), "test123", "File contents didn't match");
}

