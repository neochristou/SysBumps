#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "config.h"

struct eset_args {
    uint64_t set_base_addr;        /* Input: Address used to calculate the set index */
    uint64_t eset_out[WAYS];       /* Output: Array of kernel addresses belonging to that set */
} __attribute__((packed));

struct probe_args {
    uint64_t target_addr;          /* Input: The address to access (trigger access) */
    uint64_t eset_in[WAYS];        /* Input: The eviction set addresses to use */
    uint8_t  cond;                 /* Input: 1 = Access target, 0 = Skip access */
    uint32_t iterations;
    uint64_t result_cycles;        /* Output: Timing result */
} __attribute__((packed));


#define TLBMOD_CMD_GEN_ESET  _IOWR('T', 1, struct eset_args)
#define TLBMOD_CMD_PROBE     _IOWR('T', 2, struct probe_args)
#define TLBMOD_ALLOC_PAGE  _IOWR('T', 3, unsigned long)
#define TLBMOD_FREE_PAGE   _IOW('T', 4, unsigned long)
#define TLBMOD_ACCESS   _IOW('T', 5, unsigned long)
/* --- Display & Configuration Macros --- */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define NPAGES (WAYS * SETS)
#define ITERATIONS 100
#define PAGE_OFFSET 0

#define KERNEL_ACCESS_ADDR (KERNBASE + 15 * PAGE_SIZE)

int g_fd = -1;

int setup_driver(void) {
    if (g_fd != -1) return 0;

    g_fd = open("/dev/tlbmod0", O_RDWR);
    if (g_fd < 0) {
        perror("Failed to open /dev/tlbmod0");
        return -1;
    }
    return 0;
}

int get_eviction_set(uint64_t target_addr, uint64_t *eset_array_out) {
    struct eset_args args;
    args.set_base_addr = target_addr;

    if (ioctl(g_fd, TLBMOD_CMD_GEN_ESET, &args) == -1) {
        perror("ioctl GEN_ESET failed");
        return -1;
    }

    memcpy(eset_array_out, args.eset_out, sizeof(uint64_t) * WAYS);
    return 0;
}

int run_prime_probe(uint64_t target_addr, uint64_t *eset_array, uint64_t *result) {
    struct probe_args args;
    args.target_addr = target_addr;
    args.cond = 1;
    args.iterations = ITERATIONS;

    memcpy(args.eset_in, eset_array, sizeof(uint64_t) * WAYS);

    if (ioctl(g_fd, TLBMOD_CMD_PROBE, &args) == -1) {
        perror("ioctl PROBE failed");
        return -1;
    }

    *result = args.result_cycles;
    return 0;
}

unsigned long alloc_kernel_page() {
    unsigned long kva = 0;

    if (ioctl(g_fd, TLBMOD_ALLOC_PAGE, &kva) == -1) {
        perror("[-] ioctl alloc failed");
        exit(1);
    }

    return kva;
}

int free_kernel_page(unsigned long kva) {
    if (kva == 0) {
        fprintf(stderr, "[-] Error: Attempted to free NULL KVA.\n");
        exit(1);
    }

    if (ioctl(g_fd, TLBMOD_FREE_PAGE, &kva) == -1) {
        perror("[-] ioctl free failed");
        exit(1);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    uint64_t cycles, total_cycles;
    int ret, set, target_set;
    char *test_addr;
    char *pages = NULL;
    uint64_t eset[WAYS];

    if (setup_driver() != 0) {
        return 1;
    }

    // char *access_addr = KERNEL_ACCESS_ADDR;
    //char *access_addr; 
    //posix_memalign(&access_addr, PAGE_SIZE * 4, PAGE_SIZE);
    char *access_addr = (char *)alloc_kernel_page();
    printf("Allocated kernel buffer at %p (Set %3d)\n", access_addr, TLB_SET(access_addr));

    printf("Total test pages: %d\n", NPAGES);

    if (0 != posix_memalign(&pages, ESET_OFFSET, sizeof(char) * NPAGES * PAGE_SIZE)) {
        printf("Error in posix_memalign\n");
        return;
    }
    printf("Allocated pages at %p (set %d)\n", pages, TLB_SET(pages));

    get_eviction_set((uint64_t)access_addr, eset);
    run_prime_probe((uint64_t)access_addr, eset, &cycles);

    for (int a = 0; a < 4; a++) {
       target_set = TLB_SET(access_addr);
       printf("Access test for %p (Set %3d):\n", access_addr, target_set);
       for (int i = 0; i < SETS; i++) {
           test_addr = pages + i * PAGE_SIZE + PAGE_OFFSET;
           set = TLB_SET(test_addr);

           if (get_eviction_set((uint64_t)test_addr, eset) != 0) {
               printf("Error in get_eviction_set\n");
               return 1;
           }

           //printf("Testing addr %p (Set %3d)\n", test_addr, set);
           //printf("Eviction set:\n");
           //for (int e = 0; e < WAYS; e++) {
           //    printf("\t%p (Set %3d)\n", eset[e], TLB_SET(eset[e]));
           //}
           fflush(stdout);
           ret = run_prime_probe((uint64_t)access_addr, eset, &cycles);
           if (ret != 0) { 
               printf("Error in run_prime_probe\n");
               return 1;
           }   

           if (cycles > 25000) {
               printf(ANSI_COLOR_GREEN "\t%p evicted (Set %3d, High Latency: %llu)" ANSI_COLOR_RESET "\n", test_addr, set, cycles);
           } 
           //else {
           //    printf(ANSI_COLOR_YELLOW "Cached  (Low Latency:  %llu)" ANSI_COLOR_RESET "\n", cycles);
           //}
       }

       access_addr = access_addr + PAGE_SIZE;
    }
    

    free_kernel_page((unsigned long *)access_addr);
    close(g_fd);
    return 0;
}
