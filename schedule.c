#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>

#define NO_OF_BURSTS 3

#define ALGO_INVALID -1
#define ALGO_FCFS 0
#define ALGO_SJF 1
#define ALGO_PRIO 2
#define ALGO_VRUNTIME 3

#define N_MAX 10

// structs and typedefs
typedef struct
{
    int thread_index;
    int burst_index;
    int length_ms;
    int inter_arrival_time;
} burst_struct;

struct queue_node
{
    burst_struct burst;
    struct queue_node* next;
};
typedef struct queue_node queue_node;

// Global parameters 
int N = 0;
int algorithm_type = ALGO_INVALID;
char infile[256];

int generate_randomly;
int minB;
int avgB;
int minA;
int avgA;

// Runqueue
int vruntime[N_MAX];
int last_burst_index[N_MAX];

pthread_mutex_t rq_mutex;

queue_node* root_node;
size_t rq_size = 0;

void print_burst(burst_struct* burst)
{
    // printf("(T=%d, B=%d, L=%d, A=%d) ", burst->thread_index, burst->burst_index, burst->length_ms, burst->inter_arrival_time);

    printf("(T=%d, L=%d, A=%d) ", burst->thread_index, burst->length_ms, burst->inter_arrival_time);
}

void print_queue(int index)
{
    int pass = 0;

    printf("[consume index %d]\n", index);

    if (!pass)
    {
        if (root_node == NULL)
        {
            printf("null");
        }
        else 
        {
            queue_node* current_node = root_node;

            while (current_node != NULL)
            {
                
                print_burst(&current_node->burst);
                current_node = current_node->next;
            }
        }
    }

    printf("\n");
}

int select_burst_by_algorithm()
{
    
    int result = 0;

    if (algorithm_type == ALGO_FCFS)
    {
        result = rq_size - 1; // actuaklly this is not correct change this
    }
    else if (algorithm_type == ALGO_SJF)
    {
        queue_node* current_node = root_node;
        int current_index = 0;
        int shortest_index = 0;
        int shortest_time = 10000000;

        while (current_node != NULL)
        {
            int real_thread_index = current_node->burst.thread_index - 1;
            int current_last_burst_index = last_burst_index[real_thread_index];
            int current_burst_index = current_node->burst.burst_index;

            if (current_burst_index == current_last_burst_index)
            {
                if (shortest_time > current_node->burst.length_ms)
                {
                    shortest_index = current_index;
                    shortest_time = current_node->burst.length_ms;
                }
            }

            current_node = current_node->next;
            current_index += 1;
        }

        result = shortest_index;
    }
    else if (algorithm_type == ALGO_PRIO)
    {
        queue_node* current_node = root_node;
        int current_index = 0;
        int priori_index = 0;
        int priori_max = 10000000;

        while (current_node != NULL)
        {
            int real_thread_index = current_node->burst.thread_index - 1;
            int current_last_burst_index = last_burst_index[real_thread_index];
            int current_burst_index = current_node->burst.burst_index;

            if (current_burst_index == current_last_burst_index)
            {
                if (priori_max > current_node->burst.thread_index)
                {
                    priori_index = current_index;
                    priori_max = current_node->burst.thread_index;
                }
            }

            current_node = current_node->next;
            current_index += 1;
        }

        result = priori_index;
    }

    // printf("consuming %d\n", result);
    return result;
}

void register_statistics(burst_struct* burst)
{

}

int random_exp_dist(int minimum, int mean)
{
    int result = 0;

    do
    {
        // Make this a exponential distribution
        double temp = rand() / (RAND_MAX + 1.0);
        result = (int) (-log(1 - temp) * mean);

    } while (result < minimum);
    
    return result;
}

void custom_sleep(int sleep_ms)
{
    usleep(1000 * sleep_ms);
}

int insert(burst_struct* burst)
{
    if (root_node == NULL)
    {
        root_node = (queue_node*) malloc(sizeof(queue_node));
        root_node->burst = *burst;
        root_node->next = NULL;
    } 
    else 
    {
        queue_node* current = root_node;

        while (current->next != NULL)
            current = current->next;
        
        current->next = (queue_node*) malloc(sizeof(queue_node));
        current->next->burst = *burst;
        current->next->next = NULL;
    }

    rq_size += 1;
}

int consume(burst_struct* burst, int index) // This consume method is non blocking
{
    if (root_node == NULL)
    {
        return 0;
    } 
    else 
    {
        if (index == 0)
        {
            print_queue(index); // Print queue just before consumption

            queue_node* temp = root_node;
            root_node = root_node->next;

            *burst = temp->burst;

            free(temp);
        }
        else 
        {
            if (index >= rq_size)
            {
                return 0;
            }

            print_queue(index); // Print queue just before consumption

            queue_node* previous_node = root_node;

            for (size_t i = 1; i < index; i++)
                previous_node = previous_node->next;
            
            queue_node* selected_node = previous_node->next;

            previous_node->next = selected_node->next;

            *burst = selected_node->burst;

            free(selected_node);
        }
    }
    rq_size -= 1;
    
    

    return 1;
}

