// SPDX-License-Identifier: GPL-2.0
/*
 * Bypass total CQHCI Crypto Stub untuk Neutron Clang.
 */

#include "cqhci.h"
#include "cqhci-crypto.h"

bool cqhci_crypto_init(struct cqhci_host *host)
{
    return true;
}

int cqhci_host_init_crypto_spec(struct cqhci_host *host, void *ksm_ops)
{
    return 0;
}

void cqhci_crypto_set_suspended(struct cqhci_host *host, bool suspended)
{
}

void cqhci_crypto_enable(struct cqhci_host *host)
{
}

void cqhci_crypto_disable(struct cqhci_host *host)
{
}

int cqhci_host_init_crypto(struct cqhci_host *host)
{
    return 0;
}

int cqhci_crypto_get_ctx(struct cqhci_host *host, struct mmc_request *mrq, u64 *ice_ctx)
{
    return 0;
}

int cqhci_complete_crypto_desc(struct cqhci_host *host, struct mmc_request *mrq, u64 *ice_ctx)
{
    return 0;
}

void cqhci_crypto_debug(struct cqhci_host *host)
{
}

int cqhci_crypto_suspend(struct cqhci_host *host)
{
    return 0;
}

int cqhci_crypto_reset(struct cqhci_host *host)
{
    return 0;
}

int cqhci_crypto_recovery_finish(struct cqhci_host *host)
{
    return 0;
}

void cqhci_crypto_setup_rq_keyslot_manager(struct cqhci_host *host, struct request_queue *q)
{
}

void cqhci_crypto_qti_set_vops(struct cqhci_host *host)
{
}
