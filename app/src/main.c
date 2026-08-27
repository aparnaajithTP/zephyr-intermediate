#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define SENSOR_MS 100
#define EVENT_COUNT 10

static int total_events;
static int total_processed;

/* Work handler */
static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;

    LOG_INF("[HANDLER] processed event %d  tick=%u",
            total_processed, k_uptime_get_32());
}

K_WORK_DEFINE(sensor_work, sensor_handler);

/* Sensor simulation thread */
static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < EVENT_COUNT; i++) {
        k_msleep(SENSOR_MS);

        total_events++;

        LOG_INF("[SENSOR] event %d  tick=%u",
                i, k_uptime_get_32());

        int ret = k_work_submit(&sensor_work);

        if (ret < 0) {
            LOG_ERR("submit failed: %d", ret);
        }
    }

    LOG_INF("[SENSOR] all events produced");
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_sim_fn,
                NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    LOG_INF("=== L3 Homework: Polling to Workqueue ===");
    LOG_INF("Workqueue version: sensor fires every %dms", SENSOR_MS);

    /* Wait long enough for all events and work to complete */
    k_msleep((EVENT_COUNT + 2) * SENSOR_MS);

    LOG_INF("[SUMMARY] events=%d processed=%d",
            total_events, total_processed);

    return 0;
}