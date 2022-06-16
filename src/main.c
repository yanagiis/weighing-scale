#include <stdio.h>
#include <string.h>

#include <device.h>
#include <drivers/display.h>
#include <drivers/gpio.h>
#include <lvgl.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define HX711_POWER_PIN 27
#define HX711_RX_PIN	13
#define HX711_TX_PIN	12

static struct k_work hx711_work;
static struct gpio_callback gpio_cb;

static void hx711_ready_cb(const struct device *port, struct gpio_callback *cb,
			   gpio_port_pins_t pins);
static void hx711_recv(struct k_work *work);

void main(void)
{
	k_work_init(&hx711_work, hx711_recv);

	const struct device *gpio0 = DEVICE_DT_GET(DT_ALIAS(gpio0));
	gpio_pin_configure(gpio0, HX711_POWER_PIN,
			   GPIO_OUTPUT | GPIO_OUTPUT_INIT_HIGH);
	gpio_pin_configure(gpio0, HX711_RX_PIN, GPIO_INPUT);
	gpio_pin_configure(gpio0, HX711_TX_PIN,
			   GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);

	gpio_init_callback(&gpio_cb, hx711_ready_cb, BIT(HX711_RX_PIN));
	gpio_add_callback(gpio0, &gpio_cb);

	gpio_pin_interrupt_configure(gpio0, HX711_RX_PIN,
				     GPIO_INT_EDGE_FALLING);
}

static void display(void)
{
	const struct device *display_dev = device_get_binding("ST7789V");
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return;
	}

	const struct device *gpio0 = DEVICE_DT_GET(DT_ALIAS(gpio0));
	gpio_pin_configure(gpio0, 4, GPIO_OUTPUT | GPIO_OUTPUT_INIT_HIGH);

	lv_obj_t *hello_world_label;
	if (IS_ENABLED(CONFIG_LV_Z_POINTER_KSCAN)) {
		lv_obj_t *hello_world_button;

		hello_world_button = lv_btn_create(lv_scr_act());
		lv_obj_align(hello_world_button, LV_ALIGN_CENTER, 0, 0);
		hello_world_label = lv_label_create(hello_world_button);
	} else {
		hello_world_label = lv_label_create(lv_scr_act());
	}

	lv_label_set_text(hello_world_label, "Hello world!");
	lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t *count_label = lv_label_create(lv_scr_act());
	lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);

	lv_task_handler();
	display_blanking_off(display_dev);

	LOG_INF("ready");
	uint32_t count = 0U;
	char count_str[11] = {0};
	while (1) {
		if ((count % 100) == 0U) {
			sprintf(count_str, "%d", count / 100U);
			lv_label_set_text(count_label, count_str);
		}
		lv_task_handler();
		k_sleep(K_MSEC(10));
		++count;
	}
}

static void hx711_ready_cb(const struct device *port, struct gpio_callback *cb,
			   gpio_port_pins_t pins)
{
	k_work_submit(&hx711_work);
}

static void hx711_recv(struct k_work *work)
{
	const struct device *gpio0 = DEVICE_DT_GET(DT_ALIAS(gpio0));

	uint32_t raw = 0;
	for (int i = 0; i < 25; i++) {
		raw <<= 1;

		gpio_port_set_bits_raw(gpio0, BIT(HX711_TX_PIN));

		gpio_port_value_t v;
		gpio_port_get(gpio0, &v);

		raw |= (v & BIT(HX711_RX_PIN)) >> (HX711_RX_PIN);
		gpio_port_clear_bits_raw(gpio0, BIT(HX711_TX_PIN));
	}
}

K_THREAD_DEFINE(display_id, 1024, display, NULL, NULL, NULL, 7, 0, 0);
