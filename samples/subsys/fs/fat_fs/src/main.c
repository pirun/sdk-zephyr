/*
 * Copyright (c) 2019 Tavish Naruka <tavishnaruka@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample which uses the filesystem API and SDHC driver */

#include <zephyr.h>
#include <device.h>
#include <storage/disk_access.h>
#include <logging/log.h>
#include <fs/fs.h>
#include <ff.h>
#include <drivers/gpio.h>
#include "nrfx_gpiote.h"
#include "nrfx_clock.h"

LOG_MODULE_REGISTER(main);

static int lsdir(const char *path);
static int dumpfile(const char *path);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};
#define RET_IF_ERR(err_code)                                                                       \
	do {                                                                                       \
		if (err_code) {                                                                    \
			return err_code;                                                           \
		}                                                                                  \
	} while (0)

#define RET_IF_ERR_MSG(err_code, msg)                                                              \
	do {                                                                                       \
		if (err_code) {                                                                    \
			LOG_ERR("%s", msg);                                                        \
			return err_code;                                                           \
		}                                                                                  \
	} while (0)

/*
*  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
*  in ffconf.h
*/
static const char *disk_mount_pt = "/SD:";
#define SD_ROOT_PATH "/SD:/"

#define PATH_MAX_LEN 260 /* Maximum length for path support by Windows file system*/
#define SHARED_SPI DT_NODELABEL(spi4) /* SD card and HW codec share the SPI4 */

static int core_app_config(void)
{
	int ret;

	nrf_gpiote_latency_t latency = nrfx_gpiote_latency_get();

	if (latency != NRF_GPIOTE_LATENCY_LOWPOWER) {
		LOG_DBG("Setting gpiote latency to low power");
		nrfx_gpiote_latency_set(NRF_GPIOTE_LATENCY_LOWPOWER);
	}

	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
	if(ret - NRFX_ERROR_BASE_NUM)
		LOG_ERR("nrfx_clock_divider_set failed");

	nrfx_clock_hfclk_start();
	while (!nrfx_clock_hfclk_is_running()) {
	}

	/* Workaround for issue with PCA10121 v0.7.0 related to SD-card */
	static const struct device *gpio_dev;

	gpio_dev = device_get_binding(DT_SPI_DEV_CS_GPIOS_LABEL(DT_NODELABEL(sdhc0)));
	if (!gpio_dev) {
		return -ENODEV;
	}

	ret = gpio_pin_configure(gpio_dev, DT_PROP(SHARED_SPI, mosi_pin),
				 GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW);
	if(ret) {
		LOG_ERR("gpio_pin_configure mosi failed");
		return;
	}
	ret = gpio_pin_configure(gpio_dev, DT_PROP(SHARED_SPI, sck_pin),
				 GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW);
	if(ret) {
		LOG_ERR("gpio_pin_configure sck_pin failed");
		return;
	}
	gpio_dev = device_get_binding("GPIO_0");

	if (gpio_dev == NULL) {
		return -ENODEV;
	}
	/* USB port detection
	 * See nPM1100 datasheet for more information
	 */
	ret = gpio_pin_configure(gpio_dev, DT_GPIO_PIN(DT_NODELABEL(pmic_iset_out), gpios),
				 GPIO_OUTPUT_LOW);
	if(ret) {
		LOG_ERR("gpio_pin_configure pmic_iset_out");
		return;
	}
	return 0;
}
void sd_init()
{
	do {
		static const char *disk_pdrv = "SD";
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;

		if (disk_access_init(disk_pdrv) != 0) {
			LOG_ERR("Storage init ERROR!");
			break;
		}

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		printk("Sector size %u\n", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
	} while (0);

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK) {
		printk("Disk mounted.\n");
		lsdir(disk_mount_pt);
		dumpfile("1.txt");
		dumpfile("zephyr.dts");
	} else {
		printk("Error mounting disk.\n");
	}

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
void main(void)
{
	core_app_config();
	/* raw disk i/o */
	sd_init();
}

static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		printk("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	printk("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			printk("[DIR ] %s\n", entry.name);
		} else {
			printk("[FILE] %s (size = %zu)\n",
				entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

int dumpfile(const char *path)
{
	struct fs_file_t f_entry;
	char abs_path_name[PATH_MAX_LEN + 1] = SD_ROOT_PATH;
	int ret;
    size_t buf_size = 256;
	char const buf[buf_size];
    off_t offset = 0;
    size_t read = 0;

	fs_file_t_init(&f_entry);
	if (strlen(path) > CONFIG_FS_FATFS_MAX_LFN) {
		RET_IF_ERR_MSG(-FR_INVALID_NAME, "Filename is too long");
	} else {
		strcat(abs_path_name, path);
	}
	
	ret = fs_open(&f_entry, abs_path_name, FS_O_READ);
	if (ret) LOG_ERR("ret = %d", ret);

	RET_IF_ERR_MSG(ret, "Open file failed");
	ret = fs_read(&f_entry, buf, buf_size);
	if (ret <= 0) {
		RET_IF_ERR_MSG(ret, "Read file failed");
	} else {
		do {
			read = fs_read(&f_entry, buf, buf_size);
			if (read < 0) {
				LOG_ERR("Failed to read file data (%d)", read);
				goto done;
			}
			if (read < buf_size) {
				memset(buf + read, 0xFF, buf_size - read);
			}
			LOG_INF("read size %d", read);
			LOG_HEXDUMP_INF(buf, read, "dumpfile");
			offset += read;
		} while (read == buf_size);
	}
done:
	ret = fs_close(&f_entry);
	RET_IF_ERR_MSG(ret, "Close file failed");

	return 0;
}
