#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#define N 1024
#define NUM_THREADS 4

// Structure to pass arguments to threads
typedef struct {
    const uint64_t *array;
    size_t n;
    int thread_id;
    int num_threads;
    uint64_t partial_sum;
} thread_data_t;

// Strategy i: Strided access (Thread i takes index i + 4k)
void* sum_strategy_1(void *arg) {
    thread_data_t *data = (thread_data_t*) arg;
    uint64_t sum = 0;
    
    for (size_t k = 0; ; k++) {
        size_t idx = data->thread_id + (4 * k);
        if (idx >= data->n) break;
        sum += data->array[idx];
    }
    
    data->partial_sum = sum;
    pthread_exit(NULL);
}

// Strategy ii: Contiguous block access (Thread i takes its own chunk)
void* sum_strategy_2(void *arg) {
    thread_data_t *data = (thread_data_t*) arg;
    uint64_t sum = 0;
    
    size_t chunk_size = data->n / data->num_threads;
    size_t start = data->thread_id * chunk_size;
    size_t end = start + chunk_size;
    
    for (size_t i = start; i < end; i++) {
        sum += data->array[i];
    }
    
    data->partial_sum = sum;
    pthread_exit(NULL);
}

// Single-threaded baseline sum
uint64_t sum_single_threaded(const uint64_t *array, size_t n) {
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += array[i];
    }
    return sum;
}

// Helper function to get time in nanoseconds
long long get_elapsed_time(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000000000LL + (end->tv_nsec - start->tv_nsec);
}

int main() {
    uint64_t array[N];
    
    // Seed and populate array with random 64-bit integers
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < N; i++) {
        // Combine two rand() calls to make a 64-bit integer
        array[i] = ((uint64_t)rand() << 32) | rand();
    }

    struct timespec start, end;
    uint64_t total_sum = 0;

    // --- 1. Single-Threaded Execution ---
    clock_gettime(CLOCK_MONOTONIC, &start);
    total_sum = sum_single_threaded(array, N);
    clock_gettime(CLOCK_MONOTONIC, &end);
    long long time_single = get_elapsed_time(&start, &end);
    printf("Single-Threaded Sum: %llu | Time: %lld ns\n", (unsigned long long)total_sum, time_single);

    // --- 2. Multi-Threaded Strategy i (Strided) ---
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i] = (thread_data_t){array, N, i, NUM_THREADS, 0};
        pthread_create(&threads[i], NULL, sum_strategy_1, &thread_data[i]);
    }
    
    uint64_t sum_strat1 = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        sum_strat1 += thread_data[i].partial_sum;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long long time_strat1 = get_elapsed_time(&start, &end);
    printf("Strategy i (Strided) Sum: %llu | Time: %lld ns\n", (unsigned long long)sum_strat1, time_strat1);

    // --- 3. Multi-Threaded Strategy ii (Contiguous Blocks) ---
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i] = (thread_data_t){array, N, i, NUM_THREADS, 0};
        pthread_create(&threads[i], NULL, sum_strategy_2, &thread_data[i]);
    }
    
    uint64_t sum_strat2 = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        sum_strat2 += thread_data[i].partial_sum;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long long time_strat2 = get_elapsed_time(&start, &end);
    printf("Strategy ii (Block) Sum: %llu | Time: %lld ns\n", (unsigned long long)sum_strat2, time_strat2);

    return 0;
}