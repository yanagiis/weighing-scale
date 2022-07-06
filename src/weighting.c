#include "sensor/hx711/hx711.h"
#include "weighting.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/dt-bindings/dt-util.h>
#include <zephyr/pm/pm.h>

#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(weighting_sensor);

#define MOVING_MEDIAN_SIZE 8
#define NUM_HX711	   2

struct moving_median {
	double records[MOVING_MEDIAN_SIZE];
	int idx;
};

static void moving_median_init(struct moving_median *m)
{
	memset(m->records, 0, sizeof(m->records));
	m->idx = 0;
}

static void moving_median_put(struct moving_median *m, double value)
{
	m->records[m->idx] = value;
	m->idx = (m->idx + 1) % ARRAY_SIZE(m->records);
}

static int double_compare(const void *a, const void *b)
{
	double c = *(double *)a;
	double d = *(double *)b;
	if (c < d) {
		return -1;
	} else if (c == d) {
		return 0;
	} else {
		return 1;
	}
}

static double moving_median_get(struct moving_median *m)
{
	double sorted[MOVING_MEDIAN_SIZE];
	memcpy(sorted, m->records, sizeof(sorted));
	qsort(sorted, ARRAY_SIZE(sorted), sizeof(sorted[0]), double_compare);
	return sorted[ARRAY_SIZE(sorted) / 2];
}

struct weighting_sensor {
	struct moving_median mm;
	struct sensor_value slope;
	const struct device *hx711;
};

static struct weighting_sensor sensors[NUM_HX711];

int weighting_init(void)
{
	sensors[0].hx711 = DEVICE_DT_GET(DT_NODELABEL(hx711_1));
	sensors[1].hx711 = DEVICE_DT_GET(DT_NODELABEL(hx711_2));
	__ASSERT(sensors[0].hx711 == NULL,
		 "Failed to get hx711_1 device binding");
	__ASSERT(sensors[1].hx711 == NULL,
		 "Failed to get hx711_2 device binding");

	sensors[0].slope.val1 = 0;
	sensors[0].slope.val2 = 2116;
	sensors[1].slope.val1 = 0;
	sensors[1].slope.val2 = 1977;

	for (int i = 0; i < ARRAY_SIZE(sensors); i++) {
		moving_median_init(&sensors[i].mm);
		// FIXME: workaround
		// the first few values from hx711 are not precise sometimes
		avia_hx711_tare(sensors[i].hx711, 5);
		avia_hx711_tare(sensors[i].hx711, 5);
		sensor_attr_set(sensors[i].hx711,
				(enum sensor_channel)HX711_SENSOR_CHAN_WEIGHT,
				(enum sensor_attribute)HX711_SENSOR_ATTR_SLOPE,
				&sensors[i].slope);
	}

	return 0;
}

int weighting_deinit(void)
{
	for (int i = 0; i < ARRAY_SIZE(sensors); i++) {
		avia_hx711_power(sensors[i].hx711, HX711_POWER_OFF);
	}
	return 0;
}

int weighting_tare(void)
{
	for (int i = 0; i < ARRAY_SIZE(sensors); i++) {
		avia_hx711_tare(sensors[i].hx711, 5);
		moving_median_init(&sensors[0].mm);
	}
	return 0;
}

int weighting_get(double *weight)
{
	*weight = 0;
	for (int i = 0; i < ARRAY_SIZE(sensors); i++) {
		sensor_sample_fetch(sensors[i].hx711);

		struct sensor_value v;
		sensor_channel_get(
			sensors[i].hx711,
			(enum sensor_channel)HX711_SENSOR_CHAN_WEIGHT, &v);

		moving_median_put(&sensors[i].mm, sensor_value_to_double(&v));
		*weight += moving_median_get(&sensors[i].mm);
	}
	return 0;
}
