## Assignment 2 — Race Condition

### 1. Race condition observed

Two equal-priority threads each incremented a shared counter 1000 times.

The first version did not use a mutex.

QEMU output:

SeaBIOS (version rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org)
Booting from ROM..
*** Booting Zephyr OS build v4.4.0 ***
[00:00:00.150,000] <inf> demo: Thread A finished
[00:00:00.150,000] <inf> demo: Thread B finished
[00:00:00.150,000] <inf> demo: Final counter: 1000
[00:00:00.150,000] <inf> demo: Expected counter: 2000

The expected value was 2000, but the actual value was 1000.
This demonstrates a race condition: both threads can read the same
counter value before either thread writes the incremented value,
causing increments to be lost.

main.c :
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5
#define NUM_ITERATIONS 1000

volatile int shared_counter = 0;

void thread_a_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int temp = shared_counter;

        k_yield();

        shared_counter = temp + 1;
    }

    LOG_INF("Thread A finished");
}

void thread_b_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int temp = shared_counter;

        k_yield();

        shared_counter = temp + 1;
    }

    LOG_INF("Thread B finished");
}

K_THREAD_DEFINE(thread_a, STACK_SIZE, thread_a_fn,
                NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(thread_b, STACK_SIZE, thread_b_fn,
                NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);

int main(void)
{
    k_thread_join(&thread_a, K_FOREVER);
    k_thread_join(&thread_b, K_FOREVER);

    LOG_INF("Final counter: %d", shared_counter);
    LOG_INF("Expected counter: %d", NUM_ITERATIONS * 2);

    return 0;
}

### 2. Fix using mutex

The shared counter was protected using a Zephyr mutex.

Each thread locks the mutex before reading/modifying/writing the
counter and unlocks it afterward.

After applying the mutex, the result was:

SeaBIOS (version rel-1.16.3-0-ga6ed6b701f0a-prebuilt.qemu.org)
Booting from ROM..
*** Booting Zephyr OS build v4.4.0 ***
[00:00:00.150,000] <inf> demo: Thread A finished
[00:00:00.150,000] <inf> demo: Thread B finished
[00:00:00.150,000] <inf> demo: Final counter: 2000
[00:00:00.150,000] <inf> demo: Expected counter: 2000

The mutex prevents both threads from entering the critical section
at the same time, so no increments are lost.