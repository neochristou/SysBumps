#include "timing.h"

// XXX Maybe don't do it atomically
void *counting(void *ctx) {
  __asm__ volatile("loop:\n"
                   "    LOCK INCQ (%[ctx])\n"
                   "    JMP loop\n"
                   :
                   : [ctx] "r"(ctx)
                   : "memory");
  return NULL;
}

void start_timer() {
  pthread_create(&counting_thread, NULL, counting, &timestamp);
}

void stop_timer() { pthread_cancel(counting_thread); }
