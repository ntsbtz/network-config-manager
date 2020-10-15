/* SPDX-License-Identifier: Apache-2.0
 * Copyright © 2020 VMware, Inc.
 */

#pragma once

#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <libmnl/libmnl.h>
#include <libnftnl/chain.h>
#include <libnftnl/table.h>

#include <gmodule.h>

typedef enum NfProtoFamily {
        NF_PROTO_FAMILY_UNSPEC = NFPROTO_UNSPEC,
        NF_PROTO_FAMILY_INET   = NFPROTO_INET,
        NF_PROTO_FAMILY_IPV4   = NFPROTO_IPV4,
        NF_PROTO_FAMILY_ARP    = NFPROTO_ARP,
        NF_PROTO_FAMILY_NETDEV = NFPROTO_NETDEV,
        NF_PROTO_FAMILY_BRIDGE = NFPROTO_BRIDGE,
        NF_PROTO_FAMILY_IPV6   = NFPROTO_IPV6,
        NF_PROTO_FAMILY_DECNET = NFPROTO_DECNET,
        _NF_PROTO_FAMILY_MAX,
        _NF_PROTO_FAMILY_INVALID = -1
} NfProtoFamily;

typedef struct NFTNLTable {
        struct nftnl_table *table;

        char *name;
        int family;
} NFTNLTable;

const char *nft_family_to_name(int id);
int nft_family_name_to_type(char *name);

void nft_table_unref(NFTNLTable **t);

int new_nft_table(int family, const char *name, NFTNLTable **ret);
int nft_add_table(int family, const char *name);
int nft_get_tables(int family, GPtrArray **ret);

void nft_chain_unref(struct nftnl_chain **c);
int new_nft_chain(int family, const char *name, const char *table, struct nftnl_chain **ret);
int nft_add_chain(int family, const char *table, const char *name);
