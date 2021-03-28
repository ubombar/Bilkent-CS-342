#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ALGO_INVALID -1
#define ALGO_FCFS 0
#define ALGO_SJF 1
#define ALGO_PRIO 2
#define ALGO_VRUNTIME 3

#define MAX_NUMBER 294967294

#define DEBUG 0
#define PRINT_QUEUE 1

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
int burst_count;
int minB;
int avgB;
int minA;
int avgA;

// Other 
pthread_t worker_tids[N_MAX];
int thread_indexes[N_MAX];
int wait_times[N_MAX];

// Runqueue
int vruntime[N_MAX];
int last_burst_index[N_MAX];

int num_burst_executed  = 0;
burst_struct selected_burst;
int num_threads_consumed_exec = 0;

pthread_mutex_t rq_mutex;
pthread_mutex_t active_mutex;
pthread_mutex_t exec_mutex;
int num_finished;

queue_node* root_node;
size_t rq_size = 0;

int* burst_length_statistics;
int burst_length_statistics_last_index;

long get_time_ms()
{
    struct timeval current_timeval;
    gettimeofday(&current_timeval, NULL);

    return current_timeval.tv_sec * (long) 1000 + (long) (current_timeval.tv_usec / 1000);
}

void print_burst(burst_struct* burst)
{
    printf("(T=%d, B=%d, L=%d, A=%d) ", burst->thread_index, burst->burst_index, burst->length_ms, burst->inter_arrival_time);

    // printf("(T=%d, L=%d, A=%d) ", burst->thread_index, burst->length_ms, burst->inter_arrival_time);
}

void print_queue(int index)
{
    if (PRINT_QUEUE)
    {
        printf("[ ");
        for (size_t i = 0; i < N; i++)
        {
            printf("%d ", last_burst_index[i]);
        }
        printf("]\n");
        
        // printf("num fin=%d\n", num_finished);

        if (algorithm_type == ALGO_VRUNTIME)
        {
            for (size_t thread_index = 1; thread_index <= N; thread_index++)
                printf("[T%ld=%d]", thread_index, vruntime[thread_index - 1]);
            
            printf("\n");
        }

        
        printf("Queue: [consume %d]: ", index);

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

        printf("\n");
    }

    
}

int select_burst_by_algorithm()
{
    if (algorithm_type == ALGO_FCFS)
    {
        queue_node* current_node = root_node;
        int current_index = 0;
        int first_come_index = 0;

        while (current_node != NULL)
        {
            int real_thread_index = current_node->burst.thread_index - 1;
            int current_last_burst_index = last_burst_index[real_thread_index];
            int current_burst_index = current_node->burst.burst_index;

            if (current_burst_index == current_last_burst_index)
            {
                first_come_index = current_index;
            }

            current_node = current_node->next;
            current_index += 1;
        }

        return first_come_index;
    }
    else if (algorithm_type == ALGO_SJF)
    {
        queue_node* current_node = root_node;
        int current_index = 0;
        int shortest_index = 0;
        int shortest_time = MAX_NUMBER;

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

        return shortest_index;
    }
    else if (algorithm_type == ALGO_PRIO)
    {
        queue_node* current_node = root_node;
        int current_index = 0;
        int priori_index = 0;
        int priori_min = MAX_NUMBER;

        while (current_node != NULL)
        {
            int real_thread_index = current_node->burst.thread_index - 1;
            int current_last_burst_index = last_burst_index[real_thread_index];
            int current_burst_index = current_node->burst.burst_index;

            if (current_burst_index == current_last_burst_index)
            {
                if (priori_min > current_node->burst.thread_index)
                {
                    priori_index = current_index;
                    priori_min = current_node->burst.thread_index;
                }
            }

            current_node = current_node->next;
            current_index += 1;
        }

        return priori_index;
    }
    else if (algorithm_type == ALGO_VRUNTIME)
    {
        // Warning increment the vruntime of a thred when consuming it in the server

        queue_node* current_node = root_node;
        int current_index = 0;
        int smallest_vruntime = MAX_NUMBER;
        int smallest_vruntime_index = 0;

        while (current_node != NULL)
        {
            int real_thread_index = current_node->burst.thread_index - 1;
            int current_last_burst_index = last_burst_index[real_thread_index];
            int current_burst_index = current_node->burst.burst_index;

            if (current_burst_index == current_last_burst_index)
            {
                int current_vruntime = vruntime[real_thread_index];

                if (smallest_vruntime > current_vruntime)
                {
                    smallest_vruntime_index = current_index;
                    smallest_vruntime = current_vruntime;
                }
            }

            current_node = current_node->next;
            current_index += 1;
        }

        return smallest_vruntime_index;
    }
    else 
    {
        // If the there is no algorithm then this scope will be invoked
        printf("\tInternal error: No such algorithm! Removing first occurance.");
        return 0;
    }
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

    burst_length_statistics[burst_length_statistics_last_index++] = result;
    
    return result;
}

