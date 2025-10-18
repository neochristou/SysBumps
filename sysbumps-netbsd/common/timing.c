#include "timing.h"

void *counting(void *ctx) {
  __asm__ volatile("MOVQ (%[ctx]), %%rax\n"
                   "loop:\n"
                   "    addq $1, %%rax\n"
                   "    movq %%rax, (%[ctx])\n"
                   "    jmp loop\n"
                   :
                   : [ctx] "r"(ctx)
                   : "%rax", "memory");
  return NULL;
}

void start_timer() {
  pthread_create(&counting_thread, NULL, counting, &timestamp);
}

void stop_timer() { pthread_cancel(counting_thread); }
