#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "timing.h"
#include "memory.h"
#include "config.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_ORANGE  "\033[38;5;214m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define NPAGES (WAYS * SETS)
#define ITERATIONS 10000
#define PAGE_OFFSET 0

// #define KERNEL_ACCESS_ADDR (KERNBASE + KERN_IMAGE_SIZE - 15 * PAGE_SIZE)
#define KERNEL_ACCESS_ADDR (KERNBASE + 15 * PAGE_SIZE)

char *user = "./";
char * eset_data;
int g_fd = -1;

#define TLBMOD_ALLOC_PAGE  _IOWR('T', 3, struct alloc_args)
#define TLBMOD_FREE_PAGE   _IOW('T', 4, struct alloc_args)
#define TLBMOD_ACCESS   _IOW('T', 5, unsigned long)
#define TLBMOD_ACCESS_COND  _IOW('T', 6, struct condacc_args)
#define TLBMOD_ACCESS_UNPRIV   _IOW('T', 7, unsigned long)
#define TLBMOD_ACCESS_COND_UNPRIV  _IOW('T', 8, struct condacc_args)
#define TLBMOD_ACCESS_WORD   _IOW('T', 9, unsigned long)
#define TLBMOD_ACCESS_COND_WORD  _IOW('T', 10, struct condacc_args)

struct alloc_args {
	int  npages;
	int  nguard_pages;
	uint64_t buffer_addr;
} __attribute__((packed));

struct condacc_args {
	uint64_t target_addr;          /* Input: The address to access (trigger access) */
	uint8_t  cond;                 /* Input: 1 = Access target, 0 = Skip access */
} __attribute__((packed));

int setup_driver(void) {
	if (g_fd != -1) return 0;

	g_fd = open("/dev/tlbmod0", O_RDWR);
	if (g_fd < 0) {
		perror("Failed to open /dev/tlbmod0");
		return -1;
	}
	return 0;
}

int do_access_addr_cond(uint64_t target_addr, uint8_t cond) {
	struct condacc_args args;
	args.target_addr = target_addr;
	args.cond = cond;

	if (ioctl(g_fd, TLBMOD_ACCESS_COND, &args) == -1) {
		perror("ioctl access cond failed");
		return -1;
	}

	return 0;
}

int do_access_addr_cond_word(uint64_t target_addr, uint8_t cond) {
	struct condacc_args args;
	args.target_addr = target_addr;
	args.cond = cond;

	if (ioctl(g_fd, TLBMOD_ACCESS_COND_WORD, &args) == -1) {
		perror("ioctl access cond word failed");
		return -1;
	}

	return 0;
}

