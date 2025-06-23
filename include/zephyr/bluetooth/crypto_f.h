/* Copyright (c) 2022 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_CRYPTO_F_H
#define ZEPHYR_INCLUDE_BLUETOOTH_CRYPTO_F_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cryptographic Toolbox f4
 *
 * Defined in Core Vol. 3, part H 2.2.6.
 *
 * @param[in] u 256-bit
 * @param[in] v 256-bit
 * @param[in] x 128-bit key
 * @param[in] z 8-bit
 * @param[out] res
 *
 * @retval 0 Computation was successful. @p res contains the result.
 * @retval -EIO Computation failed.
 */
int bt_crypto_f4(const uint8_t *u, const uint8_t *v, const uint8_t *x, uint8_t z, uint8_t res[16]);

/**
 * @brief Cryptographic Toolbox f5
 *
 * Defined in Core Vol. 3, part H 2.2.7.
 *
 * @param[in] w 256-bit
 * @param[in] n1 128-bit
 * @param[in] n2 128-bit
 * @param[in] a1 56-bit
 * @param[in] a2 56-bit
 * @param[out] mackey most significant 128-bit of the result
 * @param[out] ltk least significant 128-bit of the result
 *
 * @retval 0 Computation was successful. @p res contains the result.
 * @retval -EIO Computation failed.
 */
int bt_crypto_f5(const uint8_t *w, const uint8_t *n1, const uint8_t *n2, const bt_addr_le_t *a1,
		 const bt_addr_le_t *a2, uint8_t *mackey, uint8_t *ltk);

/**
 * @brief Cryptographic Toolbox f6
 *
 * Defined in Core Vol. 3, part H 2.2.8.
 *
 * @param[in] w 128-bit
 * @param[in] n1 128-bit
 * @param[in] n2 128-bit
 * @param[in] r 128-bit
 * @param[in] iocap 24-bit
 * @param[in] a1 56-bit
 * @param[in] a2 56-bit
 * @param[out] check
 *
 * @retval 0 Computation was successful. @p res contains the result.
 * @retval -EIO Computation failed.
 */
int bt_crypto_f6(const uint8_t *w, const uint8_t *n1, const uint8_t *n2, const uint8_t *r,
		 const uint8_t *iocap, const bt_addr_le_t *a1, const bt_addr_le_t *a2,
		 uint8_t *check);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_CRYPTO_F_H */