void custom_sleep(int sleep_ms)
{
    usleep(1000 * sleep_ms);
}

int insert(burst_struct* burst)
{
    // Insert at the begining
    if (root_node == NULL)
    {
        root_node = (queue_node*) malloc(sizeof(queue_node));
        root_node->burst = *burst;
        root_node->next = NULL;
    } 
    else 
    {
        queue_node* old_root_node = root_node;
        
        root_node = (queue_node*) malloc(sizeof(queue_node));
        root_node->burst = *burst;
        root_node->next = old_root_node;
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
    int current_executed_burst = 0;

    if (generate_randomly == 0) // Read from file
    {
        char local_filename[512];
        size_t read;
        char* line;
        size_t len;
        int burst_index = 0;

        sprintf(local_filename, "./%s-%d.txt", infile, thread_index);

        // Create if does not exists
        FILE* fp = fopen(local_filename, "ab");
        fclose(fp);

        // Open it to read
        fp = fopen(local_filename, "rb");

        if (fp == NULL)
        {
            printf("\tInternal Error: Cannot open/create file '%s'\n", local_filename);
            exit(-1);
        }

        // Read from the file
        while ((read = getline(&line, &len, fp)) != -1) 
        {
            char* token1 = strtok(line, " ");
            char* token2 = strtok(NULL, " ");
            
            burst_struct burst;

            // Randomize the burst
            burst.burst_index = burst_index;
            burst.thread_index = thread_index;
            burst.inter_arrival_time = atoi(token1);
            burst.length_ms = atoi(token2);

            pthread_mutex_lock(&rq_mutex);
            insert(&burst);
            pthread_mutex_unlock(&rq_mutex);

            // Get before time
            long before_time = get_time_ms();
            long after_time;

            do 
            {
                pthread_mutex_lock(&exec_mutex);

                // Calculate the after time
                after_time = get_time_ms();

                // Check if the burst is increased
                if (burst.thread_index == thread_index && burst.burst_index == burst_index && current_executed_burst == num_burst_executed - 1)
                {
                    // If last burst index is increased we know thread is slept enough
                    // First update the current executed burst
                    current_executed_burst += 1;

                    // Then increment the consumed burst count to notify the server
                    num_threads_consumed_exec += 1;
                    
                    long wait_time_ms = after_time - before_time;

                    
                    // If thread has waited more than it should
                    if (wait_time_ms > burst.inter_arrival_time + burst.length_ms)
                    {
                        // register the wait time here!
                        wait_times[thread_index - 1] += 1;
                    }
                    else 
                    {
                        long thread_sleep_time_until_new_burst = burst.inter_arrival_time - burst.inter_arrival_time;
                        custom_sleep(thread_sleep_time_until_new_burst);
                    }

                    // Unlock the threads then exit
                    pthread_mutex_unlock(&exec_mutex);

                    break;
                }
                
                pthread_mutex_unlock(&exec_mutex);
                custom_sleep(1);
            } while (1);    

            burst_index++;
        }

        fclose(fp);
    }
    else // Generate randomly
    {
        for (size_t burst_index = 0; burst_index < burst_count; burst_index++)
        {
            burst_struct burst;

            // Randomize the burst
            burst.burst_index = burst_index;
            burst.thread_index = thread_index;
            burst.inter_arrival_time = random_exp_dist(minA, avgA);
            burst.length_ms = random_exp_dist(minB, avgB);

            pthread_mutex_lock(&rq_mutex);
            insert(&burst);
            pthread_mutex_unlock(&rq_mutex);

            // Get before time
            long before_time = get_time_ms();
            long after_time;

            do 
            {
                pthread_mutex_lock(&exec_mutex);

                // Calculate the after time
                after_time = get_time_ms();

                // Check if the burst is increased
                if (burst.thread_index == thread_index && burst.burst_index == burst_index && current_executed_burst == num_burst_executed - 1)
                {
                    // If last burst index is increased we know thread is slept enough
                    // First update the current executed burst
                    current_executed_burst += 1;

                    // Then increment the consumed burst count to notify the server
                    num_threads_consumed_exec += 1;
                    
                    long wait_time_ms = after_time - before_time;

                    
                    // If thread has waited more than it should
                    if (wait_time_ms > burst.inter_arrival_time + burst.length_ms)
                    {
                        // register the wait time here!
                        // printf("Wait time = %ld\n", wait_time_ms);
                        wait_times[thread_index - 1] += 1;
                    }
                    else 
                    {
                        long thread_sleep_time_until_new_burst = burst.inter_arrival_time - burst.inter_arrival_time;
                        custom_sleep(thread_sleep_time_until_new_burst);
                    }

                    // Unlock the threads then exit
                    pthread_mutex_unlock(&exec_mutex);

                    break;
                }
                
                pthread_mutex_unlock(&exec_mutex);
                custom_sleep(1);
            } while (1);            
        }
    }

    pthread_mutex_lock(&active_mutex);
    num_finished += 1;
    pthread_mutex_unlock(&active_mutex);

    return NULL;
}

void* server_thread(void* args) 
{
    int all_done = 0;
    size_t i = 0;
    int schutdown_scheduler = 0;

    while (!schutdown_scheduler)
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
                last_burst_index[burst.thread_index - 1] += 1;

                // register statistics while mutex is locked
                register_statistics(&burst);
            }

            pthread_mutex_unlock(&rq_mutex);

            custom_sleep(10);

            pthread_mutex_lock(&active_mutex);
            schutdown_scheduler = (N == num_finished) && (root_node == NULL);
            pthread_mutex_unlock(&active_mutex);

        } while (success == 0 && !schutdown_scheduler);
        
        
        if (PRINT_QUEUE)
        {
            // Print the consumed burst
            printf("Consumed: "); 
            print_burst(&burst); 
            printf("\n\n");
        }

        // Increment the vruntime while consuming the burst
        if (algorithm_type == ALGO_VRUNTIME)
        {
            int real_thread_index = burst.thread_index - 1;

            vruntime[real_thread_index] += (int) ((double) burst.length_ms) * (0.7 + 0.3 * burst.thread_index);
        }

        pthread_mutex_lock(&exec_mutex);
        pthread_mutex_lock(&active_mutex);

        num_burst_executed += 1;
        selected_burst = burst;

        // Simulate execution by sleeping, this also halts other threads
        custom_sleep(burst.length_ms);

        // Wait for other threads to consume new information
        do 
        {
            // printf("num_threads_consumed=%d N=%d num_finished=%d\n", num_threads_consumed_exec, N, num_finished);

            if (num_threads_consumed_exec == N - num_finished || N == num_finished)
            {
                pthread_mutex_unlock(&exec_mutex);
                pthread_mutex_unlock(&active_mutex);
                break;
            }

            pthread_mutex_unlock(&exec_mutex);
            pthread_mutex_unlock(&active_mutex);
            custom_sleep(1);
            pthread_mutex_lock(&exec_mutex);
            pthread_mutex_lock(&active_mutex);
        } while (1);

        
        

        // After this point we are certain all threads are notified about the latest
        pthread_mutex_lock(&exec_mutex);
        pthread_mutex_lock(&active_mutex);
        num_threads_consumed_exec = 0;
        pthread_mutex_unlock(&exec_mutex);
        pthread_mutex_unlock(&active_mutex);

        // Do not print in case of shutdown
        if (schutdown_scheduler)
            break;
        i++;
    }

    // Wait for all w threads to finish
    for (size_t i = 0; i < N; i++)
        pthread_join(worker_tids[i], NULL);
    
    return NULL;
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

        burst_count = atoi(argv[2]);
        minB = atoi(argv[3]);
        avgB = atoi(argv[4]);
        minA = atoi(argv[5]);
        avgA = atoi(argv[6]);

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
        wait_times[i] = 0;
    }
    root_node = NULL;
    num_finished = 0;
    

    printf("Program will run for N=%d threads using algorithm '%s'.\n", N, argv[algorithm_index]);

    if (generate_randomly)
        printf("Burst generation will be randomized with parameters (%d, %d, %d, %d).\n\n", minB, avgB, minA, avgA);
    else 
        printf("Burst generation will be from file '%s'.\n\n", infile);
}

