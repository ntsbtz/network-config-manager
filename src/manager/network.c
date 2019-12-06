/* SPDX-License-Identifier: Apache-2.0
 * Copyright © 2019 VMware, Inc.
 */

#include <assert.h>
#include <glib.h>
#include <net/if.h>
#include <linux/if.h>
#include <net/ethernet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "alloc-util.h"
#include "dracut-parser.h"
#include "macros.h"
#include "network-manager.h"
#include "network.h"
#include "networkd-api.h"
#include "parse-util.h"
#include "string-util.h"
#include "log.h"

static const char *const dhcp_modes[_DHCP_MODE_MAX] = {
        [DHCP_MODE_NO]   = "no",
        [DHCP_MODE_YES]  = "yes",
        [DHCP_MODE_IPV4] = "ipv4",
        [DHCP_MODE_IPV6] = "ipv6",
};

const char *dhcp_modes_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(dhcp_modes))
                return NULL;

        return dhcp_modes[id];
}

int dhcp_name_to_mode(char *name) {
        int i;

        assert(name);

        for (i = DHCP_MODE_NO; i < (int) ELEMENTSOF(dhcp_modes); i++)
                if (string_equal_fold(name, dhcp_modes[i]))
                        return i;

        return _DHCP_MODE_INVALID;
}

static const char *const dhcp_client_identifier[_DHCP_CLIENT_IDENTIFIER_MAX] = {
        [DHCP_CLIENT_IDENTIFIER_MAC]       = "mac",
        [DHCP_CLIENT_IDENTIFIER_DUID]      = "duid",
        [DHCP_CLIENT_IDENTIFIER_DUID_ONLY] = "duid-ony",
};

const char *dhcp_client_identifier_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(dhcp_client_identifier))
                return NULL;

        return dhcp_client_identifier[id];
}

int dhcp_client_identifier_to_mode(char *name) {
        int i;

        assert(name);

        for (i = DHCP_CLIENT_IDENTIFIER_MAC; i < (int) ELEMENTSOF(dhcp_client_identifier); i++)
                if (string_equal_fold(name, dhcp_client_identifier[i]))
                        return i;

        return _DHCP_CLIENT_IDENTIFIER_INVALID;
}

static const char *const dhcp_client_duid_type [_DHCP_CLIENT_DUID_TYPE_MAX] =  {
        [DHCP_CLIENT_DUID_TYPE_LINK_LAYER_TIME] = "link-layer-time",
        [DHCP_CLIENT_DUID_TYPE_VENDOR]          = "vendor",
        [DHCP_CLIENT_DUID_TYPE_LINK_LAYER]      = "link-layer",
        [DHCP_CLIENT_DUID_TYPE_UUID]            = "uuid",
};

const char *dhcp_client_duid_type_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(dhcp_client_duid_type))
                return NULL;

        return dhcp_client_duid_type[id];
}

int dhcp_client_duid_type_to_mode(char *name) {
        int i;

        assert(name);

        for (i = DHCP_CLIENT_DUID_TYPE_LINK_LAYER_TIME; i < (int) ELEMENTSOF(dhcp_client_duid_type); i++)
                if (string_equal_fold(name, dhcp_client_duid_type[i]))
                        return i;

        return _DHCP_CLIENT_DUID_TYPE_INVALID;
}

static const char *const link_local_address_type[_LINK_LOCAL_ADDRESS_MAX] =  {
        [LINK_LOCAL_ADDRESS_YES]           = "yes",
        [LINK_LOCAL_ADDRESS_NO]            = "no",
        [LINK_LOCAL_ADDRESS_IPV4]          = "ipv4",
        [LINK_LOCAL_ADDRESS_IPV6]          = "ipv6",
        [LINK_LOCAL_ADDRESS_FALLBACK]      = "fallback",
        [LINK_LOCAL_ADDRESS_IPV4_FALLBACK] = "ipv4-fallback",
};

const char *link_local_address_type_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(link_local_address_type))
                return NULL;

        return link_local_address_type[id];
}

int link_local_address_type_to_mode(const char *name) {
        int i;

        assert(name);

        for (i = LINK_LOCAL_ADDRESS_YES; i < (int) ELEMENTSOF(link_local_address_type); i++)
                if (string_equal_fold(name, link_local_address_type[i]))
                        return i;

        return _LINK_LOCAL_ADDRESS_INVALID;
}

