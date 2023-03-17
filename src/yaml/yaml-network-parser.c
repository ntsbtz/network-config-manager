/* Copyright 2023 VMware, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "alloc-util.h"
#include "file-util.h"
#include "log.h"
#include "macros.h"
#include "network-manager.h"
#include "netdev-link.h"
#include "network.h"
#include "networkd-api.h"
#include "parse-util.h"
#include "string-util.h"
#include "yaml-network-parser.h"
#include "yaml-parser.h"

static WiFiAccessPoint *wifi_access_point;

static ParserTable parser_wifi_vtable[] = {
        { "ssid-name",           CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WiFiAccessPoint,    ssid)},
        { "password",            CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, password)},
        { "key-management",      CONF_TYPE_WIFI,     parse_yaml_auth_key_management_type, offsetof(WIFIAuthentication, key_management)},
        { "psk",                 CONF_TYPE_WIFI,     parse_yaml_auth_key_management_type, offsetof(WIFIAuthentication, password)},
        { "method",              CONF_TYPE_WIFI,     parse_yaml_auth_eap_method,          offsetof(WIFIAuthentication, eap_method)},
        { "ca-certificate",      CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, ca_certificate)},
        { "client-certificate",  CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, client_certificate)},
        { "client-key",          CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, client_key)},
        { "client-key-password", CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, client_key_password)},
        { "identity",            CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, identity)},
        { "anonymous-identity",  CONF_TYPE_WIFI,     parse_yaml_string,                   offsetof(WIFIAuthentication, anonymous_identity)},
        { NULL,                  _CONF_TYPE_INVALID, 0,                                   0}
};

static ParserTable parser_match_vtable[] = {
        { "name",                       CONF_TYPE_NETWORK,     parse_yaml_string,                 offsetof(Network, ifname)},
        { "driver",                     CONF_TYPE_NETWORK,     parse_yaml_scalar_or_sequence,     offsetof(Network, driver)},
        { "macaddress",                 CONF_TYPE_NETWORK,     parse_yaml_mac_address,            offsetof(Network, match_mac)},
        { NULL,                         _CONF_TYPE_INVALID,    0,                                 0}
};

static ParserTable parser_network_vtable[] = {
        { "unmanaged",                  CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, unmanaged)},
        { "mtu",                        CONF_TYPE_NETWORK,     parse_yaml_uint32,                       offsetof(Network, mtu)},
        { "arp",                        CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, arp)},
        { "multicast",                  CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, multicast)},
        { "allmulticast",               CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, all_multicast)},
        { "promiscuous",                CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, promiscuous)},
        { "required-for-online",        CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, req_for_online)},
        { "required-family-for-online", CONF_TYPE_NETWORK,     parse_yaml_rf_online,                    offsetof(Network, req_family_for_online)},
        { "activation-mode",            CONF_TYPE_NETWORK,     parse_yaml_activation_policy,            offsetof(Network, activation_policy)},
        { "macaddress",                 CONF_TYPE_NETWORK,     parse_yaml_mac_address,                  offsetof(Network, mac)},
        { "dhcp",                       CONF_TYPE_NETWORK,     parse_yaml_dhcp_type,                    offsetof(Network, dhcp_type)},
        { "dhcp4",                      CONF_TYPE_NETWORK,     parse_yaml_dhcp_type,                    offsetof(Network, dhcp4)},
        { "dhcp6",                      CONF_TYPE_NETWORK,     parse_yaml_dhcp_type,                    offsetof(Network, dhcp6)},
        { "dhcp-identifier",            CONF_TYPE_NETWORK,     parse_yaml_dhcp_client_identifier,       offsetof(Network, dhcp_client_identifier_type)},
        { "lldp",                       CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, lldp)},
        { "emit-lldp",                  CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, emit_lldp)},
        { "accept-ra",                  CONF_TYPE_NETWORK,     parse_yaml_bool,                         offsetof(Network, ipv6_accept_ra)},
        { "link-local",                 CONF_TYPE_NETWORK,     parse_yaml_link_local_type,              offsetof(Network, link_local)},
        { "ipv6-address-generation",    CONF_TYPE_NETWORK,     parse_yaml_ipv6_address_generation_mode, offsetof(Network, ipv6_address_generation)},
        { "ipv6-privacy",               CONF_TYPE_NETWORK,     parse_yaml_ipv6_privacy_extensions,      offsetof(Network, ipv6_privacy)},
        { "ipv6-mtu",                   CONF_TYPE_NETWORK,     parse_yaml_uint32,                       offsetof(Network, ipv6_mtu)},
        { "ntp",                        CONF_TYPE_NETWORK,     parse_yaml_addresses,                    offsetof(Network, ntps)},
        { NULL,                         _CONF_TYPE_INVALID,    0,                                       0}
};

static ParserTable parser_dhcp4_overrides_vtable[] = {
        { "use-dns",            CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_use_dns)},
        { "use-domain",         CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_use_domains)},
        { "use-ntp",            CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_use_ntp)},
        { "use-mtu",            CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_use_mtu)},
        { "use-routes",         CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_use_routes)},
        { "use-hostname",       CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_use_hostname)},
        { "send-hostname",      CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp4_send_hostname)},
        { "route-metric",       CONF_TYPE_NETWORK,     parse_yaml_uint32, offsetof(Network, dhcp4_route_metric)},
        { "hostname",           CONF_TYPE_NETWORK,     parse_yaml_string, offsetof(Network, dhcp4_hostname)},
        { NULL,                _CONF_TYPE_INVALID,    0,                  0}
};

static ParserTable parser_dhcp6_overrides_vtable[] = {
        { "use-dns",            CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp6_use_dns)},
        { "use-domain",         CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp6_use_domains)},
        { "use-ntp",            CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp6_use_ntp)},
        { "use-address",        CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp6_use_address)},
        { "use-hostname",       CONF_TYPE_NETWORK,     parse_yaml_bool,   offsetof(Network, dhcp6_use_hostname)},
        { NULL,                _CONF_TYPE_INVALID,    0,                  0}
};

static ParserTable parser_address_vtable[] = {
        { "label",     CONF_TYPE_NETWORK,     parse_yaml_addresses, offsetof(Network, addresses)},
        { "addresses", CONF_TYPE_NETWORK,     parse_yaml_addresses, offsetof(Network, addresses)},
        { NULL,        _CONF_TYPE_INVALID,    0,                    0}
};

static ParserTable parser_nameservers_vtable[] = {
        { "search",     CONF_TYPE_NETWORK,     parse_yaml_domains,              offsetof(Network, domains)},
        { "addresses",  CONF_TYPE_NETWORK,     parse_yaml_nameserver_addresses, offsetof(Network, nameservers)},
        { NULL,         _CONF_TYPE_INVALID,    0,                               0}
};

static ParserTable parser_route_vtable[] = {
        { "via",                        CONF_TYPE_ROUTE,     parse_yaml_route,       offsetof(Route, gw)},
        { "to",                         CONF_TYPE_ROUTE,     parse_yaml_route,       offsetof(Route, dst)},
        { "from",                       CONF_TYPE_ROUTE,     parse_yaml_address,     offsetof(Route, prefsrc)},
        { "table",                      CONF_TYPE_ROUTE,     parse_yaml_uint32,      offsetof(Route, table)},
        { "type",                       CONF_TYPE_ROUTE,     parse_yaml_route_type,  offsetof(Route, type)},
        { "scope",                      CONF_TYPE_ROUTE,     parse_yaml_route_scope, offsetof(Route, scope)},
        { "metric",                     CONF_TYPE_ROUTE,     parse_yaml_uint32,      offsetof(Route, metric)},
        { "on-link",                    CONF_TYPE_ROUTE,     parse_yaml_bool,        offsetof(Route, onlink)},
        { "congestion-window",          CONF_TYPE_ROUTE,     parse_yaml_uint32,      offsetof(Route, initcwnd)},
        { "advertised-receive-window",  CONF_TYPE_ROUTE,     parse_yaml_uint32,      offsetof(Route, initrwnd)},
        { NULL,                         _CONF_TYPE_INVALID,  0,                      0}
};

static ParserTable parser_routing_policy_rule_vtable[] = {
        { "from",            CONF_TYPE_ROUTING_POLICY_RULE,     parse_yaml_address, offsetof(RoutingPolicyRule, from)},
        { "to",              CONF_TYPE_ROUTING_POLICY_RULE,     parse_yaml_address, offsetof(RoutingPolicyRule, to)},
        { "table",           CONF_TYPE_ROUTING_POLICY_RULE,     parse_yaml_uint32,  offsetof(RoutingPolicyRule, table)},
        { "priority",        CONF_TYPE_ROUTING_POLICY_RULE,     parse_yaml_uint32,  offsetof(RoutingPolicyRule, priority)},
        { "type-of-service", CONF_TYPE_ROUTING_POLICY_RULE,     parse_yaml_uint32,  offsetof(RoutingPolicyRule, tos)},
        { "mark",            CONF_TYPE_ROUTING_POLICY_RULE,     parse_yaml_uint32,  offsetof(RoutingPolicyRule, fwmark)},
        { NULL,              _CONF_TYPE_INVALID,                0,                  0}
};

static int parse_wifi_access_points_config(YAMLManager *m, yaml_document_t *doc, yaml_node_t *node, Network *network) {
        yaml_node_pair_t *entry;

        assert(doc);
        assert(node);

        for (entry = node->data.mapping.pairs.start; entry < node->data.mapping.pairs.top; entry++) {
                yaml_node_t *key, *value;
                ParserTable *p;
                void *v;

                key = yaml_document_get_node(doc, entry->key);
                value = yaml_document_get_node(doc, entry->value);

                if (string_equal(scalar(key), "ssid-name")) {
                        wifi_access_point = new0(WiFiAccessPoint, 1);
                        if (!wifi_access_point)
                                return log_oom();

                        wifi_access_point->auth = new0(WIFIAuthentication, 1);
                        if (!wifi_access_point->auth)
                                return log_oom();

                        wifi_access_point->ssid = g_strdup(scalar(value));
                        if (!network->access_points)
                                network->access_points = g_hash_table_new(g_str_hash, g_str_equal);

                        if (!g_hash_table_insert(network->access_points, wifi_access_point->ssid, wifi_access_point)) {
                                log_warning("Failed to add WiFi access point: %s", scalar(value));
                                return false;
                        }

                        continue;
                }

                p = g_hash_table_lookup(m->wifi_config, scalar(key));
                if (!p)
                        continue;

                v = (uint8_t *)  wifi_access_point->auth + p->offset;
                if (p->parser)
                        (void) p->parser(scalar(key), scalar(value), wifi_access_point, v, doc, value);
        }

        return 0;
}

static int parse_route(GHashTable *config, yaml_document_t *dp, yaml_node_t *node, Network *network) {
        _auto_cleanup_ Route *rt = NULL;
        yaml_node_t *k, *v;
        yaml_node_item_t *i;
        yaml_node_pair_t *p;
        yaml_node_t *n;
        int r;

        assert(config);
        assert(dp);
        assert(node);
        assert(network);

        for (i = node->data.sequence.items.start; i < node->data.sequence.items.top; i++) {
                n = yaml_document_get_node(dp, *i);
                if (n)
                        (void) parse_route(config, dp, n, network);
        }

        for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
                ParserTable *table;
                void *t;

                k = yaml_document_get_node(dp, p->key);
                v = yaml_document_get_node(dp, p->value);

                if (!k && !v)
                        continue;

                table = g_hash_table_lookup(config, scalar(k));
                if (!table)
                        continue;

                if (!rt) {
                        r = route_new(&rt);
                        if (r < 0)
                                return log_oom();
                }

                t = (uint8_t *) rt + table->offset;
                if (table->parser) {
                        (void) table->parser(scalar(k), scalar(v), rt, t, dp, v);
                        network->modified = true;
                }
        }

        if (rt) {
                if (!g_hash_table_insert(network->routes, rt, rt))
                        return -EINVAL;

                network->modified = true;
                steal_pointer(rt);
        }


        return 0;
}

static int parse_address(YAMLManager *m, yaml_document_t *dp, yaml_node_t *node, Network *network, IPAddress **addr) {
        _auto_cleanup_ IPAddress *a = NULL;
        yaml_node_pair_t *p;
        yaml_node_item_t *i;
        yaml_node_t *k, *v;
        yaml_node_t *n;
        int r;

        assert(m);
        assert(dp);
        assert(node);
        assert(network);

        for (i = node->data.sequence.items.start; i < node->data.sequence.items.top; i++) {
                n = yaml_document_get_node(dp, *i);
                if (n)
                        (void) parse_address(m, dp, n, network, addr);
        }

        for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
                k = yaml_document_get_node(dp, p->key);
                v = yaml_document_get_node(dp, p->value);

                if (!k && !v)
                        continue;

                if (!a && !*addr) {
                        a = new0(IPAddress, 1);
                        if (!a)
                                return log_oom();
                }

                if (string_equal(scalar(k), "lifetime")) {
                        free(a->lifetime);
                        a->lifetime = strdup(scalar(v));
                        if (!a->lifetime)
                                return log_oom();
                } else if (string_equal(scalar(k), "label")) {
                        free(a->label);
                        a->label = strdup(scalar(v));
                        if (!a->label)
                                return log_oom();
                } else {
                        _auto_cleanup_ IPAddress *address = NULL;

                        r = parse_ip_from_string(scalar(k), &address);
                        if (r < 0)
                                return r;

                        if (*addr) {
                                if ((*addr)->label)
                                        address->label = strdup((*addr)->label);
                                if ((*addr)->lifetime)
                                        address->lifetime = (*addr)->lifetime;

                                free(*addr);
                                steal_pointer(*addr);
                        }

                        set_add(network->addresses, address);
                        steal_pointer(address);

                        network->modified = true;

                        if (v) {
                                r = parse_address_from_string_and_add(scalar(v), network->addresses);
                                if (r < 0)
                                        continue;
                        }
                }
        }

        if (a) {
                *addr = a;
                steal_pointer(a);
        }

        return 0;
}

static int parse_routing_policy_rule_config(GHashTable *config, yaml_document_t *dp, yaml_node_t *node, Network *network) {
       _cleanup_(routing_policy_rule_freep) RoutingPolicyRule *rule = NULL;
        yaml_node_t *k, *v;
        yaml_node_item_t *i;
        yaml_node_pair_t *p;
        yaml_node_t *n;
        int r;

        assert(config);
        assert(dp);
        assert(node);
        assert(network);

        for (i = node->data.sequence.items.start; i < node->data.sequence.items.top; i++) {
                n = yaml_document_get_node(dp, *i);
                if (n)
                        (void) parse_routing_policy_rule_config(config, dp, n, network);
        }

        for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
                ParserTable *table;
                void *t;

                k = yaml_document_get_node(dp, p->key);
                v = yaml_document_get_node(dp, p->value);

                if (!k && !v)
                        continue;

                table = g_hash_table_lookup(config, scalar(k));
                if (!table)
                        continue;

                if (!rule) {
                        r = routing_policy_rule_new(&rule);
                        if (r < 0)
                                return log_oom();
                }

                t = (uint8_t *) rule + table->offset;
                if (table->parser) {
                        (void) table->parser(scalar(k), scalar(v), rule, t, dp, v);
                        network->modified = true;
                }
        }

        if (rule) {
                if (!g_hash_table_insert(network->routing_policy_rules, rule, rule))
                        return -EINVAL;

                network->modified = true;
                steal_pointer(rule);
        }

        return 0;
}

static int parse_config(GHashTable *config, yaml_document_t *dp, yaml_node_t *node, Network *network) {
        yaml_node_pair_t *p;
        yaml_node_t *k, *v;

        assert(dp);
        assert(node);
        assert(network);

        for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
                ParserTable *table;
                void *t;

                k = yaml_document_get_node(dp, p->key);
                v = yaml_document_get_node(dp, p->value);

                table = g_hash_table_lookup(config, scalar(k));
                if (!table)
                        continue;

                t = (uint8_t *) network + table->offset;
                if (table->parser) {
                        (void) table->parser(scalar(k), scalar(v), network, t, dp, v);
                        network->modified = true;
                }
        }

        return 0;
}

static int parse_network_config(YAMLManager *m, yaml_document_t *dp, yaml_node_t *node, Network *network) {
        yaml_node_pair_t *p;
        yaml_node_t *k, *v;
        int r;

        assert(m);
        assert(dp);
        assert(node);
        assert(network);

        for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
                ParserTable *table, *link_table;
                void *t;

                k = yaml_document_get_node(dp, p->key);
                v = yaml_document_get_node(dp, p->value);

                table = g_hash_table_lookup(m->network_config, scalar(k));
                if (!table) {
                        if (string_equal(scalar(k), "match")) {
                                r = parse_config(m->match_config, dp, v, network);
                                if (r < 0)
                                        return r;
                        } else if (string_equal(scalar(k), "dhcp4-overrides")) {
                                r = parse_config(m->dhcp4_config, dp, v, network);
                                if (r < 0)
                                        return r;
                        } else if (string_equal(scalar(k), "dhcp6-overrides")) {
                                r = parse_config(m->dhcp6_config, dp, v, network);
                                if (r < 0)
                                        return r;
                        } else if (string_equal(scalar(k), "addresses")) {
                                IPAddress *a = NULL;

                                r = parse_address(m, dp, v, network, &a);
                                if (r < 0)
                                        return r;
                        } else if (string_equal(scalar(k), "routes")) {
                                r = parse_route(m->route_config, dp, v, network);
                                if (r < 0)
                                        return r;
                        } else if (string_equal(scalar(k), "routing-policy")) {
                                r = parse_routing_policy_rule_config(m->routing_policy_rule_config, dp, v, network);
                                if (r < 0)
                                        return r;
                        } else if (string_equal(scalar(k), "nameservers"))
                                r = parse_config(m->nameserver_config, dp, v, network);
                        else
                                r = parse_network_config(m, dp, v, network);
                        if (r < 0)
                                return r;

                        /* .link  */
                        link_table = g_hash_table_lookup(m->link_config, scalar(k));
                        if (link_table) {
                                if (!network->link) {
                                        NetDevLink *l;

                                        r = netdev_link_new((NetDevLink **) &network->link);
                                        if (r < 0)
                                                return r;

                                        l = network->link;
                                        l->parser_type = PARSER_TYPE_YAML;
                                }

                                t = (uint8_t *) network->link + link_table->offset;
                                if (link_table->parser)
                                        (void) link_table->parser(scalar(k), scalar(v), link, t, dp, v);
                        }

                        continue;
                }

                t = (uint8_t *) network + table->offset;
                if (table->parser) {
                        (void) table->parser(scalar(k), scalar(v), network, t, dp, v);
                        network->modified = true;
                }
        }

        return 0;
}

