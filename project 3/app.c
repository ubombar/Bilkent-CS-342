
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "sbmem.h"

#define ASIZE 250
#define SEGSIZE 1024

void test_lib(int n, int min, int max)
{
    char *arr[n];

    for (int i = 0; i < n; i++)
    {
        int s = (int) (((double) rand() / RAND_MAX) * (max - min) + min);
        arr[i] = sbmem_alloc(s);
        printf("%d, %d, %d\n", i, s, len());
    }

    printf("\n");

    for (int i = 0; i < n; i++)
    {
        if (arr[i] != NULL) {
            sbmem_free(arr[i]);
        }
        printf("%d, %d\n", i, len());
    }
    printf("\n");
}

int main()
{
    srand(time(NULL));

    // test_lib(100, 128, 1024);

    // int i, ret;
    // char *p;
    sbmem_init(SEGSIZE);

    int ret = sbmem_open();


    if (ret == -1)
        exit(1);

    printf("Allocate 512 bytes:   ");
    char *a = sbmem_alloc(512);
    printf("Allocate 32 bytes:    ");
    void *b = sbmem_alloc(32);
    printf("Allocate 8 bytes:     ");
    void *c = sbmem_alloc(8);

    // allocate space to for characters
    for (int i = 0; i < ASIZE; ++i)
        a[i] = 'a'; // init all chars to ‘a’

    printf("Deallocate 512 bytes: ");
    sbmem_free(c);
    printf("Deallocate 32 bytes:  ");
    sbmem_free(b);
    printf("Deallocate 8 bytes:   ");
    sbmem_free(a);

    printf("\n");

    sbmem_close();

    sbmem_remove();
    return (0);
}