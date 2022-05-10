/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/flash.h>
#include <device.h>
#include <devicetree.h>
#include <stdio.h>
#include <string.h>
#include "nrfx_clock.h"
#include "nrfx_gpiote.h"
#include <drivers/gpio.h>

#if (CONFIG_SPI_NOR - 0) ||				\
	DT_NODE_HAS_STATUS(DT_INST(0, jedec_spi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#elif (CONFIG_NORDIC_QSPI_NOR - 0) || \
	DT_NODE_HAS_STATUS(DT_INST(0, nordic_qspi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, nordic_qspi_nor))
#define FLASH_NAME "JEDEC QSPI-NOR"
#elif DT_NODE_HAS_STATUS(DT_INST(0, st_stm32_qspi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, st_stm32_qspi_nor))
#define FLASH_NAME "JEDEC QSPI-NOR"
#else
#error Unsupported flash driver
#endif

#if defined(CONFIG_BOARD_ADAFRUIT_FEATHER_STM32F405)
#define FLASH_TEST_REGION_OFFSET 0xf000
#elif defined(CONFIG_BOARD_ARTY_A7_ARM_DESIGNSTART_M1) || \
	defined(CONFIG_BOARD_ARTY_A7_ARM_DESIGNSTART_M3)
/* The FPGA bitstream is stored in the lower 536 sectors of the flash. */
#define FLASH_TEST_REGION_OFFSET \
	DT_REG_SIZE(DT_NODE_BY_FIXED_PARTITION_LABEL(fpga_bitstream))
#elif defined(CONFIG_BOARD_NPCX9M6F_EVB) || \
	defined(CONFIG_BOARD_NPCX7M6FB_EVB)
#define FLASH_TEST_REGION_OFFSET 0x7F000
#else
//#define FLASH_TEST_REGION_OFFSET 0xff000
#define FLASH_TEST_REGION_OFFSET 0x0
#endif
#define FLASH_SECTOR_SIZE        4096
#define SHARED_SPI DT_NODELABEL(spi4) /* SD card and HW codec share the SPI4 */
#if defined(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP)
static int core_app_config(void)
{
	int ret;

	nrf_gpiote_latency_t latency = nrfx_gpiote_latency_get();

	if (latency != NRF_GPIOTE_LATENCY_LOWPOWER) {
		printk("Setting gpiote latency to low power");
		nrfx_gpiote_latency_set(NRF_GPIOTE_LATENCY_LOWPOWER);
	}

	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);

	nrfx_clock_hfclk_start();
	while (!nrfx_clock_hfclk_is_running()) {
	}

	/* Workaround for issue with PCA10121 v0.7.0 related to SD-card */
	static const struct device *gpio_dev;

	nrf_gpio_pin_drive_t high_drive = NRF_GPIO_PIN_H0H1;

	nrf_gpio_reconfigure(DT_PROP(SHARED_SPI, mosi_pin), NULL, NULL, NULL, &high_drive, NULL);
	nrf_gpio_reconfigure(DT_PROP(SHARED_SPI, sck_pin), NULL, NULL, NULL, &high_drive, NULL);

	gpio_dev = device_get_binding("GPIO_0");

	if (gpio_dev == NULL) {
		return -ENODEV;
	}

	/* USB port detection
	 * See nPM1100 datasheet for more information
	 */
	ret = gpio_pin_configure(gpio_dev, DT_GPIO_PIN(DT_NODELABEL(pmic_iset_out), gpios),
				 GPIO_OUTPUT_LOW);
#ifdef CONFIG_5340DK_PIN
	static const struct gpio_dt_spec dsp_sel =
		GPIO_DT_SPEC_GET(DT_NODELABEL(dsp_sel_out), gpios);

	if (!device_is_ready(dsp_sel.port)) {
		printk("GPIO is not ready!");
		return -ENXIO;
	}

	ret = gpio_pin_configure_dt(&dsp_sel, GPIO_OUTPUT_HIGH);
	if (ret) {
		return ret;
	}
#endif
	return 0;
}
#endif
void main(void)
{
	const uint8_t expected[] = { 0x55, 0xaa, 0x66, 0x99, 0x68, 0x61, 0x70, 0x70, 0x69, 0x72, 0x75, 0x6e};
	//const uint8_t expected[] = { 0x68, 0x61, 0x70, 0x70, 0x55, 0xaa, 0x66, 0x99};
	//const uint8_t expected[] = { 0x55, 0xaa, 0x66, 0x99};

	const size_t len = sizeof(expected);
	uint8_t buf[sizeof(expected)];
	const struct device *flash_dev;
	int rc;
#if defined(CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP)
	core_app_config();
#endif	
	printf("\n" FLASH_NAME " SPI flash testing\n");
	printf("==========================\n");
	printf("%s\n", FLASH_DEVICE);

	flash_dev = device_get_binding(FLASH_DEVICE);
	if (!flash_dev) {
		printf("SPI flash driver %s was not found!\n",
		       FLASH_DEVICE);
		return;
	}

	/* Write protection needs to be disabled before each write or
	 * erase, since the flash component turns on write protection
	 * automatically after completion of write and erase
	 * operations.
	 */
	printf("\nTest 1: Flash erase\n");

	rc = flash_erase(flash_dev, FLASH_TEST_REGION_OFFSET,
			 FLASH_SECTOR_SIZE);
	if (rc != 0) {
		printf("Flash erase failed! %d\n", rc);
	} else {
		printf("Flash erase succeeded!\n");
	}

	printf("\nTest 2: Flash write\n");

	printf("Attempting to write %d bytes\n", len);
	#define WRITE_LENGTH 4
	for(int i=0;i*WRITE_LENGTH<len;i++) {
		printf("round %d\n", i);
		rc = flash_write(flash_dev, FLASH_TEST_REGION_OFFSET+(i*WRITE_LENGTH), expected+(i*WRITE_LENGTH), WRITE_LENGTH);
		if (rc != 0) {
			printf("Flash write failed! %d\n", rc);
			return;
		}

		memset(buf, 0, WRITE_LENGTH);
		rc = flash_read(flash_dev, FLASH_TEST_REGION_OFFSET+(i*WRITE_LENGTH), buf, WRITE_LENGTH);
		if (rc != 0) {
			printf("Flash read failed! %d\n", rc);
			return;
		}

		if (memcmp(expected+(i*WRITE_LENGTH), buf, WRITE_LENGTH) == 0) {
			printf("Data read matches data written. Good!!\n");
		} else {
			const uint8_t *wp = expected+(i*WRITE_LENGTH);
			const uint8_t *rp = buf;
			const uint8_t *rpe = rp + WRITE_LENGTH;

			printf("Data read does not match data written!!\n");
			while (rp < rpe) {
				printf("%08x wrote %02x read %02x %s\n",
					(uint32_t)(FLASH_TEST_REGION_OFFSET + (rp - buf)+(i*WRITE_LENGTH)),
					*wp, *rp, (*rp == *wp) ? "match" : "MISMATCH");
				++rp;
				++wp;
			}
		}
	}

}