void* worker_thread(void* args)
{
    int thread_index = *((int*) args);

    for (size_t burst_index = 0; burst_index < NO_OF_BURSTS; burst_index++)
    {
        burst_struct burst;

        if (generate_randomly) // Generate random bursts
        {
            // Randomize the burst
            burst.burst_index = burst_index;
            burst.thread_index = thread_index;
            burst.inter_arrival_time = random_exp_dist(minA, avgA);
            burst.length_ms = random_exp_dist(minB, avgB);
        }
        else // Read from file
        {
            // TODO
        }

        pthread_mutex_lock(&rq_mutex);
        insert(&burst);
        pthread_mutex_unlock(&rq_mutex);

        custom_sleep(burst.length_ms);
        custom_sleep(burst.inter_arrival_time);
    }

    return NULL;
}

void* server_thread(void* args) 
{
    custom_sleep(10 * NO_OF_BURSTS * N);
    
    for (size_t i = 0; i < NO_OF_BURSTS * N; i++)
    {
        burst_struct burst;
        int success;
        
        // If rq is empty then do a busy wait
        do
        {
            pthread_mutex_lock(&rq_mutex);

            // calculate which burst to consume here!
            int consume_index = select_burst_by_algorithm();

            success = consume(&burst, consume_index);

            if (success == 1)
            {

                // increment the lasly changed burst index
                last_burst_index[burst.thread_index] += 1;

                // register statistics while mutex is locked
                register_statistics(&burst);
            }

            pthread_mutex_unlock(&rq_mutex);

            custom_sleep(10);
        } while (success == 0);

        // Print the consumed burst
        printf("Consumed: "); print_burst(&burst); printf("\n\n");

        // Simulate execution by sleeping
        custom_sleep(burst.length_ms);
    }

    printf("Server is finshed\n");
}

void parse_parameters(int argc, char const *argv[])
{
    // Get the arguments from the argv
    // First look for -f option
    size_t read_from_file_argument_index = -1;
    size_t algorithm_index = -1;

    for (size_t i = 0; i < argc; i++)
    {
        if (strcmp("-f", argv[i]) == 0)
            read_from_file_argument_index = i;
        
        if (strcmp("FCFS", argv[i]) == 0)
        {
            algorithm_index = i;
            algorithm_type = ALGO_FCFS;
        }
        else if (strcmp("SJF", argv[i]) == 0)
        {
            algorithm_index = i;
            algorithm_type = ALGO_SJF;
        }
        else if (strcmp("PRIO", argv[i]) == 0)
        {
            algorithm_index = i;
            algorithm_type = ALGO_PRIO;
        }
        else if (strcmp("VRUNTIME", argv[i]) == 0)
        {
            algorithm_index = i;
            algorithm_type = ALGO_VRUNTIME;
        }
    }
  
    // Error check 1
    if (read_from_file_argument_index != -1 && argc == read_from_file_argument_index + 1)
    {
        printf("\tInvalid arguments: -f requires a file name.\n");
        exit(-1);
    }
    // Error check 2
    if (algorithm_type == ALGO_INVALID)
    {
        printf("\tInvalid arguments: algorithm type is invalid or not given.\n");
        exit(-1);
    }

    // Get the the parameters from arguments
    if (read_from_file_argument_index == -1)
    {
        if (argc < 7)
        {
            printf("\tInvalid arguments: not enough parameters.\n");
            exit(-1);
        }

        N = atoi(argv[1]);

        minB = atoi(argv[2]);
        avgB = atoi(argv[3]);
        minA = atoi(argv[4]);
        avgA = atoi(argv[5]);

        generate_randomly = 1;
    }
    else 
    {
        if (argc != 5)
        {
            printf("\tInvalid arguments: too many parameters.\n");
            exit(-1);
        }

        // Get the number of processors from the arguments
        N = atoi(argv[1]);
        generate_randomly = 0;
        strcpy(infile, argv[read_from_file_argument_index + 1]);
    }

    if (N <= 0 || N > 10)
    {
        printf("\tInvalid arguments: N is not given in range [1, 10].\n");
        exit(-1);
    }

    // Fill the variables to their default
    for (size_t i = 0; i < N_MAX; i++)
    {
        last_burst_index[i] = 0;
        vruntime[i] = 0;
    }
    root_node = NULL;
    

    printf("Program will run for N=%d threads using algorithm '%s'.\n", N, argv[algorithm_index]);

    if (generate_randomly)
        printf("Burst generation will be randomized with parameters (%d, %d, %d, %d).\n", minB, avgB, minA, avgA);
    else 
        printf("Burst generation will be from file '%s'.\n", infile);
}

int main(int argc, char const *argv[])
{
    // Parse the parameters and write them to global space
    parse_parameters(argc, argv);

    // Init mutex and queue
    pthread_mutex_init(&rq_mutex, NULL);

    // Worker threads
    pthread_t worker_tids[N];
    int thread_indexes[N];

    // Server thread (scheduler thread)
    pthread_t server_tid;

    for (size_t i = 0; i < N; i++)
    {
        // Set the thread indexe
        thread_indexes[i] = i + 1;

        // Create the worker thread
        pthread_create(&worker_tids[i], NULL, worker_thread, (void*) &thread_indexes[i]);
    }
    
    pthread_create(&server_tid, NULL, server_thread, NULL);

    // Wait for all threads to finish
    for (size_t i = 0; i < N; i++)
        pthread_join(worker_tids[i], NULL);
    
    pthread_join(server_tid, NULL);
    

    // Destroy ready queue
    pthread_mutex_destroy(&rq_mutex);

    printf("Simulation finished.\n");
    return 0;
}
