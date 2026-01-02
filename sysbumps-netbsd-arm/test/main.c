#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "timing.h"
#include "memory.h"
#include "config.h"

#define NPAGES (WAYS * SETS)
#define ITERATIONS 10000
#define PAGE_OFFSET 0UL
#define ACCESS_PAGE 3

#define TLB_SET(addr) (((uint64_t)addr >> PAGESHIFT) & ((1 << SET_SHIFT) - 1))

char * eset_data;

int main() {
	uint64_t cycles, tmp;
	void *eset[WAYS];
	char *pages = NULL;
	char *test_addr = NULL;

	printf("Page size: %#x\n", PAGE_SIZE);
	printf("Total test pages: %d\n", NPAGES);

	eset_data = (char *) malloc(sizeof(char) * ESET_OFFSET * PAGE_SIZE);
	if (0 != posix_memalign(&pages, PAGE_SIZE, sizeof(char) * NPAGES * PAGE_SIZE)) {
		printf("Error in posix_memalign\n");
		exit(1);
	}

	char *access_addr = pages + ACCESS_PAGE * PAGE_SIZE + PAGE_OFFSET;
	printf("Access at %p (set %d, page offset %lu)\n", access_addr, 
			TLB_SET(access_addr), PAGE_OFFSET);

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
	printf("Noise test:\n");
	for (int i = 0; i < NPAGES; i++) {
		test_addr = pages + i * PAGE_SIZE;
		tmp = 0;
		gen_eset(test_addr, eset, &eset_data[0]);
		for (int r = 0; r < ITERATIONS; r++) {
			prime(eset);
			nop();
			tmp += probe(eset);
		}
		cycles = tmp / ITERATIONS;
		if (cycles > 1200) 
			printf("Avg access time for index %d (%#llx, set %d): %llu\n", 
					i, test_addr, TLB_SET(test_addr), cycles);
	}

	// Access
	printf("Access test:\n");
	for (int i = 0; i < NPAGES; i++) {
		test_addr = pages + i * PAGE_SIZE;
		tmp = 0;
		gen_eset(test_addr, eset, &eset_data[0]);
		//printf("Eset for %p, access at %p\n", 
		//		pages + i * PAGE_SIZE, 
		//		pages + ACCESS_PAGE * PAGE_SIZE);
		for (int r = 0; r < ITERATIONS; r++) {
			prime(eset);
			memory_access(access_addr);
			tmp += probe(eset);
		}
		cycles = tmp / ITERATIONS;
		if (cycles > 1200) 
			printf("Avg access time for index %d (%#llx, set %d): %llu\n", 
					i, test_addr, TLB_SET(test_addr), cycles);
	}

	stop_timer();

	return 0;
}
