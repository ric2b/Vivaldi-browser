---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: pd-firmware-update
title: USB Type-C Power Delivery (PD) Firmware
---

Some Type-C Port Controller (TCPC) chips in ChromeOS Devices are programmed
with a firmware component when shipped out of the factory. This firmware
(sometimes called PD firmware) may be updateable in the field. The firmware
update for TCPC chips happen as part of the Embedded Controller (EC) Software
Sync process which in turn happens as part of [verified boot].

## PD Firmware update overview

PD Firmware update is managed by [depthcharge]. When the system boots up,
depthcharge performs EC software sync to update the RW firmware of EC and any
peripherals that are connected to EC. During the EC software sync, PD firmware
update is performed after the EC's firmware is updated and has jumped to RW
section successfully.

PD Firmware binary along with a matching PD firmware hash is packaged into
Coreboot File System (CBFS) as part of building the AP firmware. PD Firmware
hash includes firmware version information. When the system boots up using the
updated AP firmware, depthcharge requests the firmware version from the live
TCPC chip. If the firmware version returned by the TCPC chip differs from the
firmware version built into the CBFS, the firmware update is performed. PD
firmware update progress is displayed using a splash screen with the following
message:

> Your system is applying a critical update.

> Please do not turn it off.

Once the PD firmware update is complete, EC is rebooted to RO so that the TCPC
chips are reset.

## Packaging PD firmware into CBFS

Packaging PD firmware into CBFS involves the following steps:

*   [Upload the new PD firmware blob to Google Cloud Platform Storage].
*   [Add/uprev ebuild to build the new PD firmware as a package].
*   [Update private overlay of the board to include the new PD firmware].

### Upload the new PD firmware blob to Google Cloud Platform Storage

When the TCPC chip vendor supplies the new PD firmware binary blob, it is
archived as a tarball and uploaded to [Google Cloud Platform Storage]. The
tarball name must be the same as the new PD firmware package name in [ebuild].

> eg. parade-ps8751a3-firmware-68.tar.xz

In the above example, parade-ps8751a3-firmware-68 is the new PD firmware package
and the tarball name matches that. Please refer below example showing the
contents and the layout of the tarball.

```bash
$ tar -tf parade-ps8751a3-firmware-68.tar.xz
parade-ps8751a3-firmware-68/
parade-ps8751a3-firmware-68/ps8751_a3_0x44.bin
```

The PD firmware blob name inside the tarball must follow a specific format
depending on the TCPC chip type. The ebuilds that install PD firmware blob
into CBFS use this name format to identify the firmware blob.

For Parade TCPC chips, the format is:
`{product_id}_{hardware_version}_0x{hex_firmware_version}.bin`

> eg. ps8751_a3_0x44.bin and the [referring parade ebuild]

For Analogix TCPC chips, the format is:
`{product_id}_ocm_0x{hex_firmware_version}.bin`

> eg. anx3429_ocm_0x16.bin and the [referring analogix ebuild]

For all other chips that get added in the future, the recommended PD FW blob
name format is:
`{product_id}_{hardware_version_optional}_0x{hex_firmware_version}.bin`

The important thing to note here is that the hexadecimal value of the firmware
version is encoded into the firmware blob name.

### Uprev ebuild to build the new PD firmware as a package

