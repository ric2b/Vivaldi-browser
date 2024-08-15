---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: firmware-ui
title: Firmware UI troubleshooting
---

## Failed to emerge chromeos-bootimage

```
chromeos-bootimage-0.0.3-r80:  * ERROR: sys-boot/chromeos-bootimage-0.0.3-r80::chromiumos failed (compile phase):
chromeos-bootimage-0.0.3-r80:  *   Failed cbfstool invocation: cbfstool halvor/image-halvor.dev.bin add-payload -f /build/volteer/firmware/volteer/depthcharge/dev.elf -n fallback/payload -c lzma
chromeos-bootimage-0.0.3-r80:  * E: Could not add [/build/volteer/firmware/volteer/depthcharge/dev.elf, 111696 bytes (109 KB)@0x0]; too big?
chromeos-bootimage-0.0.3-r80:  * E: Failed to add '/build/volteer/firmware/volteer/depthcharge/dev.elf' into ROM image.
chromeos-bootimage-0.0.3-r80:  * E: Failed while operating on 'COREBOOT' region!
```

When failing to add either `locale_*.bin` or `depthcharge/dev.elf` into AP
image, it usually means the COREBOOT region in RO (check your chromeos.fmd) is
too small to contain all the necessary files.

**Solution 1 (preferred)**: Enlarge RO space in FMAP layout. See
[example CL](https://review.coreboot.org/c/coreboot/+/44362) for zork.

**Solution 2**: Try to decrease the `dpi` setting of your board by overriding
the default value in [bmpblk/boards.yaml]. This will lower the quality of text
in firmware screens (i.e. make it blurrier). Non-text images such as screen
icons and QR code will not be affected. When building bmpblk, if you see the
following warning message:

```
Reducing effective DPI to 126, limited by screen resolution
```

it means the physical panel is unable to display bitmaps with a DPI value larger
than `126`. As such, the build process caps the DPI at `126`, and increasing to
any larger value will not improve bitmap quality or increase flash space usage.
Consider lowering the `dpi` setting to this number (or below) for clarity in the
boards.yaml file.

The following message of `chromeos-bootimage` may also help you estimate the
optimal setting.

```
* assets (RO): 2358 KiB (2820 KiB free) asurada
* assets (RW): 729 KiB (322 KiB free) asurada
```

See [example CL](https://crrev.com/c/2612470) for nautilus and soraka.

## Blurry text in firmware screen

Blurry (or pixelated) text is usually caused by low resolution bitmaps generated
in bmpblk.

**Solution**: Try to specify the highest possible `dpi` of your board in
[bmpblk/boards.yaml] that will fit.

## Fallback screen

![fallback screen](images/fallback_screen.jpg)

### Fallback message

When failing to draw any part of the screen, fallback messages will be shown in
a textbox with monospaced font (similar to the debug info screen).

In this case, please check AP log to find out the failure triggering the
fallback message. Some common reasons are:

* Bitmap not found in RO CBFS: Please check if you have the latest code of
depthcharge and bmpblk, and make sure both ebuilds are built with the same USE
flags.
* Heap too small: Uncompressed bitmaps can take up to 3MiB heap space, depending
on the board settings. Insufficient heap size may lead to drawing failure for
some languages (since localized bitmaps are stored separately for each
language).

### Fallback colored stripes

In any case of drawing failure, 3 colored stripes will also be shown on the top
left corner of the screen to indicate the screen id and the currently selected
index. Please check [CL:2062723](https://crrev.com/c/2062723) for the meaning of
the stripes.

## Failed to enable developer mode

After selecting the "Enable developer mode" option in recovery UI, the device
should reboot to developer UI. If you see the "Confirm returning to secure mode"
screen instead, without the "Cancel" option being present, it means that
booting into developer mode is not allowed due to the `FWMP_DEV_DISABLE_BOOT`
flag being set.

For Googlers: Try to enroll the device for corp access. This might be related to
the [zero-touch enrollment] on your device. See [b/181316942] for details.

<!-- Links -->

[bmpblk/boards.yaml]: https://chromium.googlesource.com/chromiumos/platform/bmpblk/+/HEAD/boards.yaml
[zero-touch enrollment]: https://support.google.com/chrome/a/answer/10130175
[b/181316942]: http://issuetracker.google.com/issues/181316942