int parse_ethernet_config(YAMLManager *m, yaml_document_t *dp, yaml_node_t *node, Networks *nets) {
        yaml_node_pair_t *p;
        yaml_node_t *n;
        int r;

        assert(m);
        assert(dp);
        assert(node);
        assert(nets);

        for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
                _cleanup_(network_freep) Network *net = NULL;
                n = yaml_document_get_node(dp, p->key);

                r = network_new(&net);
                if (r < 0)
                        return r;

                net->parser_type = PARSER_TYPE_YAML;
                net->ifname = strdup(scalar(n));
                if (!net->ifname)
                        return log_oom();

                n = yaml_document_get_node(dp, p->value);
                if (n)
                        (void) parse_network_config(m, dp, n, net);

                if (!g_hash_table_insert(nets->networks, (gpointer *) net->ifname, (gpointer *) net))
                        return log_oom();

                steal_pointer(net);
        }

        return 0;
}

int yaml_register_network(YAMLManager *m) {
        assert(m);
        assert(m->match_config);
        assert(m->network_config);
        assert(m->dhcp4_config);
        assert(m->dhcp6_config);
        assert(m->address_config);
        assert(m->routing_policy_rule_config);
        assert(m->route_config);
        assert(m->nameserver_config);
        assert(m->wifi_config);

        for (size_t i = 0; parser_match_vtable[i].key; i++) {
               if (!g_hash_table_insert(m->match_config, (void *) parser_match_vtable[i].key, &parser_match_vtable[i])) {
                        log_warning("Failed add key='%s' to match table", parser_match_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_network_vtable[i].key; i++) {
               if (!g_hash_table_insert(m->network_config, (void *) parser_network_vtable[i].key, &parser_network_vtable[i])) {
                        log_warning("Failed add key='%s' to network table", parser_network_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_dhcp4_overrides_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->dhcp4_config, (void *) parser_dhcp4_overrides_vtable[i].key, &parser_dhcp4_overrides_vtable[i])) {
                        log_warning("Failed add key='%s' to dhcp4 table", parser_dhcp4_overrides_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_dhcp6_overrides_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->dhcp6_config, (void *) parser_dhcp6_overrides_vtable[i].key, &parser_dhcp6_overrides_vtable[i])) {
                        log_warning("Failed add key='%s' to dhcp6 table", parser_dhcp6_overrides_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_address_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->address_config, (void *) parser_address_vtable[i].key, &parser_address_vtable[i])) {
                        log_warning("Failed add key='%s' to address table", parser_address_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_routing_policy_rule_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->routing_policy_rule_config, (void *) parser_routing_policy_rule_vtable[i].key, &parser_routing_policy_rule_vtable[i])) {
                        log_warning("Failed add key='%s' to routing policy rule table", parser_routing_policy_rule_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_route_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->route_config, (void *) parser_route_vtable[i].key, &parser_route_vtable[i])) {
                        log_warning("Failed add key='%s' to route table", parser_route_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_nameservers_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->nameserver_config, (void *) parser_nameservers_vtable[i].key, &parser_nameservers_vtable[i])) {
                        log_warning("Failed add key='%s' to nameserver table", parser_nameservers_vtable[i].key);
                        return -EINVAL;
                }
        }

        for (size_t i = 0; parser_wifi_vtable[i].key; i++) {
                if (!g_hash_table_insert(m->wifi_config, (void *) parser_wifi_vtable[i].key, &parser_wifi_vtable[i])) {
                        log_warning("Failed add key='%s' to wifi table", parser_wifi_vtable[i].key);
                        return -EINVAL;
                }
        }

        return 0;
}