static const char *const auth_key_management_type[_AUTH_KEY_MANAGEMENT_MAX] =  {
        [AUTH_KEY_MANAGEMENT_NONE]    = "password",
        [AUTH_KEY_MANAGEMENT_WPA_PSK] = "psk",
        [AUTH_KEY_MANAGEMENT_WPA_EAP] = "eap",
        [AUTH_KEY_MANAGEMENT_8021X]   = "8021x",
};

const char *auth_key_management_type_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(auth_key_management_type))
                return NULL;

        return auth_key_management_type[id];
}

int auth_key_management_type_to_mode(const char *name) {
        int i;

        assert(name);

        for (i = AUTH_KEY_MANAGEMENT_NONE; i < (int) ELEMENTSOF(auth_key_management_type); i++)
                if (string_equal_fold(name, auth_key_management_type[i]))
                        return i;

        return _AUTH_KEY_MANAGEMENT_INVALID;
}

static const char* const auth_eap_method_type[_AUTH_EAP_METHOD_MAX] =  {
        [AUTH_EAP_METHOD_NONE] = "none",
        [AUTH_EAP_METHOD_TLS]  = "tls",
        [AUTH_EAP_METHOD_PEAP] = "peap",
        [AUTH_EAP_METHOD_TTLS] = "ttls",
};

const char *auth_eap_method_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(auth_eap_method_type))
                return NULL;

        return auth_eap_method_type[id];
}

int auth_eap_method_to_mode(const char *name) {
        int i;

        assert(name);

        for (i = AUTH_EAP_METHOD_NONE; i < (int) ELEMENTSOF(auth_eap_method_type); i++)
                if (string_equal_fold(name, auth_eap_method_type[i]))
                        return i;

        return _AUTH_EAP_METHOD_INVALID;
}

int network_new(Network **ret) {
        _cleanup_free_ Network *n = NULL;
        int r;

        n = new0(Network, 1);
        if (!n)
                return log_oom();

        *n = (Network) {
                .dhcp_type = _DHCP_MODE_INVALID,
                .use_mtu = -1,
                .use_dns = -1,
                .use_domains = -1,
                .gateway_onlink = -1,
                .lldp = -1,
                .ipv6_accept_ra = -1,
                .dhcp_client_identifier_type = _DHCP_CLIENT_IDENTIFIER_INVALID,
                .link_local = _LINK_LOCAL_ADDRESS_INVALID,
                .parser_type = _PARSER_TYPE_INVALID,
        };

        r = set_new(&n->addresses, NULL, NULL);
        if (r < 0)
                return r;

        r = set_new(&n->nameservers, NULL, NULL);
        if (r < 0)
                return r;

        r = set_new(&n->ntps, NULL, NULL);
        if (r < 0)
                return r;

        *ret = steal_pointer(n);

        return 0;
}

static int wifi_access_point_unref (void *key, void *value, void *user_data) {
        WiFiAccessPoint *ap = value;

        if (!ap)
                return 0;

        free(ap->ssid);
        free(ap->auth->identity);
        free(ap->auth->anonymous_identity);
        free(ap->auth->password);
        free(ap->auth->ca_certificate);
        free(ap->auth->client_certificate);
        free(ap->auth->client_key);
        free(ap->auth->client_key_password);

        free(ap->auth);
        free(ap);

        return 0;
}

void network_unrefp(Network **n) {
        if (n && *n) {
                set_unrefp(&(*n)->addresses);
                set_unrefp(&(*n)->ntps);
                set_unrefp(&(*n)->nameservers);

                if ((*n)->access_points) {
                        g_hash_table_foreach_steal((*n)->access_points, wifi_access_point_unref, NULL);
                        g_hash_table_destroy((*n)->access_points);
                }

                g_free((*n)->ifname);
                g_free((*n)->mac);
                g_free((*n)->match_mac);
                g_free((*n)->hostname);
                g_free((*n)->gateway);
                g_free(*n);
        }
}

void g_network_free (gpointer data) {
        Network *n;

        n = data;
        network_unrefp(&n);
}

int parse_address_from_string_and_add(const char *s, Set *a) {
        _cleanup_free_ IPAddress *address = NULL;
        _cleanup_free_ char *p = NULL;
        int r;

        if (set_contains(a, (void *) s))
                return -EEXIST;

        r = parse_ip_from_string(s, &address);
        if (r < 0)
                return r;

        p = g_strdup(s);
        if (!p)
                return log_oom();

        set_add(a, p);
        p = NULL;

        return 0;
}

