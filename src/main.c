#include "sensor/hx711/hx711.h"

#include <stdio.h>
#include <string.h>

#include <device.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>
#include <dt-bindings/dt-util.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

struct moving_average {
	double window[16];
	size_t victim;
	size_t len;
};

static void moving_average_init(struct moving_average *ma)
{
	ma->victim = 0;
	ma->len = 0;
}

static void moving_average_put(struct moving_average *ma, double value)
{
	ma->window[ma->victim] = value;
	ma->victim = (ma->victim + 1) % ARRAY_SIZE(ma->window);
	if (ma->len != ARRAY_SIZE(ma->window)) {
		ma->len++;
	}
}

static double moving_average_get(struct moving_average *ma)
{
	double avg = 0;
	for (size_t i = 0; i < ma->len; i++) {
		avg += ma->window[i];
	}
	avg /= ma->len;
	return avg;
}

void hx711_monitor(void)
{
	const struct device *hx711 = DEVICE_DT_GET(DT_NODELABEL(hx711));
	__ASSERT(hx711 == NULL, "Failed to get device binding");

	struct moving_average ma;
	moving_average_init(&ma);

	k_msleep(100);
	avia_hx711_tare(hx711, 100);

	while (true) {
		k_msleep(50);

		int ret = sensor_sample_fetch(hx711);
		if (ret != 0) {
			LOG_ERR("Cannot take measurement: %d", ret);
			continue;
		}

		struct sensor_value v;
		sensor_channel_get(hx711, (int)HX711_SENSOR_CHAN_WEIGHT, &v);

		double weight = sensor_value_to_double(&v);
		if (weight >= -50 && weight <= 4000) {
			moving_average_put(&ma, weight);
		}

		double average = moving_average_get(&ma);
		printf("avg %f weight %f\n", average, weight);
	}
}

K_THREAD_DEFINE(hx711_id, 1024, hx711_monitor, NULL, NULL, NULL, -1, 0, 0);
