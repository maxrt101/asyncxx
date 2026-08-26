/**
 * @brief x86-64 Platform port
 */
#pragma once

namespace async::platform {

/**
 * @brief Set stack pointer (rsp) to provided `stack` value
 *
 * @param stack New stack pointer
 */
__attribute__((always_inline))
inline static void set_stack(void * stack) {
  __asm__ volatile ("mov %0, %%rsp\n" :: "r" (stack) : "memory");
}

/**
 * @brief Retrieves current stack pointer (rsp)
 *
 * @return the current value of RSP
 */
__attribute__((always_inline))
inline static void * get_stack() {
  void * stack;
  __asm__ volatile ("mov %%rsp, %0\n" : "=r" (stack) :: "memory");
  return stack;
}

}
