
// Initialize the memory segment and bookkeep part
int sbmem_init(int segsize);

// Destroy the initialized part
int sbmem_remove();

// For already initialized libraries, open new segment
int sbmem_open();

// For already initialized libraries, close the process access
int sbmem_close();

// Allocate given amount
void* sbmem_alloc(int reqsize);

// Free the allocated memory
void sbmem_free(void* ptr);

// For debug purposes
int len();