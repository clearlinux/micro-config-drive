#Network configuration with micro-config-drive (ucd)

##Goal of this document

As most cloud-based VMs and containers are deployed in a typical
DHCP-enabled environment, and all major Linux distributions are by
default configured to use DHCP, there is very little need for users
to delve into network configuration details in most cases.

However, in certain environments and more complex uses cases, users
may wish to manually configure networking to extend the basic default
network setup, or enable extra functionality.

Ucd does not provide any direct functionality to perform network
configuration changes, but indirectly it allows the user to do almost
everything that the underlying Linux OS allows through 2 simple
mechanisms. This document describes those and provides a few basic
clues and pointers that demonstrate this functionality.


##Introduction

Micro-config-drive does not offer any specific functionality to
facilitate network configuration, but offers various generic tools
that make this relatively easy to do.

On Linux distributions that use `systemd` and `systemd-networkd`,
one can provide simple network configuration files to `systemd-networkd`
that will instruct it to set up the network as you need.

Other distributions may rely on NetworkManager or other network
configuration services. In general, these will work in a similar way
but may require use of the `runcmd` directive instead. These
services are not covered by this document.


##General Use.

As systemd-networkd uses plain text configuration files, we use the
cloud-config directive `write_files` to output these networkd
configuration files. This can be done by including the following text
in a user data file:

```
#cloud-config
write_files:
  -
    path: /etc/systemd/network/<name1>.<network|link|netdev>
    content: |
       <content of networkd config file>
```

We can repeat sections like these in case we want to provide multiple
configuration files to networkd:

```
#cloud-config
write_files:
  -
    path: /etc/systemd/network/<name1>.<network|link|netdev>
    content: |
       <content of networkd 1 config file>
  -
    path: /etc/systemd/network/<name2>.<network|link|netdev>
    content: |
       <content of networkd 2 config file>
```

Once the proper network configuration files are written out, we may have
to restart the network daemon as follows:

```
#cloud-config
service:
    restart: systemd-networkd
```

Combined, we just append these two parts to have it all written and
executed immediately in the order provided:

```
#cloud-config
write_files:
  -
    path: /etc/systemd/network/<name1>.<network|link|netdev>
    content: |
       <content of networkd 1 config file>
  -
    path: /etc/systemd/network/<name2>.<network|link|netdev>
    content: |
       <content of networkd 2 config file>
service:
    restart: systemd-networkd
```


##Typical Use Cases.

###Dynamic IP Address.

Most distributions provide default networking configurations for DHCP
networking, and they usually include a standard DHCP configuration file
that largely look as follows:

`/usr/lib/systemd/network/80-dhcp.network`:

```
[Match]
Name=en*

[Network]
DHCP=both

[DHCP]
UseDomains=yes
UseMTU=yes
```

###Static IP Address.

If one needs to use a static IP address, this can be done as follows:

`/etc/systemd/network/enp1s0.network`:

```
[Match]
Name=enp1s0

[Network]
Address=10.0.0.5/24
Gateway=10.0.0.1
```


###Creating a bridged interface.

Creating a bridge creates a new virtual netdev, so we have to create
a `.netdev` file, and declare the bridge interface in it.

`/etc/systemd/network/br0.netdev`:

```
[NetDev]
Name=br0
Kind=bridge
```

Subsequently, we can add any ethernet device to this bridge:

`/etc/systemd/network/en.netdev`:

```
[Match]
Name=en*

[Network]
Bridge=br0
```

And finally, we can instruct the system to get a DHCP lease and
assign it to the bridge interface:


`/etc/systemd/network/br0.network`:

```
[Match]
Name=br0

[Network]
DHCP=both
```

All combined, this would look as follows in YAML:

```
#cloud-config
write_files:
  -
    path: /etc/systemd/network/br0.netdev
    content: |
        [NetDev]
        Name=br0
        Kind=bridge
  -
    path: /etc/systemd/network/en.netdev
    content: |
        [Match]
        Name=en*
        
        [Network]
        Bridge=br0
  -
    path: /etc/systemd/network/br0.network
    content: |
        [Match]
        Name=br0
        
        [Network]
        DHCP=both
service:
    restart: systemd-networkd
```


##See Also.

The following documentation is not exhaustive and covers a few, more
common use cases. Additional documentation is available on several
places on the internet that may be useful to the reader.

 - https://www.freedesktop.org/software/systemd/man/systemd.network.html
 - https://wiki.archlinux.org/index.php/Systemd-networkd


##About

This documentation is part of micro-config-drive, see https://github.
com/clearlinux/micro-config-drive. For bugs, please e-mail
dev@lists.clearlinux.org or visit the https://clearlinux.org/ website.
dev@lists.clearlinux.org or visit the https://clearlinux.org/ website.
dev@lists.clearlinux.org or visit the https://clearlinux.org/ website.