int do_access_addr_cond_unpriv(uint64_t target_addr, uint8_t cond) {
	struct condacc_args args;
	args.target_addr = target_addr;
	args.cond = cond;

	if (ioctl(g_fd, TLBMOD_ACCESS_COND_UNPRIV, &args) == -1) {
		perror("ioctl access cond unpriv failed");
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

int free_kernel_page(unsigned long kva, int npages) {

	struct alloc_args args;
	args.npages = npages;
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

int do_access_addr(unsigned long kva) {
	if (kva == 0) {
		fprintf(stderr, "[-] Error: Attempted to access NULL KVA.\n");
		exit(1);
	}

	if (ioctl(g_fd, TLBMOD_ACCESS, &kva) == -1) {
		perror("[-] ioctl access failed");
		exit(1);
	}

	return 0;
}

int do_access_addr_unpriv(unsigned long kva) {
	if (kva == 0) {
		fprintf(stderr, "[-] Error: Attempted to access NULL KVA.\n");
		exit(1);
	}

	if (ioctl(g_fd, TLBMOD_ACCESS_UNPRIV, &kva) == -1) {
		perror("[-] ioctl access failed");
		exit(1);
	}

	return 0;
}

int main() {
	uint64_t cycles, tmp;
	void *eset[WAYS];
	char *pages = NULL;
	char *test_addr = NULL;
	int set;

	if (setup_driver() != 0) {
		return 1;
	}

	printf("Page size: %#x\n", PAGE_SIZE);
	printf("Total test pages: %d\n", NPAGES);
	printf("Code at: %p (set %d)\n", &main, TLB_SET(&main));

	//eset_data = (char *) malloc(sizeof(char) * ESET_OFFSET * PAGE_SIZE);
	posix_memalign(&eset_data, ESET_OFFSET, sizeof(char) * ESET_OFFSET * PAGE_SIZE);
	if (0 != posix_memalign(&pages, ESET_OFFSET, sizeof(char) * NPAGES * PAGE_SIZE)) {
		printf("Error in posix_memalign\n");
		exit(1);
	}
	printf("Allocated pages at %p (set %d)\n", pages, TLB_SET(pages));
	printf("User address at %p (set %d)\n", user, TLB_SET(user));

	//char *access_addr = (char *)KERNEL_ACCESS_ADDR;
	//char *access_addr; 
	//posix_memalign(&access_addr, PAGE_SIZE * 4, PAGE_SIZE);
	char *access_addr = (char *)alloc_kernel_buf(4, 0);
	//printf("Allocated kernel buffer at %p (Set %3d)\n", access_addr, TLB_SET(access_addr));

	int target_set = TLB_SET(access_addr);
	printf("Access at %p (set %d, page offset %lu)\n", access_addr, 
			target_set, PAGE_OFFSET);

	start_timer();

	gen_eset(access_addr, eset, &eset_data[0]);
	for (int r = 0; r < ITERATIONS; r++) {
		prime(eset);
		nop();
		tmp += probe(eset);
	}
	cycles = tmp / ITERATIONS;
	printf("Access time overhead: %llu\n", cycles);

	// No access
	//printf("Noise test:\n");
	//for (int i = 0; i < NPAGES; i++) {
	//	test_addr = pages + i * PAGE_SIZE + PAGE_OFFSET;
	//	tmp = 0;
	//	gen_eset(test_addr, eset, &eset_data[0]);
	//	for (int r = 0; r < ITERATIONS; r++) {
	//		prime(eset);
	//		nop();
	//		tmp += probe(eset);
	//	}
	//	cycles = tmp / ITERATIONS;
	//	if (cycles > 1300) 
	//		printf("Avg access time for index %d (%#llx, set %d): %llu\n", 
	//				i, test_addr, TLB_SET(test_addr), cycles);
	//}

	void* target[TRAINING_ITERS] = {user, user, user, user, user};
	uint8_t flags[TRAINING_ITERS] = {1, 1, 1, 1, 1, 0};
	// Access
	for (int a = 0; a < 4; a++) {
		target[TRAINING_ITERS - 1] = access_addr;
		printf("Access test for %p (set %d):\n", access_addr, target_set);
		for (int i = 0; i < SETS; i++) {
			test_addr = pages + i * PAGE_SIZE + PAGE_OFFSET;
			tmp = 0;
			gen_eset(test_addr, eset, &eset_data[0]);
			for (int r = 0; r < ITERATIONS; r++) {
				for (int t = 0; t < TRAINING_ITERS; t++) {
					prime(eset);
					do_access_addr_cond_unpriv((unsigned long)target[t], flags[t]);
				}
				tmp += probe(eset);
			}
			cycles = tmp / ITERATIONS;
			set = TLB_SET(test_addr);
			if (cycles > 1250) {
				if (set == target_set) 	
					printf(ANSI_COLOR_GREEN);
				if (set == target_set + 1) 	
					printf(ANSI_COLOR_ORANGE);
				printf("Avg access time for index %d (%#llx, set %d): %llu\n" 
						ANSI_COLOR_RESET, i, test_addr, set, cycles);
			}
		}
		access_addr = access_addr + PAGE_SIZE;
		target_set = TLB_SET(access_addr);
		}

		stop_timer();

		return 0;

		free_kernel_page((unsigned long *)access_addr, 4);
	}
