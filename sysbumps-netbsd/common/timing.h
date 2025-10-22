#ifndef TIME_H
#define TIME_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

extern uint64_t timestamp;
extern pthread_t counting_thread;
extern uint64_t timer_overhead;

#define timer_read(x) x = timestamp
// #define timer_read(x) __asm__ volatile("mrs %[time], S3_2_c15_c0_0" :
// [time]"=r"(x));
void start_timer();
void stop_timer();

static inline __attribute__((always_inline)) unsigned long
get_timer_overhead(void) {
  unsigned long t1; /* start time */
  unsigned long t2; /* end time */

  asm __volatile__("mfence               \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "lfence               \n"
                   "movq %%rax, %[start] \n"
                   "nop                  \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "movq %%rax, %[stop]  \n"
                   /* output */
                   : [start] "=&r"(t1), [stop] "=&r"(t2)
                   /* input */
                   :
                   /* clobber */
                   : "%rax", "%rdx", "memory");
  /* Return timing overhead */
  return t2 - t1;
}

#endif
