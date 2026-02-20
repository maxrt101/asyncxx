# asyncxx - Asynchronous backend for C++

asyncxx is a header-only library for asynchronous programming in C++. It is heavily inspired by pyhon/rust implementations of async/await features.  

Features:  
 - Full async task backend based on setjmp and some black magic  
 - Futures (can be used stand-alone or incorporated into your functions)  
 - Events (spin lock without the spinning)  
 - Built-in thread pool for resource-heavy tasks to be offloaded from main thread  
 - Asynchronous File class  
 - Asynchronous Process class (with dynamic IO)  
 - Easy to use API  
 - Examples in form of comprehensive test suite  

### How to build
Prerequisites:  
 - Unix based OS (Linux/MacOS)  
 - `gcc`/`clang` that supports C++23
 - `cmake`  

Steps:  
 - `cmake -B cmake-build -S .`
 - `cmake --build cmake-build`

To run the tests: `./cmake-build/async`  

### Example

```c++
auto ev = async::Event();

async_task_r(read_file, std::string, std::string filename) {
    return async::io::File(filename, "r").readAll()->await();
}

async_task(cat, std::string input) {
    auto proc = async::io::Process::create("cat", input);
    auto res = proc->await();
    
    if (res.exit_code != 0) {
        throw std::runtime_error("cat failed");
    }
}

async_task(worker) {
    try {
        auto contents = read_file("test.txt")->await();
        cat(contents)->await();
    } catch (...) {
        ev.notifyOne();
        throw;
    }
    
    ev.notifyOne();
}

async_task(waiter) {
    std::print("Waiting for other task to cat the file...\n");
    ev.wait();
    std::print("Done!\n");
}

async::run([] {
    async::gather(waiter(), worker());
});
```