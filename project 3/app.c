
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "sbmem.h"


#define SEGSIZE 1024

int main()
{
    srand(time(NULL));

    int r = sbmem_init(SEGSIZE);
    printf("sbmem_init() => %d\n", r);

    r = sbmem_open();
    printf("sbmem_open() => %d\n", r);



    r = sbmem_close();
    printf("sbmem_close() => %d\n", r);

    pid_t pid = fork();

    if (pid != 0) {
        void* p = sbmem_alloc(128);
        printf("p1: sbmem_alloc(128) => %p\n", p);
        sbmem_free(p);

    } else {
        void* p = sbmem_alloc(512);
        printf("p2: sbmem_alloc(512) => %p\n", p);
        sbmem_free(p);
    }




    // if (ret == -1)
    //     exit(1);

    // printf("Allocate 512 bytes:   ");
    // char *a = sbmem_alloc(512);
    // printf("Allocate 32 bytes:    ");
    // void *b = sbmem_alloc(32);
    // printf("Allocate 8 bytes:     ");
    // void *c = sbmem_alloc(8);

    // // allocate space to for characters
    // for (int i = 0; i < ASIZE; ++i)
    //     a[i] = 'a'; // init all chars to ‘a’

    // printf("Deallocate 512 bytes: ");
    // sbmem_free(c);
    // printf("Deallocate 32 bytes:  ");
    // sbmem_free(b);
    // printf("Deallocate 8 bytes:   ");
    // sbmem_free(a);

    // printf("\n");

    // sbmem_close();

    // sbmem_remove();
    // return (0);
}