/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CRYPTO_QTI_COMMON_H
#define _CRYPTO_QTI_COMMON_H

#include <linux/blk-crypto.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/delay.h>

#define QTI_ICE_MAX_BIST_CHECK_COUNT 100
#define QTI_ICE_TYPE_NAME_LEN 8

#define ICE_REGS_NON_SEC_IRQ_MASK              0x0044
#define ICE_STREAM1_PREMATURE_LBA_CHANGE       (1L << 0)
#define ICE_STREAM2_PREMATURE_LBA_CHANGE       (1L << 1)
#define ICE_STREAM1_NOT_EXPECTED_LBO           (1L << 2)
#define ICE_STREAM2_NOT_EXPECTED_LBO           (1L << 3)
#define ICE_STREAM1_NOT_EXPECTED_DUN           (1L << 4)
#define ICE_STREAM2_NOT_EXPECTED_DUN           (1L << 5)
#define ICE_STREAM1_NOT_EXPECTED_DUS           (1L << 6)
#define ICE_STREAM2_NOT_EXPECTED_DUS           (1L << 7)
#define ICE_STREAM1_NOT_EXPECTED_DBO           (1L << 8)
#define ICE_STREAM2_NOT_EXPECTED_DBO           (1L << 9)
#define ICE_STREAM1_NOT_EXPECTED_ENC_SEL       (1L << 10)
#define ICE_STREAM2_NOT_EXPECTED_ENC_SEL       (1L << 11)
#define ICE_STREAM1_NOT_EXPECTED_CONF_IDX      (1L << 12)
#define ICE_STREAM2_NOT_EXPECTED_CONF_IDX      (1L << 13)
#define ICE_STREAM1_NOT_EXPECTED_NEW_TRNS      (1L << 14)
#define ICE_STREAM2_NOT_EXPECTED_NEW_TRNS      (1L << 15)

#define ICE_NON_SEC_IRQ_MASK                           \
			(ICE_STREAM1_PREMATURE_LBA_CHANGE |\
			ICE_STREAM2_PREMATURE_LBA_CHANGE |\
			ICE_STREAM1_NOT_EXPECTED_LBO |\
			ICE_STREAM2_NOT_EXPECTED_LBO |\
			ICE_STREAM1_NOT_EXPECTED_DUN |\
			ICE_STREAM2_NOT_EXPECTED_DUN |\
			ICE_STREAM2_NOT_EXPECTED_DUS |\
			ICE_STREAM1_NOT_EXPECTED_DBO |\
			ICE_STREAM2_NOT_EXPECTED_DBO |\
			ICE_STREAM1_NOT_EXPECTED_ENC_SEL |\
			ICE_STREAM2_NOT_EXPECTED_ENC_SEL |\
			ICE_STREAM1_NOT_EXPECTED_CONF_IDX |\
			ICE_STREAM1_NOT_EXPECTED_NEW_TRNS |\
			ICE_STREAM2_NOT_EXPECTED_NEW_TRNS)

#define crypto_qti_ice_writel(ice, val, reg)    \
	writel_relaxed((val), (ice)->mmio + (reg))
#define crypto_qti_ice_readl(ice, reg)  \
	readl_relaxed((ice)->mmio + (reg))

struct crypto_vops_qti_entry {
	void __iomem *icemmio_base;
	void __iomem *hwkm_slave_mmio_base;
	uint32_t ice_hw_version;
	uint8_t ice_dev_type[QTI_ICE_TYPE_NAME_LEN];
	uint32_t flags;
};

/* MSM ICE Crypto Data Unit of target DUN of Transfer Request */
enum ice_crypto_data_unit {
	ICE_CRYPTO_DATA_UNIT_512_B	= 0,
	ICE_CRYPTO_DATA_UNIT_1_KB	= 1,
	ICE_CRYPTO_DATA_UNIT_2_KB	= 2,
	ICE_CRYPTO_DATA_UNIT_4_KB	= 3,
	ICE_CRYPTO_DATA_UNIT_8_KB	= 4,
	ICE_CRYPTO_DATA_UNIT_16_KB	= 5,
	ICE_CRYPTO_DATA_UNIT_32_KB	= 6,
	ICE_CRYPTO_DATA_UNIT_64_KB	= 7,
};
struct request;

enum ice_cryto_algo_mode {
	ICE_CRYPTO_ALGO_MODE_AES_ECB = 0x0,
	ICE_CRYPTO_ALGO_MODE_AES_XTS = 0x3,
};

enum ice_crpto_key_size {
	ICE_CRYPTO_KEY_SIZE_128 = 0x0,
	ICE_CRYPTO_KEY_SIZE_256 = 0x2,
};

struct ice_crypto_setting {
	enum ice_crpto_key_size		key_size;
	enum ice_cryto_algo_mode	algo_mode;
	short				key_index;
};

struct ice_data_setting {
	struct ice_crypto_setting	crypto_data;
	bool				sw_forced_context_switch;
	bool				decr_bypass;
	bool				encr_bypass;
};
typedef void (*ice_error_cb)(void *, u32 error);

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
int crypto_qti_ice_setup_ice_hw(const char *storage_type, int enable);
int crypto_qti_ice_config_start(struct request *req,
				struct ice_data_setting *setting);
unsigned int crypto_qti_ice_get_num_fde_slots(void);
int crypto_qti_ice_init_fde_node(struct device *dev);
int crypto_qti_ice_add_userdata(const unsigned char *inhash);
#else
static inline int crypto_qti_ice_setup_ice_hw(const char *storage_type,
					      int enable)
{
	return 0;
}

static inline int crypto_qti_ice_config_start(struct request *req,
					      struct ice_data_setting *setting)
{
	return 0;
}

static inline unsigned int crypto_qti_ice_get_num_fde_slots(void)
{
	return 0;
}

static inline int crypto_qti_ice_init_fde_node(struct device *dev)
{
	return 0;
}

static inline int crypto_qti_ice_add_userdata(const unsigned char *inhash)
{
	return 0;
}
#endif /* CONFIG_QTI_CRYPTO_FDE */
#endif /* _CRYPTO_QTI_COMMON_H */
