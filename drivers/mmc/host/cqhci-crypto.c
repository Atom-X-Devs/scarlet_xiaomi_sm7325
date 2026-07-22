// SPDX-License-Identifier: GPL-2.0
/*
 * Sunat paksa CQHCI Crypto untuk bypass error Clang terbaru.
 * Redwood menggunakan UFS, driver MMC/CQHCI Crypto ini tidak digunakan.
 */

#include "cqhci.h"
#include "cqhci-crypto.h"

bool cqhci_crypto_init(struct cqhci_host *host)
{
    return true;
}

int cqhci_host_init_crypto_spec(struct cqhci_host *host, const struct keyslot_mgmt_ll_ops *ksm_ops)
{
    return 0;
}

void cqhci_crypto_set_suspended(struct cqhci_host *host, bool suspended)
{
}
