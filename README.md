
## A config-drive handler.


### Contents:

  - Description of this project
  - Compiling, prerequisites
  - Security considerations
  - Bugs and feedback?


## Description

Micro-config-drive ("ucd" for short) provides a subset of functionality
that is provided by cloud-init implementations. Specifically,
ucd provides the basic options and functions to handle config-2
config-drive data for systems that have this config-drive data provided
as a block device to the container or VM.

cloud-init is the standard way for cloud customers to initialize
containers and virtual hosts. These virtual machines are usually
provisioned in bulk and provided without any customization to cloud
customers, which then require that they are customized for their
particular purpose.

Several implementations exist that implement this functionality,
and this implementation is fairly similar to the other ones. For
reference, we're listing the other implementations here:

- https://launchpad.net/cloud-init
    A Python-based implementation and the "standard". Many of the
    features of this implementation are derived and benchmarked against
    this version.  This implementation supports many different OS's,
    not just Ubuntu.
- https://github.com/coreos/coreos-cloudinit
    A Go-based implementation for CoreOS.

While generally it's preferred to use existing implementations,
since it reduces duplicate code and makes stronger communities, in
this project's case it was decided to forego extending and working
on the existing implementations. Several factors were considered, and
a short summary of the key points of that decision are listed below.

Speed is a significant factor. Interpreted languages have come a long
way and are highly performant, especially if properly used. However,
there is a significant cost of provisioning cloud nodes that have
increased base storage costs due to the inclusion of libraries of
interpreted languages. Since we expect people to prefer cloud nodes are
minimal in size, having interpreted language libraries just for the
sake of a cloud initialization script makes little sense, and we can
reduce the storage need and copy times of cloud images significantly.

Language choice is a minor factor. Generally languages that have
good exception handling are preferred. Since execution speed and low
complexity are preferred for installation and boot-time critical tasks,
lots of libraries should be avoided, as robustness is critical. Missed
python exceptions could cause cloud-init execution to halt, resulting
in an unusable cloud host, which is to be avoided at all cost. The
drawbacks of C being fairly sparse are known, and debugging tools
compensate for that.

We can shed additional overhead by eliminating unwanted functionality
that a clearlinux cloud node does not need, but we can likely
never recoup the initial cost of a base Python installation, as it
is generally in the order of 100MB or more. Hence, we've opted to
implement a version in C that has reduced library requrements. This
has brought the size of this implementation down to under 1mb. Even
considering used libraries, the resulting binary is small.


## Compiling

Currently, ucd requires the following prerequisites:
- `glib-2.0 >= 2.24.1`
- `yaml-0.1 >= 0.1.4`
- `libparted >= 3.1`
- `blkid >= 2.25.0`
- `json-glib-1.0`

As ucd is tooled with autotools, one shouldn't have to do
more than:

```
$ sh autogen.sh
$ make
$ sudo make install
```


## Security considerations

`micro-config-drive` and `user-data-fetch` should never be deployed
on existing, or provisioned, installations. Doing so may expose your
system to an attacker on a local network.

If deploying on a public could infrastructure, you should assure that
routing to your instance is limited to trusted infrastructure nodes.

Most cloud service providers use LL addresses to serve cloud configs.
If your CSP does not, you should verify that your use of non-LL
addresses for cloud config service data is secure and does not expose
your systems to unnecesary risk.

When running this project in a CSP environment, you should assure
you are enabling the correct cloud service type. Do not under any
circumstance enable several service types, this may further expose
your instances to risk.

`micro-config-drive` is not immune to `man-in-the-middle` (MITM)
attacks, since it relies on unencryted HTTP services. This means a
local attacker may manipulate the received cloud service data. This
is a fundamental issue with `cloud-config` as it currently is offered
by CSP's, and not an implementation bug.


## Bugs, feedback, contact

ucd is hosted on github. You can find releases, an issue tracker
and git sources on github.com/clearlinux/micro-config-drive. For
mailing lists, subscribe to dev@lists.clearlinux.org (via
lists.clearlinux.org).

When submitting bug reports, it may be relevant to include the
log output of clouds- init. This can be done with the following
command: `journalctl -b -u ucd`. Omit the `-b` option if
you want output from prior to the last boot event.

This project has many contributors. Not all may be mentioned in the
AUTHORS file.

