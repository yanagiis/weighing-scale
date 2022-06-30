#include "sensor/hx711/hx711.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/dt-bindings/dt-util.h>
#include <zephyr/pm/pm.h>

#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

const static uint8_t characters[][7] = {
	// G, F, E, D, C, B, A
	[0] = {0, 1, 1, 1, 1, 1, 1},  // 0
	[1] = {0, 0, 0, 0, 1, 1, 0},  // 1
	[2] = {1, 0, 1, 1, 0, 1, 1},  // 2
	[3] = {1, 0, 0, 1, 1, 1, 1},  // 3
	[4] = {1, 1, 0, 0, 1, 1, 0},  // 4
	[5] = {1, 1, 0, 1, 1, 0, 1},  // 5
	[6] = {1, 1, 1, 1, 1, 0, 1},  // 6
	[7] = {0, 0, 0, 0, 1, 1, 1},  // 7
	[8] = {1, 1, 1, 1, 1, 1, 1},  // 8
	[9] = {1, 1, 0, 1, 1, 1, 1},  // 9
	[10] = {1, 0, 0, 0, 0, 0, 0}, // -
};

struct moving_average {
	double window[4];
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

static void show_weight(const struct device *max7219, double weight)
{
	uint8_t buf[64] = {0};
	uint8_t digits[9] = {0};

	weight += 0.05;

	int weight_int = (int)weight;

	int idx = 0;
	digits[idx++] = abs((int)((weight - (double)weight_int) * 10)) % 10;

	weight_int = abs(weight_int);
	while (weight_int > 0) {
		digits[idx++] = weight_int % 10;
		weight_int /= 10;
	}

	// if integral part is zero
	if (idx == 1) {
		digits[idx++] = 0;
	}

	// nagitive sign
	if (weight < 0) {
		digits[idx++] = 10;
	}

	for (int i = 0; i < idx; i++) {
		printk("%d ", digits[i]);
		memcpy(buf + (i * 8), characters[digits[i]],
		       sizeof(characters[digits[i]]));
	}
	printk("\n");

	// dot
	buf[15] = 1;

	for (int i = 0; i < ARRAY_SIZE(buf); i++) {
		int ret = (buf[i] != 0) ? led_on(max7219, i)
					: led_off(max7219, i);
		if (ret != 0) {
			LOG_ERR("Failed to update led: ret=%d\n", ret);
		}
	}
}

static const struct gpio_dt_spec zero_button =
	GPIO_DT_SPEC_GET(DT_ALIAS(zerobtn), gpios);
static struct gpio_callback zero_button_cb_data;

static const struct gpio_dt_spec power_button =
	GPIO_DT_SPEC_GET(DT_ALIAS(powerbtn), gpios);
static struct gpio_callback power_button_cb_data;

struct k_sem zero_sem;

static void zero_button_pressed(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	printk("Zero button pressed at %" PRIu32 "\n", k_cycle_get_32());
	k_sem_give(&zero_sem);
}

static void power_button_pressed(const struct device *dev,
				 struct gpio_callback *cb, uint32_t pins)
{
	printk("Power button pressed at %" PRIu32 "\n", k_cycle_get_32());
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
}

static int init_button(const struct gpio_dt_spec *button,
		       struct gpio_callback *cb_data,
		       gpio_callback_handler_t cb)
{
	if (!device_is_ready(button->port)) {
		printk("Error: button device %s is not ready\n",
		       button->port->name);
		return -ENOENT;
	}

	int ret = gpio_pin_configure_dt(button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret,
		       button->port->name, button->pin);
		return -EIO;
	}

	ret = gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
		       ret, button->port->name, button->pin);
		return -EIO;
	}

	gpio_init_callback(cb_data, cb, BIT(button->pin));
	gpio_add_callback(button->port, cb_data);
	return 0;
}

void main(void)
{
	init_button(&zero_button, &zero_button_cb_data, zero_button_pressed);
	init_button(&power_button, &power_button_cb_data, power_button_pressed);
	k_sem_init(&zero_sem, 0, 1);
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
	// XXX: workaround
	// the first value from hx711 always not precise
	avia_hx711_tare(hx711, 10);

	struct sensor_value slope = {.val1 = 0, .val2 = 2116};
	sensor_attr_set(hx711, (enum sensor_channel)HX711_SENSOR_CHAN_WEIGHT,
			(enum sensor_attribute)HX711_SENSOR_ATTR_SLOPE, &slope);

	while (true) {
		int ret;

		ret = k_sem_take(&zero_sem, K_NO_WAIT);
		if (ret == 0) {
			moving_average_init(&ma);
			avia_hx711_tare(hx711, 10);
		}

		ret = sensor_sample_fetch(hx711);
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

		k_msleep(100);
	}
}

K_THREAD_DEFINE(monitor_id, 1024, monitor, NULL, NULL, NULL, -1, 0, 0);
