#pragma once

#include <unordered_map>
#include <memory>

namespace async {

/**
 * @brief Singleton that manges allocated stacks, reusing them instead of
 *        freeing and re-allocating each time
 */
class StackPool {
  struct {
    std::unordered_map<uint8_t *, size_t> free;
    std::unordered_map<uint8_t *, size_t> used;
  } stacks;

  static inline std::shared_ptr<StackPool> instance;

public:
  StackPool() = default;

  /**
   * @brief Returns memory to the OS
   */
  ~StackPool() {
    for (const auto & map : {stacks.free, stacks.used}) {
      for (const auto& item : map) {
        delete[] item.first;
      }
    }

    stacks.free.clear();
    stacks.used.clear();
  }

  /** @brief Retrieve (and lazy-initialize if needed) StackPool instance */
  static std::shared_ptr<StackPool> get() {
    if (!instance) {
      instance = std::make_shared<StackPool>();
    }

    return instance;
  }

  /**
   * @brief Allocation request. Will search `stacks.free` before resorting to
   *        allocating new memory
   */
  uint8_t * alloc(const size_t size) {
    uint8_t * ptr = nullptr;

    for (const auto& item : stacks.free) {
      if (item.second == size) {
        ptr = item.first;
        stacks.free.erase(ptr);
        break;
      }
    }

    if (!ptr) {
      ptr = new uint8_t[size];
    }

    stacks.used[ptr] = size;

    return ptr;
  }

  /**
   * @brief Move allocated `ptr` from `stacks.used` to `stacks.free`. Won't
   *        return memory to the system
   */
  void free(uint8_t * ptr) {
    const auto size = stacks.used[ptr];
    stacks.used.erase(ptr);
    stacks.free[ptr] = size;
  }
};

}
