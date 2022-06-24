#include "sensor/hx711/hx711.h"

#include <stdio.h>
#include <string.h>

#include <device.h>
#include <drivers/led.h>
#include <drivers/sensor.h>
#include <dt-bindings/dt-util.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

static uint8_t characters[][7] = {
	// G, F, E, D, C, B, A
	{0, 1, 1, 1, 1, 1, 1}, // 0
	{0, 0, 0, 0, 1, 1, 0}, // 1
	{1, 0, 1, 1, 0, 1, 1}, // 2
	{1, 0, 0, 1, 1, 1, 1}, // 3
	{1, 1, 0, 0, 1, 1, 0}, // 4
	{1, 1, 0, 1, 1, 0, 1}, // 5
	{1, 1, 1, 1, 1, 0, 1}, // 6
	{0, 0, 0, 0, 1, 1, 1}, // 7
	{1, 1, 1, 1, 1, 1, 1}, // 8
	{1, 1, 0, 1, 1, 1, 1}, // 9
	{1, 0, 0, 0, 0, 0, 0}, // -
};

struct moving_average {
	double window[8];
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

static int show_weight(const struct device *max7219, double weight)
{
	uint8_t buf[64] = {0};
	uint8_t digits[8] = {0};

	struct sensor_value v = {0};
	sensor_value_from_double(&v, weight);

	size_t num_digits = 0;
	digits[num_digits++] = v.val2 / 100000;
	while (num_digits < (ARRAY_SIZE(digits) - 1) && v.val1 > 0) {
		digits[num_digits++] = v.val1 % 10;
		v.val1 /= 10;
	}
	if (num_digits == 1) {
		digits[num_digits++] = 0;
	}
	if (weight < 0) {
		digits[num_digits++] = 10;
	}

	for (size_t i = 0; i < num_digits; i++) {
		memcpy(buf + (i * 8), characters[digits[i]],
		       sizeof(characters[digits[i]]));
	}
	buf[15] = 1;

	int ret = led_write_channels(max7219, 0, ARRAY_SIZE(buf), buf);
	if (ret != 0) {
		LOG_ERR("led update: ret=%d", ret);
	}
	return ret;
}

void monitor(void)
{
	const struct device *hx711 = DEVICE_DT_GET(DT_NODELABEL(hx711));
	__ASSERT(hx711 == NULL, "Failed to get hx711 device binding");
	const struct device *max7219 = DEVICE_DT_GET(DT_NODELABEL(max7219));
	__ASSERT(max7219 == NULL, "Failed to get max7219 device binding");

	struct moving_average ma;
	moving_average_init(&ma);

	avia_hx711_tare(hx711, 1);
	// workaround
	// the first value from hx711 always not precise
	avia_hx711_tare(hx711, 20);

	struct sensor_value slope = {.val1 = 0, .val2 = 2116};
	sensor_attr_set(hx711, (enum sensor_channel)HX711_SENSOR_CHAN_WEIGHT,
			(enum sensor_attribute)HX711_SENSOR_ATTR_SLOPE, &slope);

	while (true) {
		int ret = sensor_sample_fetch(hx711);
		if (ret != 0) {
			LOG_ERR("Cannot take measurement: %d", ret);
			continue;
		}

		struct sensor_value v;
		sensor_channel_get(
			hx711, (enum sensor_channel)HX711_SENSOR_CHAN_WEIGHT,
			&v);

		double weight = sensor_value_to_double(&v);
		moving_average_put(&ma, weight);
		show_weight(max7219, moving_average_get(&ma));
	}
}

K_THREAD_DEFINE(monitor_id, 1024, monitor, NULL, NULL, NULL, -1, 0, 0);
