---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: firmware-uprev
title: Firmware uprev troubleshooting
---

## Missing 'component\_manifest.json' in bcs://\*.tbz2

If you see the following error message in the CQ:

```
 * Build BCS firmware updater to chromeos-firmwareupdate: -i /build/corsola/tmp/portage/chromeos-base/chromeos-firmware-corsola-0.0.1-r100/distdir -c /build/corsola/usr/share/chromeos-config/yaml/config.yaml --ec_component_manifest_output cme
Traceback (most recent call last):
  File "./pack_firmware.py", line 1676, in <module>
    main(sys.argv)
  File "./pack_firmware.py", line 1672, in main
    packer.Start(argv[1:])
  File "./pack_firmware.py", line 1602, in Start
    images, ec_component_manifests = self._WriteFirmwareImages(
  File "./pack_firmware.py", line 1438, in _WriteFirmwareImages
    fw_source = self._ExtractFirmware(firmware, unpack_dir)
  File "./pack_firmware.py", line 1403, in _ExtractFirmware
    raise PackError(
__main__.PackError: Missing 'component_manifest.json' in bcs://Skitty_EC.15194.195.0.tbz2
```

that means the BCS tarball `bcs://Skitty_EC.15194.195.0.tbz2` was not correctly packed.
Please do the following steps to re-upload the BCS tarball:

1. Use [`repack_firmware_tars`](https://chromium.googlesource.com/chromiumos/platform/dev-util/+/refs/heads/main/contrib/firmware/repack_fw_tars)
   (or your own script) to create the correct AP and EC tarballs.
   The EC tarball must contain both `ec.bin` and `component_manifest.json`.
   If the firmware archive (such as `ChromeOS-firmware-R109-15194.10.0-corsola.tar.bz2`) doesn't contain `component_manifest.json`,
   please contact yupingso@google.com.
2. Since BCS files cannot be overwritten by most people,
   rename the tarballs by incrementing the "patch version".
   For example, rename `Skitty_EC.15194.195.0.tbz2` to `Skitty_EC.15194.195.1.tbz2`.
3. Upload the new AP and EC tarballs to BCS.
   Actually, only the EC tarball needs to be re-uploaded.
   However, some projects' configs don't support separating AP and EC versions.
   In that case, re-uploading both AP and EC tarballs might be easier.
4. Revert your repositories to a clean state (especially the
   `chromeos-base/chromeos-firmware-*/Manifest` file in the private overlay).
5. Follow the usual firmware uprev flow
   (changing the firmware version in project's config,
   building `chromeos-firmware-${BOARD}`, uploading CLs, ...).
   **Please remember to update the "patch version" in project's config
   (usually `config.star`) to match the BCS file name.**