static void append_wpa_auth_conf(const WIFIAuthentication *auth, GString *s) {
        assert(s);
        assert(auth);

        switch (auth->key_management) {
        case AUTH_KEY_MANAGEMENT_NONE:
                break;
        case AUTH_KEY_MANAGEMENT_WPA_PSK:
                g_string_append(s, "        key_mgmt=WPA-PSK\n");
                break;
        case AUTH_KEY_MANAGEMENT_WPA_EAP:
                g_string_append(s, "        key_mgmt=WPA-EAP\n");
                break;
        case AUTH_KEY_MANAGEMENT_8021X:
                g_string_append(s, "        key_mgmt=IEEE8021X\n");
                break;
        default:
                break;
        }

        switch (auth->eap_method) {
        case AUTH_EAP_METHOD_NONE:
                break;
        case AUTH_EAP_METHOD_TLS:
                g_string_append(s, "        eap=TLS\n");
                break;
        case AUTH_EAP_METHOD_PEAP:
                g_string_append(s, "        eap=PEAP\n");
                break;
        case AUTH_EAP_METHOD_TTLS:
                g_string_append(s, "        eap=TTLS\n");
                break;
        default:
                break;
        }

        if (auth->identity)
                g_string_append_printf(s, "        identity=\"%s\"\n", auth->identity);

        if (auth->anonymous_identity)
                g_string_append_printf(s, "        anonymous_identity=\"%s\"\n", auth->anonymous_identity);

        if (auth->password) {
                if (auth->key_management == AUTH_KEY_MANAGEMENT_WPA_PSK)
                        g_string_append_printf(s, "        psk=\"%s\"\n", auth->password);
                else
                        g_string_append_printf(s, "        password=\"%s\"\n", auth->password);
        }

        if (auth->ca_certificate)
                g_string_append_printf(s, "        ca_cert=\"%s\"\n", auth->ca_certificate);

        if (auth->client_certificate)
                g_string_append_printf(s, "        client_cert=\"%s\"\n", auth->client_certificate);

        if (auth->client_key)
                g_string_append_printf(s, "        private_key=\"%s\"\n", auth->client_key);

        if (auth->client_key_password)
                g_string_append_printf(s, "        private_key_passwd=\"%s\"\n", auth->client_key_password);
}

static void append_access_points(gpointer key, gpointer value, gpointer userdata) {
        GString *config = userdata;
        WiFiAccessPoint *ap = value;

        g_string_append(config, "network={\n");
        g_string_append_printf(config, "        ssid=\"%s\"\n", ap->ssid);

        append_wpa_auth_conf(ap->auth, config);

        g_string_append(config, "}\n\n");
}

int generate_wifi_config(Network *n, GString **ret) {
        _cleanup_(g_string_unrefp) GString *config = NULL;

        assert(n);

        config = g_string_new(NULL);
        if (!config)
                return log_oom();

        g_string_append(config, "# WPA Supplicant Configuration\n"
                                "# this goes in /etc/net-manager/wpa_supplicant.conf on Photon OS\n"
                                "# chown root, chmod 600 \n\n");
        g_string_append(config, "# allow frontend (e.g., wpa_cli) to be used by all users in 'wheel' group\n"
                                "ctrl_interface=DIR=/run/wpa_supplicant GROUP=wheel\n"
                                "update_config=1\n\n");

        g_hash_table_foreach(n->access_points, append_access_points, config);

        *ret = steal_pointer(config);

        return 0;
}

static void append_routes(gpointer key, gpointer value, gpointer userdata) {
        _cleanup_free_ char *gateway = NULL, *destination = NULL;
        GString *config = userdata;
        Route *route = value;

        if (ip_is_null(&route->destination) && ip_is_null(&route->gw))
                return;

        g_string_append(config, "\n[Route]\n");

        if (!ip_is_null(&route->destination)) {
                (void) ip_to_string(AF_INET, &route->destination, &destination);
                g_string_append_printf(config, "Destination=%s\n", destination);
        }

        if (!ip_is_null(&route->gw)) {
                (void) ip_to_string(AF_INET, &route->gw, &gateway);
                g_string_append_printf(config, "Gateway=%s\n", gateway);
        }
}

static void append_nameservers(gpointer key, gpointer value, gpointer userdata) {
        GString *config = userdata;

        g_string_append_printf(config, "%s ", (char *) key);
}

