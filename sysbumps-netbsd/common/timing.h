#ifndef TIME_H
#define TIME_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define timer_read(x) x = timestamp
// #define timer_read(x) __asm__ volatile("mrs %[time], S3_2_c15_c0_0" :
// [time]"=r"(x));
void start_timer();
void stop_timer();

#endif