int main(int argc, char const *argv[])
{
    // Parse the parameters and write them to global space
    parse_parameters(argc, argv);

    int burst_lengths[N * 1024];

    burst_length_statistics = burst_lengths;
    burst_length_statistics_last_index = 0;
    
    // Get Before time
    long before_time = get_time_ms();

    // Init mutex and queue
    pthread_mutex_init(&rq_mutex, NULL);
    pthread_mutex_init(&active_mutex, NULL);
    pthread_mutex_init(&exec_mutex, NULL);

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
    
    pthread_join(server_tid, NULL);
    

    // Destroy ready queue
    pthread_mutex_destroy(&rq_mutex);
    pthread_mutex_destroy(&active_mutex);
    pthread_mutex_destroy(&exec_mutex);

    printf("\nSimulation finished in %ld ms.\n", get_time_ms() - before_time);

    printf("Waiting times of threads:\n");
    
    for (int i = 0; i < N; i++)
    {
        printf("Thread %d waited %d ms\n", i + 1, wait_times[i]);
    }

    if (DEBUG == 1)
    {
        printf("Burst times (ms):\n");

        for (int i = 0; i < burst_length_statistics_last_index; i++)
        {
            printf("%d\n", burst_lengths[i]);
        }
    }
    
    return 0;
}
