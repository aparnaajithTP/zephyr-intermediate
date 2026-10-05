#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(assignment5, LOG_LEVEL_INF);

#define STACK_SIZE              1024

#define QUEUE_DEPTH             8
#define SAMPLE_COUNT            30

#define PRODUCER_PERIOD_MS      50
#define CONSUMER_NORMAL_MS      80

#define HEALTH_PERIOD_MS        100
#define HEALTH_WARN_PERCENT     75

#define WATCHDOG_TIMEOUT_MS     300
#define STUCK_SLEEP_MS          1000

struct sensor_msg {
    uint32_t timestamp_ms;
    uint32_t seq;
    int32_t value;
};

K_MSGQ_DEFINE(sensor_queue,
              sizeof(struct sensor_msg),
              QUEUE_DEPTH,
              4);

static int watchdog_channel = -1;
static bool consumer_stuck;

/*
 * Used so the consumer does not start feeding the watchdog
 * before main() has initialized the task watchdog.
 */
K_SEM_DEFINE(start_sem, 0, 1);

/* ------------------------------------------------------------------ */
/* Watchdog callback                                                   */
/* ------------------------------------------------------------------ */

static void watchdog_callback(int channel_id, void *user_data)
{
    ARG_UNUSED(user_data);

    LOG_ERR("[WATCHDOG] CALLBACK TRIGGERED!");
    LOG_ERR("[WATCHDOG] consumer watchdog channel=%d expired",
            channel_id);

    LOG_ERR("[WATCHDOG] consumer appears stuck");
}

/* ------------------------------------------------------------------ */
/* Producer                                                            */
/* ------------------------------------------------------------------ */

static void producer_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "producer");

    for (uint32_t seq = 0; seq < SAMPLE_COUNT; seq++) {

        struct sensor_msg msg = {
            .timestamp_ms = k_uptime_get_32(),
            .seq = seq,
            .value = 100 + seq,
        };

        int ret = k_msgq_put(&sensor_queue,
                             &msg,
                             K_MSEC(100));

        if (ret == 0) {
            uint32_t used = k_msgq_num_used_get(&sensor_queue);

            LOG_INF("[PRODUCER] seq=%u q=%u/%u",
                    msg.seq,
                    used,
                    QUEUE_DEPTH);
        } else {
            LOG_WRN("[PRODUCER] queue full - dropped seq=%u",
                    msg.seq);
        }

        k_msleep(PRODUCER_PERIOD_MS);
    }

    LOG_INF("[PRODUCER] done");
}

/* ------------------------------------------------------------------ */
/* Consumer                                                            */
/* ------------------------------------------------------------------ */

static void consumer_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "consumer");

    /*
     * Wait until main has initialized the watchdog.
     */
    k_sem_take(&start_sem, K_FOREVER);

    watchdog_channel = task_wdt_add(
        WATCHDOG_TIMEOUT_MS,
        watchdog_callback,
        NULL);

    if (watchdog_channel < 0) {
        LOG_ERR("[CONSUMER] failed to create watchdog channel: %d",
                watchdog_channel);
        return;
    }

    LOG_INF("[CONSUMER] watchdog channel=%d timeout=%dms",
            watchdog_channel,
            WATCHDOG_TIMEOUT_MS);

    for (uint32_t i = 0; i < SAMPLE_COUNT; i++) {

        struct sensor_msg msg;

        int ret = k_msgq_get(&sensor_queue,
                             &msg,
                             K_MSEC(500));

        if (ret != 0) {
            LOG_WRN("[CONSUMER] queue receive timeout");
            task_wdt_feed(watchdog_channel);
            continue;
        }

        uint32_t used = k_msgq_num_used_get(&sensor_queue);

        LOG_INF("[CONSUMER] seq=%u q=%u/%u",
                msg.seq,
                used,
                QUEUE_DEPTH);

        /*
         * Deliberately simulate a stuck consumer once.
         */
        if (i == 4 && !consumer_stuck) {

            consumer_stuck = true;

            LOG_WRN("[CONSUMER] *** SIMULATING STUCK CONSUMER ***");
            LOG_WRN("[CONSUMER] sleeping for %dms",
                    STUCK_SLEEP_MS);
            LOG_WRN("[CONSUMER] watchdog timeout is only %dms",
                    WATCHDOG_TIMEOUT_MS);

            /*
             * Deliberately do NOT feed the watchdog here.
             */
            k_msleep(STUCK_SLEEP_MS);

            LOG_WRN("[CONSUMER] recovered from simulated stall");
        }

        /*
         * Normal operation.
         */
        if (watchdog_channel >= 0) {
            ret = task_wdt_feed(watchdog_channel);

            if (ret != 0) {
                LOG_WRN("[CONSUMER] watchdog feed failed: %d",
                        ret);
            }
        }

        k_msleep(CONSUMER_NORMAL_MS);
    }

    LOG_INF("[CONSUMER] done");

    if (watchdog_channel >= 0) {
        task_wdt_delete(watchdog_channel);
    }
}

/* ------------------------------------------------------------------ */
/* Health monitor                                                      */
/* ------------------------------------------------------------------ */

static void health_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "health");

    for (int i = 0; i < 25; i++) {

        k_msleep(HEALTH_PERIOD_MS);

        uint32_t used = k_msgq_num_used_get(&sensor_queue);

        uint32_t percent =
            (used * 100U) / QUEUE_DEPTH;

        LOG_INF("[HEALTH] queue=%u/%u (%u%%)",
                used,
                QUEUE_DEPTH,
                percent);

        if (percent >= HEALTH_WARN_PERCENT) {

            LOG_WRN("[HEALTH] WARNING: queue at %u%% capacity",
                    percent);
        }
    }

    LOG_INF("[HEALTH] monitoring complete");
}

/* ------------------------------------------------------------------ */
/* Thread definitions                                                  */
/* ------------------------------------------------------------------ */

K_THREAD_DEFINE(producer_thread,
                STACK_SIZE,
                producer_fn,
                NULL, NULL, NULL,
                5,
                0,
                0);

K_THREAD_DEFINE(consumer_thread,
                STACK_SIZE,
                consumer_fn,
                NULL, NULL, NULL,
                5,
                0,
                0);

K_THREAD_DEFINE(health_thread,
                STACK_SIZE,
                health_fn,
                NULL, NULL, NULL,
                6,
                0,
                0);

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    LOG_INF("=== L5 Assignment: Reliability Under Pressure ===");

    LOG_INF("queue depth=%d", QUEUE_DEPTH);
    LOG_INF("producer period=%dms", PRODUCER_PERIOD_MS);
    LOG_INF("consumer normal delay=%dms", CONSUMER_NORMAL_MS);
    LOG_INF("watchdog timeout=%dms", WATCHDOG_TIMEOUT_MS);
    LOG_INF("stuck consumer sleep=%dms", STUCK_SLEEP_MS);
    LOG_INF("health warning threshold=%d%%",
            HEALTH_WARN_PERCENT);

    /*
     * Initialize the task watchdog before releasing
     * the consumer.
     */
    int ret = task_wdt_init(NULL);

    if (ret != 0) {
        LOG_ERR("[WATCHDOG] init failed: %d", ret);
        return 0;
    }

    LOG_INF("[WATCHDOG] task watchdog initialized");

    /*
     * Allow consumer to create its watchdog channel.
     */
    k_sem_give(&start_sem);

    /*
     * Allow enough time for:
     * - producer to finish
     * - consumer to hit the intentional stall
     * - health monitor to report queue buildup
     */
    k_msleep(5000);

    LOG_INF("=== L5 Assignment complete ===");

    return 0;
}