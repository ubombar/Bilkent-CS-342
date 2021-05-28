
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

    void* mem1 = sbmem_alloc(512);
    void* mem2 = sbmem_alloc(128);
    void* mem3 = sbmem_alloc(128);
    void* mem4 = sbmem_alloc(128);
    void* mem5 = sbmem_alloc(128);

    printf("mem1=%p, mem2=%p, mem3=%p, mem4=%p, mem5=%p\n", mem1, mem2, mem3, mem4, mem5);
    // printf("The difference between them should be 512, mem2 - mem1 = %d\n", mem2 - mem1);

    sbmem_free(mem2);

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