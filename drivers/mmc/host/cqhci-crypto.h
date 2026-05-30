/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * CQHCI crypto engine (inline encryption) support
 *
 * Copyright 2020 Google LLC
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef LINUX_MMC_CQHCI_CRYPTO_H
#define LINUX_MMC_CQHCI_CRYPTO_H

#include <linux/mmc/host.h>

#include "cqhci.h"
#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
#include <linux/crypto-qti-common.h>
#include "../core/queue.h"
#endif /* CONFIG_QTI_CRYPTO_FDE */

#ifdef CONFIG_MMC_CRYPTO

int cqhci_crypto_init(struct cqhci_host *host);

/*
 * Returns the crypto bits that should be set in bits 64-127 of the
 * task descriptor.
 */
static inline u64 cqhci_crypto_prep_task_desc(struct mmc_request *mrq)
{
	u64 ice_ctx = 0;

	if (!mrq)
		return 0;

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
	{
		struct mmc_queue_req *mqrq;
		struct request *req;
		struct ice_data_setting setting;
		bool bypass = true;
		short key_index = 0;

		mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
		req = mmc_queue_req_to_req(mqrq);

		if (!req || !req->bio)
			return 0;

		/*
		 * If blk-crypto (standard inline encryption) is *not* in use
		 * (no crypt_keyslot), use QTI FDE/ICE configuration.
		 */
		if (!req->crypt_keyslot) {
			int ret;

			ret = crypto_qti_ice_config_start(req, &setting);
			if (ret)
				return 0;

			key_index = setting.crypto_data.key_index;
			bypass = (rq_data_dir(req) == WRITE) ?
				 setting.encr_bypass : setting.decr_bypass;

			ice_ctx = DATA_UNIT_NUM(req->__sector) |
				  CRYPTO_CONFIG_INDEX(key_index) |
				  CRYPTO_ENABLE(!bypass);

			return ice_ctx;
		}
	}
#endif /* CONFIG_QTI_CRYPTO_FDE */

	/*
	 * Generic blk-crypto path (same as cqhci_crypto_prep_task_desc()).
	 * Works both when CONFIG_QTI_CRYPTO_FDE is disabled and when it's
	 * enabled but the request is using blk-crypto (crypt_keyslot set).
	 */
	if (!mrq->crypto_ctx)
		return 0;

	/* We set max_dun_bytes_supported=4, so all DUNs should be 32-bit. */
	WARN_ON_ONCE(mrq->crypto_ctx->bc_dun[0] > U32_MAX);

	ice_ctx = CQHCI_CRYPTO_ENABLE_BIT |
		  CQHCI_CRYPTO_KEYSLOT(mrq->crypto_key_slot) |
		  mrq->crypto_ctx->bc_dun[0];

	return ice_ctx;
}

#else /* CONFIG_MMC_CRYPTO */

static inline int cqhci_crypto_init(struct cqhci_host *host)
{
	return 0;
}

static inline u64 cqhci_crypto_prep_task_desc(struct mmc_request *mrq)
{
	return 0;
}

#endif /* !CONFIG_MMC_CRYPTO */

#endif /* LINUX_MMC_CQHCI_CRYPTO_H */
