// SPDX-License-Identifier: GPL-2.0
/*
 * Bypass total CQHCI Crypto untuk Neutron Clang.
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
