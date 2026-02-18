#pragma once

namespace async::platform {

__attribute__((always_inline))
static void set_stack(void * stack) {
  __asm__ volatile ("mov %0, %%rsp\n" :: "r" (stack) : "sp");
}

__attribute__((always_inline))
static void * get_stack() {
  void * stack;
  __asm__ volatile ("mov %%rsp, %0\n" : "=r" (stack) :: "sp");
  return stack;
}

}
