/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/crypto/crypto.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_ecb.h>
#include <zephyr/sys/byteorder.h>

#define DT_DRV_COMPAT nordic_nrf_ecb

#define ECB_AES_KEY_SIZE   16
#define ECB_AES_BLOCK_SIZE 16

LOG_MODULE_REGISTER(crypto_nrf_ecb, CONFIG_CRYPTO_LOG_LEVEL);

struct ecb_data {
#if defined(CONFIG_SOC_SERIES_NRF54LX)
	nrf_vdma_job_t in_job;
	nrf_vdma_job_t out_job;
#endif /* CONFIG_SOC_SERIES_NRF54LX */
	uint8_t key[ECB_AES_KEY_SIZE];
	uint8_t cleartext[ECB_AES_BLOCK_SIZE];
	uint8_t ciphertext[ECB_AES_BLOCK_SIZE];
};

struct nrf_ecb_drv_state {
	struct ecb_data data;
	bool in_use;
};

static struct nrf_ecb_drv_state drv_state;

static int do_ecb_encrypt(struct cipher_ctx *ctx, struct cipher_pkt *pkt)
{
	ARG_UNUSED(ctx);

	if (pkt->in_len != ECB_AES_BLOCK_SIZE) {
		LOG_ERR("only 16-byte blocks are supported");
		return -EINVAL;
	}
	if (pkt->out_buf_max < pkt->in_len) {
		LOG_ERR("output buffer too small");
		return -EINVAL;
	}

	if (pkt->in_buf != drv_state.data.cleartext) {
		memcpy(drv_state.data.cleartext, pkt->in_buf,
		       ECB_AES_BLOCK_SIZE);
	}
#if defined(CONFIG_SOC_SERIES_NRF54LX)
	uint32_t *key_reverse = (uint32_t *)drv_state.data.key;

	/* AES key order should be reversed on nRF54L */
	sys_mem_swap(key_reverse, ECB_AES_KEY_SIZE);
	nrf_ecb_key_set(NRF_ECB00, (uint32_t *)drv_state.data.key);

	if (IS_ENABLED(CONFIG_CRYPTO_NRF_ECB_MULTIUSER)) {
		nrf_ecb_in_ptr_set(NRF_ECB00, &drv_state.data.in_job);
		nrf_ecb_out_ptr_set(NRF_ECB00, &drv_state.data.out_job);
	}

	drv_state.data.in_job.size = pkt->in_len;
	drv_state.data.out_job.size = pkt->in_len;
	nrf_ecb_event_clear(NRF_ECB00, NRF_ECB_EVENT_END);
	nrf_ecb_event_clear(NRF_ECB00, NRF_ECB_EVENT_ERROR);
	nrf_ecb_task_trigger(NRF_ECB00, NRF_ECB_TASK_START);
	while (!(nrf_ecb_event_check(NRF_ECB00, NRF_ECB_EVENT_END) ||
		 nrf_ecb_event_check(NRF_ECB00, NRF_ECB_EVENT_ERROR))) {
	}
	if (nrf_ecb_event_check(NRF_ECB00, NRF_ECB_EVENT_ERROR)) {
		LOG_ERR("ECB operation error");
		return -EIO;
	}

#else
	if (IS_ENABLED(CONFIG_CRYPTO_NRF_ECB_MULTIUSER)) {
		nrf_ecb_data_pointer_set(NRF_ECB, &drv_state.data);
	}

	nrf_ecb_event_clear(NRF_ECB, NRF_ECB_EVENT_ENDECB);
	nrf_ecb_event_clear(NRF_ECB, NRF_ECB_EVENT_ERRORECB);
	nrf_ecb_task_trigger(NRF_ECB, NRF_ECB_TASK_STARTECB);
	while (!(nrf_ecb_event_check(NRF_ECB, NRF_ECB_EVENT_ENDECB) ||
		 nrf_ecb_event_check(NRF_ECB, NRF_ECB_EVENT_ERRORECB))) {
	}
	if (nrf_ecb_event_check(NRF_ECB, NRF_ECB_EVENT_ERRORECB)) {
		LOG_ERR("ECB operation error");
		return -EIO;
	}
#endif /* CONFIG_SOC_SERIES_NRF54LX */
	if (pkt->out_buf != drv_state.data.ciphertext) {
		memcpy(pkt->out_buf, drv_state.data.ciphertext,
		       ECB_AES_BLOCK_SIZE);
	}

	pkt->out_len = pkt->in_len;

	return 0;
}

