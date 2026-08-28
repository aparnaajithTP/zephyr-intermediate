#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(assignment4, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define SENSOR_PERIOD_MS 100
#define SENSOR_COUNT 10

struct sensor_data {
    uint32_t timestamp_ms;
    int32_t value;
    uint8_t seq;
};

/* Forward declaration for the listener callback */
static void display_listener_cb(const struct zbus_channel *chan);

/* Listener: fast display update */
ZBUS_LISTENER_DEFINE(display_listener, display_listener_cb);

/* Subscriber: slower logging */
ZBUS_MSG_SUBSCRIBER_DEFINE(logger_subscriber);

/* One zbus channel */
ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_data,
                 NULL, NULL,
                 ZBUS_OBSERVERS(display_listener, logger_subscriber),
                 ZBUS_MSG_INIT(.timestamp_ms = 0,
                               .value = 0,
                               .seq = 0));

/*
 * Fast listener.
 *
 * This runs immediately when the sensor publishes.
 */
static void display_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_data *msg =
        (const struct sensor_data *)zbus_chan_const_msg(chan);

    LOG_INF("[DISPLAY] seq=%u value=%d tick=%u",
            msg->seq,
            msg->value,
            k_uptime_get_32());
}

/*
 * Sensor publisher.
 *
 * Publishes one sample every 100 ms.
 */
static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "sensor");

    for (int i = 0; i < SENSOR_COUNT; i++) {
        struct sensor_data data = {
            .timestamp_ms = k_uptime_get_32(),
            .value = 100 + i,
            .seq = (uint8_t)i,
        };

        LOG_INF("[SENSOR] publish seq=%u value=%d",
                data.seq,
                data.value);

        int ret = zbus_chan_pub(&sensor_chan, &data, K_MSEC(100));

        if (ret != 0) {
            LOG_ERR("[SENSOR] publish failed: %d", ret);
        }

        k_msleep(SENSOR_PERIOD_MS);
    }

    LOG_INF("[SENSOR] done");
}

/*
 * Slow subscriber.
 *
 * Unlike the listener, this runs in its own thread and
 * deliberately processes messages slowly.
 */
static void logger_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "logger");

    const struct zbus_channel *chan;
    int received = 0;

    while (received < SENSOR_COUNT) {
        struct sensor_data msg;

        int ret = zbus_sub_wait_msg(&logger_subscriber,
                                    &chan,
                                    &msg,
                                    K_MSEC(1000));

        if (ret != 0) {
            LOG_WRN("[LOGGER] timeout: %d", ret);
            break;
        }

        received++;

        LOG_INF("[LOGGER] seq=%u value=%d tick=%u",
                msg.seq,
                msg.value,
                k_uptime_get_32());

        /* Simulate slower logging */
        k_msleep(250);
    }

    LOG_INF("[LOGGER] done received=%d", received);
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE,
                sensor_thread_fn,
                NULL, NULL, NULL,
                5, 0, 0);

K_THREAD_DEFINE(logger_thread, STACK_SIZE,
                logger_thread_fn,
                NULL, NULL, NULL,
                6, 0, 0);

int main(void)
{
    LOG_INF("=== L4 Assignment: Zbus ===");
    LOG_INF("Sensor publishes every %d ms", SENSOR_PERIOD_MS);
    LOG_INF("One fast listener + one slow subscriber");

    return 0;
}