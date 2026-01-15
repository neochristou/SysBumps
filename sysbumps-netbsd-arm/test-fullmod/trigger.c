#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "tlbmod.h"

#define KERNBASE          0xffffc00000000000

int main() {
    int fd;
    struct access_args args;
    uint64_t my_data = 0;

    fd = open("/dev/tlbmod0", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/tlbmod0");
        return 1;
    }

    args.target_addr = my_data;
    args.cond = 1;

    //if (mlock(&my_data, sizeof(my_data)) == -1) {
    //    perror("mlock failed");
    //    close(fd);
    //    return 1;
    //}

    printf("[User] Sending IOCTL... Addr: %p\n", (void*)args.target_addr);

    /* 4. Trigger IOCTL */
    if (ioctl(fd, TLBMOD_CMD_ACCESS, &args) == -1) {
        perror("ioctl failed");
        close(fd);
        return 1;
    }

    printf("[User] Success.\n");
    close(fd);
    return 0;
}
