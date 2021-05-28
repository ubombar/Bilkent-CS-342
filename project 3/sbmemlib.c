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

int heap_buddy(int c) {
    // root cannot have a buddy!
    if (c == 0) {
        return -1;
    }

    // return the buddy
    if (c % 2 == 1) {
        return c + 1;
    } else {
        return c - 1;
    }
}

int heap_parent(int c) {
    if (c == 0) {
        return -1;
    }

    return (c - 1) / 2;
}

int __deallocate_on_bitmap(int heap_index) {
    const int max_depth = 1 + __log2(segment_size / (0x01 << MIN_MEMORY_POW));
    int index_depth = 0;
    int leftmost = 0;
    int rightmost = 0;

    // find the depth of the index
    for (size_t depth = 0; depth < max_depth; depth++) {
        if (leftmost <= heap_index && rightmost >= heap_index) {
            index_depth = depth;
            break;
        }
        // printf("leftmost=%d, rightmost=%d, heap_index=%d\n", leftmost, rightmost, heap_index);
        leftmost = heap_left(leftmost);
        rightmost = heap_right(rightmost);
    }

    int heap_req = heap_index;

    while (1) {
        if (heap_req > freelist_size) {
            heap_req = heap_parent(heap_req);
            break;
        }

        if (freelist[heap_req] == 0) {
            heap_req = heap_parent(heap_req);
            break;
        }

        heap_req = heap_left(heap_req);
    }

    printf("heap_index=%d\n", heap_index);
    printf("heap_req=%d\n", heap_req);

    // set the current node to available
    int h_left = heap_left(heap_req);
    int h_right = heap_right(heap_req);

    // not enough memory!
    // printf("freelist_size=%d\n", freelist_size);
    if (h_left < freelist_size && h_right < freelist_size) {
        if (freelist[h_left] != 0 || freelist[h_right] != 0) {
            // printf("> wow\n");
            return -1; // already allocated!
        }
    }

    // printf("h_left=%d, h_right=%d\n", h_left, h_right);

    // current chunk is allocated
    freelist[heap_req] = 0;
    heap_req = heap_parent(heap_req);

    while (1) {
        // successfull exit
        if (heap_req == 0) {
            return 0;
        }
        
        int h_left = heap_left(heap_req);
        int h_right = heap_right(heap_req);

        // not enough memory!
        // printf("freelist_size=%d\n", freelist_size);
        if (freelist[h_left] != 0 || freelist[h_right] != 0) {
            break;
        }
        freelist[heap_req] = 0;

        int heap_req_buddy = heap_buddy(heap_req);

        // If buddy is also available, merge and proceed to up
        if (freelist[heap_req_buddy] == 0) {
            heap_req = heap_parent(heap_req);
        } else {
            return 0;
        }
    }
    

    return 0;
}

// allocate memory on bitmap, size_pow2 is power of two!
// return the heap index of the allocated chunk
int __allocate_on_bitmap(int size_pow2, int heap_index, int depth) {
    int current_size = segment_size >> depth;

    // // already allocated! Cannot allocate // NOT CORRECT!
    // if (freelist[heap_index] == 1) {
    //     return -1;
    // }

    if (current_size == size_pow2) {
        // Test if the hole is empty!

        if (freelist[heap_index] == 1) {
            return -1; // already allocated!
        }

        freelist[heap_index] = 1;
        return heap_index;
    } else if (current_size > size_pow2) {
        // check the childs if anyone available
        
        int h_left = heap_left(heap_index);
        int h_right = heap_right(heap_index);

        // not enough memory!
        if (h_left > freelist_size || h_right > freelist_size) {
            return -1;
        }

        printf("h_left=%d, h_right=%d\n", h_left, h_right);

        // current chunk is allocated
        if (freelist[heap_index] == 1 && freelist[h_left] == 0 && freelist[h_right] == 0) {
            // printf("ff\n");
            return -1; // already allocated!
        }

        int left_allocation_index = __allocate_on_bitmap(size_pow2, h_left, depth + 1);

        if (left_allocation_index != -1) {
            freelist[heap_index] = 1;
            return left_allocation_index;
        }

        int right_allocation_index = __allocate_on_bitmap(size_pow2, h_right, depth + 1);

        if (right_allocation_index != -1) {
            freelist[heap_index] = 1;
            return right_allocation_index;
        }

        return -1; // not enough memory!
    } else {
        return -1; // not enough memory!
    }
}

int __max(int a, int b) {
    return a > b ? a : b;
}

int __print_heap() {
    int next_pow2 = 1;

    for (size_t i = 0; i < freelist_size; i++) {
        if (i == next_pow2 - 1) {
            printf("\n");
            next_pow2 = next_pow2 << 1;
        }

        printf("%d ", freelist[i]);
    }

    printf("\n");
}

void* __heap_index_to_ptr(int heap_index) {
    const int max_depth = 1 + __log2(segment_size / (0x01 << MIN_MEMORY_POW));
    int index_depth = 0;
    
    int leftmost = 0;
    int rightmost = 0;

    for (size_t depth = 0; depth < max_depth; depth++) {
        if (leftmost <= heap_index && rightmost >= heap_index) {
            // offset = CHUNK SIZE IN CURRENT DEPTH * INDEX OFFSET
            int offset = (segment_size >> depth) * (heap_index - leftmost);
            return segment + offset;
        }
        // printf("leftmost=%d, rightmost=%d, heap_index=%d\n", leftmost, rightmost, heap_index);
        leftmost = heap_left(leftmost);
        rightmost = heap_right(rightmost);
    }

    return NULL;    
}

int __log2(int x) {
    if (x <= 0) {
        return -1;
    }

    int l = 0;
    int n = 1;

    while (n < x) {
        l += 1;
        n *= 2;
    }

    return l;
    
}

int __ptr_to_heap_index(void* ptr) {
    int offset = ptr - segment;
    const int max_depth = 1 + __log2(segment_size / (0x01 << MIN_MEMORY_POW));
    int index_offset = 0;

    for (size_t depth = 0; depth < max_depth; depth++) {
        int rdepth = max_depth - depth - 1;
        int chunk_size = (0x01 << (MIN_MEMORY_POW + rdepth)); //  for depth this is segmnet size

        // printf("max_depth=%d, pow=%d\n", max_depth, 0x01 << depth);
        if (offset % chunk_size == 0) {

            return index_offset + offset / chunk_size;
        }

        index_offset = index_offset + 0x01 << depth;
    }
    return -1;
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
    int free_list_size = (2 * (segsize_c / min_memory_size) - 1);

    int r = ftruncate(fd, sizeof(char) * free_list_size + segsize_c);
    
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

    __print_heap();

    reqsize = __max(to_multiple_of_two(reqsize), (0x01 << MIN_MEMORY_POW)); // cannot allocate less than 128 bytes!

    // printf("req size = %d\n", reqsize);

    int heap_index = __allocate_on_bitmap(reqsize, 0, 0);

    // printf("> allocation result = %d\n", heap_index);

    __print_heap();

    sem_post(&mutex);

    return __heap_index_to_ptr(heap_index);
}

void sbmem_free(void *ptr)
{
    sem_wait(&mutex);

    __print_heap();

    int heap_index_to_delete = __ptr_to_heap_index(ptr);

    if (heap_index_to_delete == -1) {
        return -1;
    }

    int r = __deallocate_on_bitmap(heap_index_to_delete);

    printf("> free result = %d\n", r);

    __print_heap();


    sem_post(&mutex);
}