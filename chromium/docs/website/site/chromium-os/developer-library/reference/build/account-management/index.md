---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: account-management
title: CrOS user & group management
---

In accordance with [Gentoo's GLEP 27](https://wiki.gentoo.org/wiki/GLEP:27),
ChromiumOS centrally manages its users & groups so they are stable for a given
build profile.

[TOC]

## Account Database

Each user and group is defined in a file underneath the appropriate
`profiles/base/accounts/` subdirectory in the [eclass-overlay].

For example, the `chronos` user is defined as follows in a file at
[`profiles/base/accounts/user/chronos`](https://chromium.googlesource.com/chromiumos/overlays/eclass-overlay/+/HEAD/profiles/base/accounts/user/chronos):
```
user:chronos
password:*
uid:1000
gid:1000
gecos:system_user
home:/home/chronos/user
shell:/bin/bash
```

The `cras` group is defined in a similarly constructed file at
[`profiles/base/accounts/group/cras`](https://chromium.googlesource.com/chromiumos/overlays/eclass-overlay/+/HEAD/profiles/base/accounts/group/cras):
```
group:cras
password:!
gid:220
users:chronos,power
```

Notice how the membership of the group is provided in the group definition, even
though traditionally this is done at user-creation time.

The password field can be set to one of the following:

*   `!` - The account is locked and may not be logged into (this is the
    default).
*   `*` - No password yet, but the account has the ability to have one added,
    so this should be used for accounts that people expect to have a password
    set for, or want to otherwise login as remotely.
*   `x` - The password is shadowed but the account is for an internal feature;
    people should not set a password themselves.
*   An encrypted password as per
    [crypt(3)](https://man7.org/linux/man-pages/man3/crypt.3.html#NOTES).

### No longer needed UIDs/GIDs

When a UID or GID is no longer needed, the account database entry needs to
remain to prevent the UID/GID from being accidentally reused. Re-using the
UID/GID could lead to a number of problems especially if there are left over
files owned by the UID/GID.

To mark a UID or GID as no longer used, add `defunct:true` to the end of the
database entry file. For example:

```
user:tpmd
uid:225
gid:225
gecos:TPM daemon
home:/dev/null
shell:/bin/false
defunct:true
```

## Choosing UIDs and GIDs

Every UID on CrOS has an associated GID with the same value. The
opposite does not hold true, however.

CrOS system daemon UIDs (and associated GIDs) range from 200-299 and
from 20100-29999. If you're creating a new user, pick the first UID in
the range 20100-29999 that is not currently used, and create both a user
and a group with this ID. To find the next available UID, invoke
`./display-accts.py --show-free` (found in the `profiles/base/accounts/`
subdirectory in the [eclass-overlay]).

FUSE-based filesystem daemons have UID/GIDs that range from 300-399.
If you're adding a daemon that will be talking to `cros-disks` and
managing some kind of volumes (archives mounted as volumes, external
disks, network-mounted storage, etc.) then you should create a user
and group with IDs in this range.

Groups that have no associated user should be given GIDs in the 400 range.

Groups and users that are shared with programs running in different user
namespaces should be in the 600-699 range.

The `chronos` user, which all user-facing processes in CrOS run as, is
UID/GID 1000. There is also a special user/group that has access to
many resources owned by `chronos`, called `chronos-access`, which has
the UID/GID 1001.

### Testing

> *Note*: This step is not necessary for self-groups (groups which only
contain their corresponding user).

If a new group has users, matching baselines must be added to chromite's
[`cros/test/usergroup_baseline.py`](https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/cros/test/usergroup_baseline.py):
```
GroupEntry(group='cras', gid=220, users={'chronos', 'power'}),
```

### Board/project-specific accounts

For boards creating their own users, the 2000-4999 range is reserved for that.
Boards must take care that they don't create conflicts amongst themselves or
any project overlays they inherit.

This space is not intended for mainline CrOS use. Such projects should be
integrated directly into this overlay instead. This is only for boards/projects
that are maintained by partners.

## Creating users and groups in ebuilds

There are two ways to add accounts to the system. The new way (`acct-group` &
`acct-user` packages), is ready to use, but we haven't migrated existing code to
it, so the old way (`user.eclass`) is still used quite a bit. Developers should
use the new system when writing new ebuilds.

The two systems are compatible, and accounts can be safely declared & created by
both simultaneously, so migration should be easy.

### acct-group & acct-user

For accounts that exist in upstream Gentoo, the [cros_portage_upgrade] tool can
be used to quickly import packages. Keep in mind these will be imported to the
portage-stable overlay by default, so they will have to be manually moved to the
[eclass-overlay] instead.

In the [eclass-overlay], create a package for the user in the `acct-user`
category, and a package for the group in the `acct-group` category.

The ebuilds are largely stubs as the business logic is handled by our custom
`acct-group.eclass` & `acct-user.eclass` eclasses. Keep in mind that the values
set in the ebuild are currently ignored (e.g. UID & GID), and instead are taken
from the `profiles/base/accounts/` database instead.

Here is an example `acct-user/sshd`:
```
# Copyright 2020 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

inherit acct-user

DESCRIPTION="User for ssh"

# NB: These settings are ignored in CrOS for now.
# See the files in profiles/base/accounts/ instead.

ACCT_USER_ID=22
```

Here is an example `acct-group/sshd`:
```
# Copyright 2020 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

inherit acct-group

# NB: These settings are ignored in CrOS for now.
# See the files in profiles/base/accounts/ instead.

ACCT_GROUP_ID=22
```

With those packages in place, the ebuild that wants the accounts available at
runtime just adds them to their `RDEPEND`.

```
RDEPEND+="
	acct-group/sshd
	acct-user/sshd
"
```

### user.eclass (the old way)

The API implemented by the CrOS-specific [user.eclass] is compatible with the
one that historically was provided by the upstream user.eclass.

```sh
enewuser cras   # Creates a user called 'cras' with the pre-specified UID.
enewgroup cras  # Ditto for the group.
```

You can choose to specify other fields when calling the functions to create
new users and groups, but the eclass will bail if the values you choose conflict
with those in the DB.

Calls to `enewuser` and `enewgroup` are allowed ONLY in three ebuild stanzas:

*   `pkg_setup()`: Make the calls here if you need to chown/chgrp files using
    the accounts you're creating.
*   `pkg_preinst()`: Make the calls here if you just need the accounts to exist
    at runtime.
*   `pkg_postinst()`: Try to avoid making the calls here. If you need a failed
    account creation to be non-fatal, then you can add them here.

Bear in mind that when creating a new user, simply using `cros deploy` to
install the new package on the test system will **not** install the new user.
This is discussed in [crbug.com/402673]. You can build a new image (by using
`emerge-$BOARD <package>` and `cros build-image`) to get the new user/group to
show up in `/etc/passwd` and `/etc/group`.

Alternatively you can `emerge-$BOARD` the package with the new user/group and
then copy `/build/$BOARD/etc/{passwd,group}` over the device's
`/etc/{passwd,group}`, via `scp` or some other mechanism.

## FAQ

### User/group not showing up in newly built image!

Defining an account in the `profiles/base/accounts/` databases is not sufficient
to actually create the account at runtime. By design, CrOS only creates accounts
that are used by packages that are installed into the rootfs. So if no installed
package creates those accounts, they won't show up in the image.

### Deploying packages via 'cros deploy' are missing accounts!

Currently the account databases are created only when the image is initially
created. Trying to deploy new ones on the fly won't work. You can workaround it
by manually editing the `/etc/passwd` & `/etc/group` files to add the accounts,
or creating the image.


[crbug.com/402673]: https://crbug.com/402673
[cros_portage_upgrade]: /chromium-os/developer-library/guides/portage/package-upgrade-process/
[eclass-overlay]: https://chromium.googlesource.com/chromiumos/overlays/eclass-overlay/
[user.eclass]: https://chromium.googlesource.com/chromiumos/overlays/eclass-overlay/+/HEAD/eclass/user.eclass
