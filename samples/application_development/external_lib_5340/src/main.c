/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* hello world example: calling functions from a static library */


#include <zephyr.h>
#include <stdio.h>

#include <mylib.h>
#include <logging/log.h>

#define LOG_MODULE_NAME external_lib_5340
LOG_MODULE_REGISTER(LOG_MODULE_NAME,4);
void main(void)
{
	printf("Hello World!\n");
	printk("Hello World!\n");
	LOG_ERR("Hello World!\n");
	mylib_hello_world();
	printf("mylib_add(1, 2) %d", mylib_add(1, 2));
}
