#pragma once

namespace async::platform {

__attribute__((always_inline))
inline static void set_stack(void * stack) {
  __asm__ volatile ("mov %0, %%rsp\n" :: "r" (stack) : "memory");
}

__attribute__((always_inline))
inline static void * get_stack() {
  void * stack;
  __asm__ volatile ("mov %%rsp, %0\n" : "=r" (stack) :: "memory");
  return stack;
}

}
