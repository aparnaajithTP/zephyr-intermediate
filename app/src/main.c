#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5
#define NUM_ITERATIONS 1000

volatile int shared_counter = 0;

K_MUTEX_DEFINE(counter_mutex); //defining mutex

void thread_a_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        int temp = shared_counter;
        k_yield();
        shared_counter = temp + 1;

        k_mutex_unlock(&counter_mutex);
    }

    LOG_INF("Thread A finished");
}

void thread_b_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        int temp = shared_counter;
        k_yield();
        shared_counter = temp + 1;

        k_mutex_unlock(&counter_mutex);
    }

    LOG_INF("Thread B finished");
}

K_THREAD_DEFINE(thread_a, STACK_SIZE, thread_a_fn,
                NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(thread_b, STACK_SIZE, thread_b_fn,
                NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);

int main(void)
{
    k_thread_join(thread_a, K_FOREVER);
    k_thread_join(thread_b, K_FOREVER);

    LOG_INF("Final counter: %d", shared_counter);
    LOG_INF("Expected counter: %d", NUM_ITERATIONS * 2);

    return 0;
}