# network-config-manager

### What is nmctl

The network-config-manager `nmctl` allows to configure and introspect the state of the network links as seen by [systemd-networkd](https://www.freedesktop.org/software/systemd/man/systemd-networkd.service.html). nmctl can be used to query and configure interfaces for Address, Routes, Gateways and also hostname, DNS, NTP or Domain. nmctl uses [sd-bus](http://0pointer.net/blog/the-new-sd-bus-api-of-systemd.html), [libudev](https://www.freedesktop.org/software/systemd/man/libudev.html) APIs to interact with [systemd](https://www.freedesktop.org/wiki/Software/systemd), [systemd-networkd](https://www.freedesktop.org/software/systemd/man/systemd-networkd.service.html), [systemd-resolved](https://www.freedesktop.org/software/systemd/man/systemd-resolved.service.html), [systemd-hostnamed](https://www.freedesktop.org/software/systemd/man/systemd-hostnamed.service.html), and [systemd-timesyncd](https://www.freedesktop.org/software/systemd/man/systemd-timesyncd.service.html) via dbus. nmctl uses networkd verbs to explain output. nmctl can generate configurations for required network interfaces from YAML description. It also understands kernel command line specified in [dracut](http://man7.org/linux/man-pages/man7/dracut.cmdline.7.html)'s network configuration format and can generate systemd-networkd's configuration while the system boots and will persist between reboots.

### Features

Configure
  - Static IPv4 and IPv6 Address, Routes, Gateway.
  - DHCP type (IPv4/IPv6), DHCP4 Client Identifier, UseMTU/UseDNS/UseDomains/UseNTP/UseRoutes.
  - LLDP, Link Local Addressing, IPv4LLRoute, LLMNR.
  - DNS, Domains and NTP.
  - Link's MAC, MTU.
  - Hostname.

  Please see [systemd.network](https://www.freedesktop.org/software/systemd/man/systemd.network.html) for more information.

Gererates networkd unit configs from
 - [YML](https://yaml.org) file.
 - [Dracut](https://mirrors.edge.kernel.org/pub/linux/utils/boot/dracut/dracut.html#dracutkernel7) kernel command line network config.

Introspect
 - Links.
 - DNS and Domains.
 - Hostname.

### Dependencies

 `meson, ninja-build, systemd-devel, libudev-devel, libyaml-devel, glib2-devel, python3-sphinx`

### Building from source.

On Photon
```bash
➜  ~ tdnf install -y build-essential
➜  ~ tdnf install meson ninja-build systemd-devel libudev-devel libyaml-devel glib2-devel python3-sphinx
➜  ~ meson build
➜  ~ ninja -C build
➜  ~ sudo ninja -C build install
```
On Fedora/CentOS/RHEL
```bash
➜  ~ sudo dnf group install 'Development Tools'
➜  ~ sudo dnf install meson ninja-build systemd-devel libudev-devel libyaml-devel glib2-devel python3-sphinx
➜  ~ meson build
➜  ~ ninja -C build
➜  ~ sudo ninja -C build install
```

### Use cases

```bash
➜  ~ nmctl --help
```
### Gererate network config from yml file:

nmctl can generate configurations for required network interfaces from YAML description. Configuration written to disk under `/etc/systemd/network` will persist between reboots. When `netmgr-yaml-generator.service` is enabled it reads yaml files from `/etc/network-config-manager/yaml` and generates systemd-networkd configuration files.

`nmctl` uses similar format as defined by [different YAML format](https://curtin.readthedocs.io/en/latest/topics/networking.html).

#### Using DHCP:

To set the interface named `eth1` get an address via DHCP4 and client identifier as `mac` create a YAML file with the following:

```yml
 network:
  interface:
     name: eth1
     dhcp: ipv4
     dhcp-client-identifier: mac
 ```

#### Static configuration
To set a static IP address, use the addresses key, which takes a list of (IPv4 or IPv6), addresses along with the subnet prefix length (e.g. /24). Gateway and DNS information can be provided as well:

```yml
 network:
  interface:
     name: eth1
     gateway: 192.168.1.1/24
     gateway-onlink: yes
     nameservers: [192.168.0.1, 8.8.8.8]
     ntps: [192.168.0.2, 8.8.8.1]
     addresses:
       - 192.168.1.5/24
```

#### Directly connected gateway
```yml
 network:
  interface:
     name: eth1
     addresses: [ 192.168.1.45/24 ]
     gateway: 192.168.1.1
     gateway-onlink: true
 ```

#### Multiple addresses on a single interface

```yml
 network:
  interface:
     name: eth1
     addresses: [ 192.168.1.45/24, 192.168.1.46 ]
     gateway: 192.168.1.1
 ```
#### Using multiple addresses with multiple gateways and DHCP4
```yml
 network:
  interface:
     name: eth1
     mtu : 1200
     mac-address: 00:0c:29:3a:bc:89
     match-mac-address: 00:0c:29:3a:bc:89
     dhcp: yes
     dhcp-client-identifier: mac
     lldp: yes
     link-local: yes
     ipv6-accept-ra: yes
     use-mtu: yes
     use-domain: yes
     gateway: 192.168.1.1/24
     gateway-onlink: yes
     nameservers: [192.168.0.1, 8.8.8.8]
     ntps: [192.168.0.2, 8.8.8.1]
     addresses:
       - 9.0.0.9/24
       - 10.0.0.10/24
       - 11.0.0.11/24
     routes:
       - to: 0.0.0.0/0
         via: 9.0.0.1
       - to: 0.0.0.1/0
         via: 9.0.0.2
```
### Generate WiFi config from yml file

nmctl can generate [WPA Supplicant](https://w1.fi/wpa_supplicant/) configuration from yaml file. When a yml file with wifi
configuration are found it generates a confiration file found in ```/etc/network-config-manager/wpa_supplicant_photon_os.conf``` which is understood by  `wpa_supplicant`.

#### Connecting to a WPA Personal wireless network

```yml
 network:
  interface:
     name: wlan1
     dhcp: yes
     use-dns: no
     use-mtu: yes
     use-domain: yes
     gateway: 192.168.1.1/24
     gateway-onlink: yes
     nameservers: [192.168.0.1, 8.8.8.8]
     access-points:
         - ssid-name: "network_ssid_name1"
           password: "test123"
         - ssid-name: "network_ssid_name2"
           password: "test456"
```

#### WPA Enterprise wireless networks

```yml
 network:
  interface:
     name: wlan0
     dhcp: yes
     access-points:
         - ssid-name: "network_ssid_name1"
           password: "test123"
           method: ttls
           anonymous-identity: "@test.example.com"
           identity: "max@internal.example.com"
```

#### WPA-EAP and TLS:

```yml

 network:
  interface:
     name: wlan1
     dhcp: yes
     access-points:
         - ssid-name: "network_ssid_name1"
           key-management: eap
           method: tls
           anonymous-identity: "@test.example.com"
           identity: "cert-max@test.example.com"
           ca-certificate: /etc/ssl/cust-cacrt.pem
           client-certificate: /etc/ssl/cust-crt.pem
           client-key: /etc/ssl/cust-key.pem
           client-key-password: "QZTrSEtq:h_d.W7_"
```
### Generate network config from kernel command line

nmctl understands kernel command line specified in [dracut's](https://mirrors.edge.kernel.org/pub/linux/utils/boot/dracut/dracut.html#dracutkernel7) network configuration format and can generate [systemd-networkd](https://www.freedesktop.org/software/systemd/man/systemd-networkd.service.html)'s configuration while the system boots and will persist between reboots.

```bash
 Network
       ip={dhcp|on|any|dhcp6|auto6}
           dhcp|on|any: get ip from dhcp server from all interfaces. If root=dhcp, loop
           sequentially through all interfaces (eth0, eth1, ...) and use the first with a valid
           DHCP root-path.

           auto6: IPv6 autoconfiguration

           dhcp6: IPv6 DHCP

       ip=<interface>:{dhcp|on|any|dhcp6|auto6}
           dhcp|on|any|dhcp6: get ip from dhcp server on a specific interface

           auto6: do IPv6 autoconfiguration

           This parameter can be specified multiple times.

       ip=<client-IP>:[ <server-id>]:<gateway-IP>:<netmask>:<client_hostname>:<interface>:{none|off}
           explicit network configuration.

       ifname=<interface>:<MAC>
           Assign network device name <interface> (ie eth0) to the NIC with MAC <MAC>. Note
           letters in the MAC-address must be lowercase!  Note: If you use this option you must
           specify an ifname= argument for all interfaces used in ip= or fcoe= arguments.  This
           parameter can be specified multiple times.

       nameserver=<IP>[nameserver=<IP> ...]
           specify nameserver(s) to use
```

```bash
➜  ~ cat /proc/cmdline
   BOOT_IMAGE=/boot/vmlinuz-4.19.52-2.ph3-esx root=PARTUUID=ebf01b6d-7e9c-4345-93f4-122f44eb2726
   init=/lib/systemd/systemd rcupdate.rcu_expedited=1 rw systemd.show_status=0 quiet noreplace-smp
   cpu_init_udelay=0 net.ifnames=0 plymouth.enable=0 systemd.legacy_systemd_cgroup_controller=yes
   ip=dhcp
```

`network-config-manager-generator.service` is a [oneshot](https://www.freedesktop.org/software/systemd/man/systemd.service.html#Type=) type systemd service unit which runs while system boots. It parses the kernel command line and generates networkd config in ```/etc/systemd/network```.

```bash
➜  ~ sudo systemctl enable network-config-manager-generator.service
Created symlink /etc/systemd/system/network.target.wants/network-config-manager-generator.service → /usr/lib/systemd/system/network-config-manager-generator.service.

```

**Note**: Only IPv4 configuration is supported for static configs.

### Contributing

The network-config-manager project team welcomes contributions from the community. Before you start working with network-config-manager, please
read our [Developer Certificate of Origin](https://cla.vmware.com/dco). All contributions to this repository must be
signed as described on that page. Your signature certifies that you wrote the patch or have the right to pass it on
as an open-source patch. For more detailed information, refer to [CONTRIBUTING.md](CONTRIBUTING.md).

License
----

[Apache-2.0](https://spdx.org/licenses/Apache-2.0.html)