static int nrf_ecb_driver_init(const struct device *dev)
{
	ARG_UNUSED(dev);
#if defined(CONFIG_SOC_SERIES_NRF54LX)
	/* Magic number in nRF54L15 OPS v0.5b page 251 8.7.2 EasyDMA */
	#define NRF_VDMA_ATTRIBUTE_ECB 11

	drv_state.data.in_job.p_buffer = (uint8_t *)&drv_state.data.cleartext;
	drv_state.data.in_job.size = 0;
	drv_state.data.in_job.attributes = NRF_VDMA_ATTRIBUTE_ECB;
	drv_state.data.out_job.p_buffer = (uint8_t *)&drv_state.data.ciphertext;
	drv_state.data.out_job.size = 0;
	drv_state.data.out_job.attributes = NRF_VDMA_ATTRIBUTE_ECB;
	nrf_ecb_in_ptr_set(NRF_ECB00, &drv_state.data.in_job);
	nrf_ecb_out_ptr_set(NRF_ECB00, &drv_state.data.out_job);
#else
	nrf_ecb_data_pointer_set(NRF_ECB, &drv_state.data);
#endif /* CONFIG_SOC_SERIES_NRF54LX */
	drv_state.in_use = false;
	return 0;
}

static int nrf_ecb_query_caps(const struct device *dev)
{
	ARG_UNUSED(dev);

	return (CAP_RAW_KEY | CAP_SEPARATE_IO_BUFS | CAP_SYNC_OPS);
}

static int nrf_ecb_session_setup(const struct device *dev,
				 struct cipher_ctx *ctx,
				 enum cipher_algo algo, enum cipher_mode mode,
				 enum cipher_op op_type)
{
	ARG_UNUSED(dev);

	if ((algo != CRYPTO_CIPHER_ALGO_AES) ||
	    !(ctx->flags & CAP_SYNC_OPS) ||
	    (ctx->keylen != ECB_AES_KEY_SIZE) ||
	    (op_type != CRYPTO_CIPHER_OP_ENCRYPT) ||
	    (mode != CRYPTO_CIPHER_MODE_ECB)) {
		LOG_ERR("This driver only supports 128-bit AES ECB encryption"
			" in synchronous mode");
		return -EINVAL;
	}

	if (ctx->key.bit_stream == NULL) {
		LOG_ERR("No key provided");
		return -EINVAL;
	}

	if (drv_state.in_use) {
		LOG_ERR("Peripheral in use");
		return -EBUSY;
	}

	drv_state.in_use = true;

	ctx->ops.block_crypt_hndlr = do_ecb_encrypt;
	ctx->ops.cipher_mode = mode;

	if (ctx->key.bit_stream != drv_state.data.key) {
		memcpy(drv_state.data.key, ctx->key.bit_stream,
		       ECB_AES_KEY_SIZE);
	}

	return 0;
}

static int nrf_ecb_session_free(const struct device *dev,
				struct cipher_ctx *sessn)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sessn);

	drv_state.in_use = false;

	return 0;
}

static const struct crypto_driver_api crypto_enc_funcs = {
	.cipher_begin_session = nrf_ecb_session_setup,
	.cipher_free_session = nrf_ecb_session_free,
	.cipher_async_callback_set = NULL,
	.query_hw_caps = nrf_ecb_query_caps,
};

DEVICE_DT_INST_DEFINE(0, nrf_ecb_driver_init, NULL,
		      NULL, NULL,
		      POST_KERNEL, CONFIG_CRYPTO_INIT_PRIORITY,
		      &crypto_enc_funcs);
