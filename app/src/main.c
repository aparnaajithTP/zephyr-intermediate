#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define BURST_COUNT 5
#define BURST_INTERVAL_MS 5
#define DEBOUNCE_MS 30

static int total_events;
static int total_processed;

static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;

    LOG_INF("[HANDLER] processed event %d tick=%u",
            total_processed, k_uptime_get_32());
}

K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < BURST_COUNT; i++) {
        k_msleep(BURST_INTERVAL_MS);

        total_events++;

        LOG_INF("[SENSOR] burst event %d tick=%u",
                i, k_uptime_get_32());

        int ret = k_work_reschedule(&debounce_work,
                                    K_MSEC(DEBOUNCE_MS));

        if (ret < 0) {
            LOG_ERR("reschedule failed: %d", ret);
        }
    }

    LOG_INF("[SENSOR] burst complete");
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_sim_fn,
                NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    LOG_INF("=== L3 Homework: Workqueue Debounce Bonus ===");
    LOG_INF("5 events within 20ms, debounce delay = 30ms");

    /* Wait for burst and delayed handler */
    k_msleep(200);

    LOG_INF("[SUMMARY] events=%d handler_calls=%d",
            total_events, total_processed);

    return 0;
}