static void append_ntp(gpointer key, gpointer value, gpointer userdata) {
        GString *config = userdata;

        g_string_append_printf(config, "%s ", (char *) key);
}

static void append_addresses(gpointer key, gpointer value, gpointer userdata) {
        GString *config = userdata;

        g_string_append(config, "\n[Address]\n");
        g_string_append_printf(config, "Address=%s\n", (char *) key);
}

int generate_network_config(Network *n, GString **ret) {
        _cleanup_(g_string_unrefp) GString *config = NULL;
        _cleanup_free_ char *gateway = NULL;

        assert(n);

        config = g_string_new(NULL);
        if (!config)
                return log_oom();

        g_string_append(config, "[Match]\n");
        if (n->ifname)
                g_string_append_printf(config, "Name=%s\n", n->ifname);

        if (n->match_mac)
                g_string_append_printf(config, "MACAddress=%s\n", n->match_mac);

        g_string_append(config, "\n");

        if (n->mtu > 0 || n->mac) {
                g_string_append(config, "[Link]\n");

                if (n->mtu > 0)
                        g_string_append_printf(config, "MTUBytes=%d\n", n->mtu);

                if (n->mac)
                        g_string_append_printf(config, "MACAddress=%s\n", n->mac);

                g_string_append(config, "\n");
        }

        g_string_append(config, "[Network]\n");

        if (n->dhcp_type != _DHCP_MODE_INVALID) {
                if (n->parser_type == PARSER_TYPE_YAML)
                        g_string_append_printf(config, "DHCP=%s\n", dhcp_modes_to_name(n->dhcp_type));
                else
                        g_string_append_printf(config, "DHCP=%s\n", dracut_to_networkd_dhcp_mode_to_name(n->dhcp_type));
        }

        if (n->lldp != -1)
                g_string_append_printf(config, "LLDP=%s\n", bool_to_string(n->lldp));

        if (n->link_local != _LINK_LOCAL_ADDRESS_INVALID)
                g_string_append_printf(config, "LinkLocalAddressing=%s\n", link_local_address_type_to_name(n->link_local));

        if (n->ipv6_accept_ra != -1)
                g_string_append_printf(config, "IPv6AcceptRA=%s\n", bool_to_string(n->ipv6_accept_ra));

        if (n->nameservers && set_size(n->nameservers) > 0) {
                g_string_append(config, "DNS=");
                set_foreach(n->nameservers, append_nameservers, config);
                g_string_append(config, "\n");
       }

        if (n->ntps && set_size(n->ntps) > 0) {
                g_string_append(config, "NTP=");
                set_foreach(n->ntps, append_ntp, config);
                g_string_append(config, "\n");
        }

        if (n->use_dns != -1 || n->use_domains != -1 || n->use_mtu != -1 || n->dhcp_client_identifier_type != _DHCP_CLIENT_IDENTIFIER_INVALID) {
                g_string_append(config, "\n[DHCP]\n");

                if (n->dhcp_client_identifier_type != _DHCP_CLIENT_IDENTIFIER_INVALID)
                        g_string_append_printf(config, "ClientIdentifier=%s\n", dhcp_client_identifier_to_name(n->dhcp_client_identifier_type));

                if (n->use_dns != -1)
                        g_string_append_printf(config, "UseDNS=%s\n", bool_to_string(n->use_dns));

                if (n->use_domains != -1)
                        g_string_append_printf(config, "UseDomains=%s\n", bool_to_string(n->use_domains));

                if (n->use_mtu != -1)
                        g_string_append_printf(config, "UseMTU=%s\n", bool_to_string(n->use_mtu));
        }

        if (n->addresses && set_size(n->addresses) > 0)
                set_foreach(n->addresses, append_addresses, config);

        if (n->gateway && !ip_is_null(n->gateway)) {
                g_string_append(config, "\n[Route]\n");

                (void) ip_to_string_prefix(AF_INET, n->gateway, &gateway);
                g_string_append_printf(config, "Gateway=%s\n", gateway);

                if (n->gateway_onlink != -1)
                        g_string_append_printf(config, "GatewayOnlink=%s\n", bool_to_string(n->gateway_onlink));
        }

        if (n->routes)
                g_hash_table_foreach(n->routes, append_routes, config);

        *ret = steal_pointer(config);

        return 0;
}