Once the PD firmware tarball is uploaded to Google Cloud Platform Storage, add
an ebuild that builds the new PD firmware as a package. This is just a symlink
to the main ebuild package and it just has a different file name - refer to
[example CL](https://crrev.com/c/1683930). The PD firmware package name must
follow a specific format depending on the TCPC chip type.

For Parade TCPC chips, the format is:
`parade-{product_id}{hw_version}-firmware-{decimal_fw_version}`

> eg. parade-ps8751a3-firmware-68

For Analogix TCPC chips, the format is:
`analogix-{product_id}-firmware-{decimal_fw_version}`

> eg. analogix-anx3429-firmware-22

In general, the recommended PD firmware package name format is:
`{vendor_name}-{product_id}{hw_version_optional}-firmware-{decimal_fw_version}`

Ebuild uses the decimal value of the firmware version, encoded in the PD
firmware package, to identify the package version. The install rules in the
ebuild file encodes the firmware version into a separate hash file and strips
the same from the firmware blob name.

Note: The PD firmware packages for a new TCPC chip need to be masked by default
in the [package.mask] file. This is usually done when the first PD firmware
package for a TCPC chip is added. The required PD firmware version for a board
is unmasked in the concerned board's package.unmask file.

### Update private overlay of the board to include the new PD firmware

This step is board-specific depending on the requirement to update the PD
firmware. The package.unmask file in a board's private overlay is updated to
build the new PD firmware version - refer to
[example CL](https://crrev.com/i/1625942). This step installs the artifacts of
the chosen PD firmware package (i.e. firmware blob & hash files) in the firmware
blob path `${SYS_ROOT}/firmware/`.

Additionally update the board's private overlay to add the PD firmware artifacts
into the CBFS while building coreboot. This is a one time process that needs to
be done when the board's private overlay is defined initially - refer to
[example CL](https://crrev.com/i/642000).

## Identifying TCPC chips that require firmware update

Depthcharge identifies the TCPC chips that require firmware update by probing
the EC. During run-time, depthcharge requests EC for the list of TCPC chips
requiring firmware update. Depending on the TCPC configuration, EC responds with
the vendor id, product id and the firmware version. Depthcharge uses this
information and invokes the appropriate TCPC chip driver to perform the firmware
update. To use this approach, enable `CONFIG_CROS_EC_PROBE_AUX_FW_INFO` in
depthcharge - refer to [example CL](https://crrev.com/c/1672053).

## Testing PD FW Update

Here are some of the conditions that need to be met for a successful PD firmware
update:

*   Firmware version in the live chip must differ from the firmware version in
    the CBFS.
*   EC & PD Software Sync must be enabled. To ensure that EC & PD SW Sync are
    enabled, get the GBB flags from a live system using
    `/usr/bin/futility gbb --flash --get --flags` and ensure that
    GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC (1 << 9) and
    GBB_FLAG_DISABLE_PD_SOFTWARE_SYNC (1 << 11) flags are not set.
*   EC must be able to suspend and resume the TCPC chips. One of the conditions
    where TCPC chips cannot be suspended is the absence of battery - a common
    scenario during proto stages. When the battery is absent and the concerned
    USB port acts as a sink, the PD firmware update is not applied.
*   I2C tunnels connecting to the TCPC chips must be unprotected by the EC. When
    the tunnels are protected, depthcharge triggers an "EC reboot to RO" to
    unlock the tunnels.

## Miscellaneous Information

### Power-fail safety

PD firmware update tries to recover the TCPC chip in a bad state due to a
power cycle during an earlier PD firmware update. At the start of PD firmware
update, depthcharge requests the EC for the hard-coded TCPC chip information.
Depthcharge uses this informartion to identify the chips requiring PD firmware
update. TCPC chips that are in bad state will return the firmware version as 0
leading to a firmware version mismatch. This will cause depthcharge to perform
the PD firmware update again.

### Identifying PD firmware version on a live/running chip

In order to identify the PD firmware version on a live/running chip, please
execute the below command from the Device Under Test (DUT):

`ectool pdchipinfo <port_num>`

The output of the above command includes the live PD firmware version from the
chip. This information can be used to ensure that the PD firmware update is
successful.


[verified boot]: https://www.chromium.org/chromium-os/chromiumos-design-docs/verified-boot
[depthcharge]: https://link.springer.com/chapter/10.1007/978-1-4842-0070-4_5#Sec13
[Upload the new PD firmware blob to Google Cloud Platform Storage]: #upload
[Add/uprev ebuild to build the new PD firmware as a package]: #uprev
[Update private overlay of the board to include the new PD firmware]: #update
[Google Cloud Platform Storage]: https://pantheon.corp.google.com/storage/browser/chromeos-localmirror/distfiles
[ebuild]: #uprev
[referring parade ebuild]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/sys-firmware/parade-ps8751a3-firmware/parade-ps8751a3-firmware-52.ebuild
[referring analogix ebuild]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/sys-firmware/analogix-anx3429-firmware/analogix-anx3429-firmware-21.ebuild
[package.mask]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/profiles/base/package.mask
