
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

    sbmem_alloc(256);
    sbmem_alloc(512);
    sbmem_alloc(512);

    // pid_t pid = fork();

    // if (pid != 0) {
    //     void* p = sbmem_alloc(128);
    //     printf("p1: sbmem_alloc(128) => %p\n", p);
    //     sbmem_free(p);

    // } else {
    //     void* p = sbmem_alloc(512);
    //     printf("p2: sbmem_alloc(512) => %p\n", p);
    //     sbmem_free(p);
    // }

    return 0;
}