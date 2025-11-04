#include "memory.h"

#define ITERS 100000

extern uint8_t *flush_set;

int main() {

    void *eset_stlb1[STLB_EVSET_SIZE_4K];
    void *eset_stlb2[STLB_EVSET_SIZE_4K];

    void *eset_dtlb1[DTLB_EVSET_SIZE_4K];
    void *eset_dtlb2[DTLB_EVSET_SIZE_4K];

    register uint64_t tmp;
    uint64_t avg;
    char *test_addr;
    if (0 != posix_memalign((void **)&test_addr, PAGE_4K, PAGE_4K)) {
        printf("posix_memalign failed");
        exit(1);
    }

    pin_to_core(0);

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    get_timer_overhead();
    init_tlb_flush();

    gen_eset((size_t)test_addr, eset_stlb1, eset_dtlb1, (size_t)flush_set);
    gen_eset((size_t)test_addr, eset_stlb2, eset_dtlb2, (size_t)eset_stlb1[STLB_EVSET_SIZE_4K - 1] + PAGE_4K);

    tmp = 0;
    for (int i = 0; i < ITERS; i++) {
        prime(eset_stlb1);
        prime(eset_stlb2);
        // cache_flush(eset_stlb1);
        // cache_flush(eset_stlb2);
        tmp += probe(eset_stlb1);
    }

    avg = tmp / ITERS; printf("STLB set access time after evicting: %lu\n", avg);

    tmp = 0;
    for (int i = 0; i < ITERS; i++) {
        prime(eset_stlb1);
        maccess(test_addr);
        tmp += probe(eset_stlb1);
    }

    avg = tmp / ITERS; printf("STLB set access time after accessing addr: %lu\n", avg);

    tmp = 0;
    for (int i = 0; i < ITERS; i++) {
        prime(eset_stlb1);
        // cache_flush(eset_stlb1);
        tmp += probe(eset_stlb1);
    }

    avg = tmp / ITERS;
    printf("STLB set access time without evicting: %lu\n", avg);


    // tmp = 0;
    // for (int i = 0; i < ITERS; i++) {
    //     prime_l1(eset_dtlb1);
    //     prime_l1(eset_dtlb2);
    //     tmp += probe_l1(eset_dtlb1);
    // }

    // avg = tmp / ITERS;
    // printf("DTLB set access time after evicting: %lu\n", avg);

    // tmp = 0;
    // for (int i = 0; i < ITERS; i++) {
    //     prime_l1(eset_dtlb1);
    //     maccess(test_addr);
    //     tmp += probe_l1(eset_dtlb1);
    // }

    // avg = tmp / ITERS;
    // printf("DTLB set access time after accessing addr: %lu\n", avg);

    // tmp = 0;
    // for (int i = 0; i < ITERS; i++) {
    //     prime_l1(eset_dtlb1);
    //     tmp += probe_l1(eset_dtlb1);
    // }

    // avg = tmp / ITERS;
    // printf("DTLB set access time without evicting: %lu\n", avg);

}
