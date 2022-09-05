/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);
#include "rhea_finger_gun_120x120.h"
#include "rhea_finger_gun_240x120.h"
#include "two_finger_guns_220x110.h"
#include "price_tag_sample_1.h"
#define DEMO_IMAGE
void main(void)
{
	const struct device *display_dev;
#if !defined(DEMO_IMAGE)
	uint32_t count = 0U;
	char count_str[11] = {0};
#endif
#if defined(DEMO_LABEL)
	lv_obj_t *hello_world_label;
	lv_obj_t *count_label;
#endif
	LOG_INF("%s %d", __func__, __LINE__);
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return;
	}
	LOG_INF("%s %d", __func__, __LINE__);
	display_blanking_on(display_dev);
#if defined(DEMO_IMAGE)
	lv_obj_t * img1 = lv_img_create(lv_scr_act());
	LOG_INF("%s %d", __func__, __LINE__);
	// lv_img_set_src(img1, &rhea_finger_gun_120x120);
	// LOG_HEXDUMP_INF((char*)&rhea_finger_gun_120x120, 300, "lv_img_dsc_t");
	// LOG_INF("data_size %d", rhea_finger_gun_120x120.data_size);
	// LOG_HEXDUMP_INF((char*)rhea_finger_gun_120x120.data, 300, "lv_img_dsc_t.data");
	//lv_img_set_src(img1, &rhea_finger_gun_240x120);
	lv_img_set_src(img1, &price_tag_sample_2);
	LOG_INF("%s %d", __func__, __LINE__);
	lv_obj_align(img1, LV_ALIGN_TOP_LEFT, 0, 0);
	LOG_INF("%s %d", __func__, __LINE__);
	lv_task_handler();
	LOG_INF("%s %d", __func__, __LINE__);
	display_blanking_off(display_dev);
	LOG_INF("%s %d", __func__, __LINE__);
#endif
#if defined(DEMO_LABEL)
	if (IS_ENABLED(CONFIG_LV_Z_POINTER_KSCAN)) {
		lv_obj_t *hello_world_button;
		LOG_INF("%s %d", __func__, __LINE__);
		hello_world_button = lv_btn_create(lv_scr_act());
		lv_obj_align(hello_world_button, LV_ALIGN_CENTER, 0, 0);
		hello_world_label = lv_label_create(hello_world_button);
		LOG_INF("%s %d", __func__, __LINE__);
	} else {
		hello_world_label = lv_label_create(lv_scr_act());
	}

	lv_label_set_text(hello_world_label, "Hello world!");
	lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);
	LOG_INF("%s %d", __func__, __LINE__);
	count_label = lv_label_create(lv_scr_act());
	lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_task_handler();
	display_blanking_off(display_dev);
	LOG_INF("%s %d", __func__, __LINE__);
#endif
#if defined(DEMO_QRCODE)
	/**
	 * Display QR CODE
	 **/
	const char *data = "Hello world";

	/*Create a 120x120 QR code*/
	lv_obj_t *qr = lv_qrcode_create(lv_scr_act(), 120, lv_color_black(), lv_color_white());
	LOG_INF("%s %d", __func__, __LINE__);
	lv_qrcode_update(qr, data, strlen(data));
	LOG_INF("%s %d", __func__, __LINE__);
	lv_obj_center(qr);
	LOG_INF("%s %d", __func__, __LINE__);
	lv_task_handler();
	display_blanking_off(display_dev);
	LOG_INF("%s %d", __func__, __LINE__);
#endif
	while (1) {

#if defined(DEMO_LABEL)
		if ((count % 100) == 0U) {
			sprintf(count_str, "%d", count/100U);
			lv_label_set_text(count_label, count_str);
		}
		//lv_task_handler();
		k_sleep(K_MSEC(10));
		++count;
#endif
	}
}
