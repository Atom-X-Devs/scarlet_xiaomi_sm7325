// SPDX-License-Identifier: GPL-2.0
/* 
 * Di-sunat paksa buat bypass error Neutron Clang.
 * Redwood pake UFS, MMC Crypto kaga guna dimari!
 */

#include <linux/blk-crypto.h>
#include <linux/blkdev.h>
#include <linux/keyslot-manager.h>
#include <linux/mmc/host.h>

#include "core.h"
#include "queue.h"

void mmc_crypto_setup_queue(struct mmc_host *host, struct request_queue *q) 
{
    // Dikosongin biar Clang ga cerewet
}
EXPORT_SYMBOL_GPL(mmc_crypto_setup_queue);

void mmc_crypto_free_host(struct mmc_host *host) 
{
    // Dikosongin
}

void mmc_crypto_prepare_req(struct mmc_queue_req *mqrq) 
{
    // Dikosongin
}
EXPORT_SYMBOL_GPL(mmc_crypto_prepare_req);
