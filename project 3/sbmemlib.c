#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sbmem.h"
#include <pthread.h>
#include <semaphore.h>

#define DEBUG

#define MIN_MEMORY_POW 7 // 2^7=128

sem_t mutex;

// Memory and freelist
void *segment;
int segment_size;

char* freelist;
int freelist_size;

unsigned int num_processes = 0;

int to_multiple_of_two(int n) {
    if (n <= 0) {
        return n;
    }

    int mul = 1;

    while (mul < n) {
        mul = mul << 1;
    }

    return mul;
}

int heap_left(int c) {
    return 2 * c + 1;
}

int heap_right(int c) {
    return 2 * c + 2;
}

// This will be initialized by one process thus no need for semaphores.
int sbmem_init(int segsize)
{
    // Cannot be negative
    if (segsize <= 0) {
        return -1;
    }

    int segsize_c = to_multiple_of_two(segsize);
    const int min_memory_size = (0x01 << MIN_MEMORY_POW);

    // Not Multiple of two
    if (segsize != segsize_c) {
        return -1;
    }

    // Cannot be smaller than minimum memory
    if (segsize_c < min_memory_size) {
        return -1;
    }

    int fd = shm_open("shared_m", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        shm_unlink("shared_m");
        fd = shm_open("shared_m", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    }

    // Cannot open memory
    if (fd == -1) {
        return -1;
    }

    // allocate the free list heap inside the memory segment
    int free_list_size = sizeof(char) * (2 * (segsize_c / min_memory_size) - 1);

    int r = ftruncate(fd, free_list_size + segsize_c);
    
    // If cannot truncate
    if (r == -1) {
        return -1;
    }

    void* mmap_segmnet = mmap(NULL, segsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    // If cannot create segmnet
    if (mmap_segmnet == NULL)
    {
        return -1;
    }

    freelist_size = free_list_size;
    freelist = mmap_segmnet;

    segment_size = segsize_c;
    segment = ((char*) mmap_segmnet) + freelist_size;

    sem_init(&mutex, 10, 1);

    return 0;
}

int sbmem_remove()
{
    shm_unlink("shared_m");
    sem_destroy(&mutex);

    return 0;
}

int sbmem_open()
{
    sem_wait(&mutex);
    if (num_processes >= 10)
    {
        sem_post(&mutex);
        return -1;
    }
    num_processes += 1;
    sem_post(&mutex);
    return 0;
}

int sbmem_close()
{
    sem_wait(&mutex);
    if (num_processes <= 0)
    {
        sem_post(&mutex);
        return -1;
    }
    num_processes -= 1;
    sem_post(&mutex);
    return 0;
}

void *sbmem_alloc(int reqsize)
{
    sem_wait(&mutex);

    sem_post(&mutex);

    return 0;
}

void sbmem_free(void *ptr)
{
    sem_wait(&mutex);



    sem_post(&mutex);
}