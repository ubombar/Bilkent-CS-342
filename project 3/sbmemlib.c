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

struct sbmem_node
{
    int size;
    unsigned char allocated;
    struct sbmem_node *next;
};

sem_t mutex;

void *segment;

unsigned int num_processes = 0;

struct sbmem_node head;

int len() {
    struct sbmem_node *tmp = &head;
    int l = 0;
    while (tmp != NULL)
    {
        l++;
        tmp = tmp->next;
    }

    return l;
}

void __reverse_patch_little_buddy(struct sbmem_node *n)
{
    if (n == NULL)
    {
        return;
    }

    __reverse_patch_little_buddy(n->next);

    if (n->next == NULL)
    {
        return;
    }

    struct sbmem_node *nn = n->next;

    if (n->size == nn->size && n->allocated == nn->allocated && n->allocated == 0)
    {
        struct sbmem_node *temp = nn->next;

        free(nn);

        n->size *= 2;
        n->next = temp;

        __reverse_patch_little_buddy(n);
        return;
    }
    return;
}

int __hello_little_buddy(struct sbmem_node *n, int s, int offset)
{
    if (n == NULL)
    {
        return -1;
    }

    if (n->allocated == 1)
    {
        return __hello_little_buddy(n->next, s, offset + n->size);
    }

    if (n->size == s)
    {
        n->allocated = 1;
        return offset;
    }
    else if (n->size > s)
    {
        int csize = n->size;
        struct sbmem_node *tmp = n->next;

        n->next = malloc(sizeof(struct sbmem_node));
        n->allocated = 0;
        n->size = csize >> 1;

        n->next->next = tmp;
        n->next->allocated = 0;
        n->next->size = csize >> 1;

        return __hello_little_buddy(n, s, offset);
    }
    else
    {
        return __hello_little_buddy(n->next, s, offset + n->size);
    }
}

void __print_little_buddy(int offset, int reqsize)
{
    // printf("Trying to allocate %d bytes. ", reqsize);

    // if (offset == -1)
    // {
    //     printf("Failed to allocate %d bytes!\n", reqsize);
    // }
    // else
    // {
    //     printf("Allocated %d bytes with offset %d\n", reqsize, offset);
    // }

    struct sbmem_node *tmp = &head;
    while (tmp != NULL)
    {
        printf("(%d, %d)->", tmp->size, tmp->allocated);
        tmp = tmp->next;
    }
    printf("\n");
}

int __bye_little_buddy(struct sbmem_node *n, int offset)
{
    if (n == NULL)
    {
        return -1;
    }

    if (offset == 0)
    {
        if (n->allocated != 1)
        {
            return -1;
        }
        n->allocated = 0;

        // if (n->next != NULL) {
        //     struct sbmem_node* nn = n->next;

        //     if (n->size == nn->size && (n->allocated == nn->allocated) == 0) {
        //         struct sbmem_node* temp = nn->next;

        //         n->size *= 2;
        //         n->next = temp;

        //         free(nn);
        //     }
        // }
        return 1;
    }
    else
    {
        return __bye_little_buddy(n->next, offset - n->size);
    }
}

// This will be initialized by one process thus no need for semaphores.
int sbmem_init(int segsize)
{
    if (segsize <= 0)
    {
        return -1;
    }

    sem_init(&mutex, 1, 1);

    int tmp = segsize;
    int num_bits = 0;

    for (int i = 0; i < sizeof(int) * 8; i++)
    {
        num_bits = num_bits + (tmp % 2);
        tmp = tmp >> 1;
    }

    // segsize is not a multiple of 2
    if (num_bits != 1)
    {
        return -1;
    }

    int fd = shm_open("shared_m", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd == -1)
    {
        shm_unlink("shared_m");
        fd = shm_open("shared_m", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    }

    if (fd == -1)
    {
        return -1;
    }

    int r = ftruncate(fd, segsize);

    if (r == -1)
    {
        return -1;
    }

    segment = mmap(NULL, segsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (segment == NULL)
    {
        return -1;
    }

    head.allocated = 0;
    head.next = NULL;
    head.size = segsize;

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
    if (reqsize <= 0)
    {
        return NULL;
    }
    {
        // Normalize the reqsize to power of two
        int tmp = reqsize;
        int msb = 0;
        int cbit = 0;
        for (size_t i = 0; i < sizeof(reqsize) * 8; i++)
        {
            if (tmp % 2 == 1)
            {
                msb = i;
                cbit++;
            }
            tmp = tmp >> 1;
        }

        if (cbit != 1)
        {
            msb++;
        }

        // Set reqsize to power of two
        reqsize = 1 << msb;
    }

    // Buddy allocation algorithm
    sem_wait(&mutex);

    int offset = __hello_little_buddy(&head, reqsize, 0);

#ifdef DEBUG
    __print_little_buddy(offset, reqsize);
#endif

    sem_post(&mutex);

    if (offset == -1)
    {
        return NULL;
    }

    return segment + offset;
}

void sbmem_free(void *ptr)
{
    sem_wait(&mutex);
    int offset = ptr - segment;

    int r = __bye_little_buddy(&head, offset);

    __reverse_patch_little_buddy(&head);

#ifdef DEBUG
    __print_little_buddy(0, 0);
#endif

    sem_post(&mutex);
}