---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: debug-buttons
title: Debug Button Shortcuts
---

ChromeOS devices provide a variety of advanced keyboard and button shortcuts
that are useful for debugging. We document those here, as well as highlight
some differences between devices with and without keyboards.

[TOC]

## Devices With Keyboards

See the [official support page] for non-debug keyboard shortcuts.

Note that some of the below debug keyboard shortcuts are implemented at a very
low level and are not keyboard-layout aware. When in doubt, assume debug
shortcuts refer to the physical mapping of a US keyboard layout.

Shortcuts with an asterisk (**\***) also work with tablets/detachables with a
keyboard attached.

Functionality                         | Shortcut
------------------------------------- | --------
Clean shutdown                        | Power button, hold for 3 - 8 seconds
Hard shutdown                         | Power button, hold for 8 seconds
Screenshot capture                    | `Ctrl + Switch-Window` **\***
File feedback / bug report            | `Alt + Shift + I` **\***
Powerwash                             | `Ctrl + Alt + Shift + R` from login screen **\***
Full system reset                     | `Power + Refresh + ChromeOS-Back (F1)`, hold for 10 seconds. Only works on some models starting in 2023.
EC reset                              | `Power + Refresh`
Recovery mode                         | See [firmware keyboard UI] section
Developer mode                        | See [firmware keyboard UI] section
Battery cutoff                        | `Power + Refresh`, hold down, remove power for 5 seconds
Warm AP reset                         | `Alt + Volume-Up + R`
Restart Chrome                        | `Alt + Volume-Up + X` **\***
Kernel panic/reboot                   | `Alt + Volume-Up + X + X` **\***
Reboot EC but don't boot AP           | `Alt + Volume-Up + Down-Arrow`
Force EC hibernate                    | `Alt + Volume-Up + H`
Override USB-C port used for charging | `Left-Ctrl + Right-Ctrl + Search + (0 or 1 or 2)` **\***
Virtual terminal switching            | `Ctrl + Alt + F1 (or F2, F3, F4)` **\***

*Notes:*

*   AP = Application Processor; EC = Embedded Controller.
*   EC hibernate is supported only on devices with select Embedded Controllers.
*   `Alt + Volume-Up` is treated as the [Magic SysRq] key. When in Developer
    mode, you can enable SysRq key combinations as documented in the Linux
    kernel docs.
*   Overriding the charging port is only supported on Samus.

### Firmware Keyboard Interface

*   **Recovery mode**: Hold `Esc + Refresh` ![][refresh-icon] and press
    `Power`.
    *   On some Chromebooks the combination to hold is `Esc + Full screen`
        ![][fullscreen-icon] instead.
    *   From here, follow the instructions at
        https://google.com/chromeos/recovery to recover your device using
        external media.

*   **Recovery mode**: Engage the small `Reset` pinhole with a paperclip,
    hit `Power` and continue engaging `Reset` for 2 seconds. Typically (only?)
    for chromeboxes with a QWERTY keyboard attached.
    *   From here, follow the instructions at
        https://google.com/chromeos/recovery to recover your device using
        external media.

*   **Developer mode**: To enter [Developer Mode], first enter recovery mode,
    then press `Ctrl + D`, followed by `Enter` to accept.
    *   Note that [Developer Mode] disables security features and may leave your
        device open to attack. Only enable if you understand the risks.
    *   Devices in [Developer Mode] will show a warning screen on every boot. The
        screen will time out after 30 seconds, playing a warning beep.
    *   From the warning screen, the following keyboard shortcuts are available:
        *   `Ctrl + D`: Boot the system from the internal disk.
        *   `Ctrl + U`: Boot the system from an external USB stick or SD card.
            The option `crossystem dev_boot_usb=1` must be set from the command
            line before this option is available.
        *   `Ctrl + L`: Chain-load an alternative bootloader (e.g. SeaBIOS).
            This may allow booting other Operating Systems more easily. Not all
            devices are shipped with alternative bootloaders. New alternative
            bootloaders can be manually installed on the device from
            [Developer Mode]. The option `crossystem dev_boot_legacy=1` must be
            set from the command line before this option is available.
        *   `0` through `9`: On newer (2019+) platforms, more than one
            alternative bootloader can be installed and `Ctrl + L` will show a
            selection menu. On these devices, the number keys can be used to
            directly boot the numbered bootloader without going through the
            menu.
    *   The option `crossystem dev_default_boot=usb` can be set to override the
        default boot behavior if no key combination is pressed for 30 seconds.

[refresh-icon]: https://lh3.googleusercontent.com/3XKnstOuiq5G1zw_fiVWq18N1Q5nBcmrj9-Yqq5hYeFnD7XkMAWpAwBo12CpF098UJc=w36-h36
[fullscreen-icon]: https://storage.googleapis.com/support-kms-prod/ZgwI1JWIMPXn0bOjJXIbIkYraiD8KtwGrJRH

Some devices do not support `Esc + Power + Refresh` for entering Recovery Mode:

*   Chromeboxes have a recovery button. To enter Recovery Mode, hold down the
    recovery button and press `Power`.
    *   To enter [Developer Mode] on a Chromebox, first enter recovery mode and
        press `Ctrl + D` on the keyboard, then push the physical recovery button
        on the Chromebox to accept.
*   Older devices have a physical [Developer Mode] switch. You can read up on
    your particular device under the [device-specific developer information]
    table.

_Note_: An additional, deeper reset (with forced memory retraining) can be
triggered on some devices on the way to recovery mode by holding `Left-Shift`
during the normal recovery sequence (i.e., `Left-Shift + Esc + Refresh +
Power`).

## Devices Without Keyboards

### Shortcuts

Starting in 2018, keyboardless ChromeOS devices are being built, in tablet and
detachable form factors. Without a keyboard, some typical keyboard-based
debugging shortcuts can't be supported. Below are the shortcuts for performing
various system and debugging tasks with only the limited physical buttons and
touch screen found on tablets or detachable devices.

Note that some of the following behaviors work on [convertible] devices when
used in tablet mode.

Note: If the tablet has a detachable keyboard, remember to detach it before trying these shortcuts.

Functionality       | Shortcut
------------------- | ----------------------------------------------------------
Screen off          | Power button, short press 
Clean shutdown      | Power button, hold for 3 seconds, select option in menu
Hard shutdown       | Power button, hold for 10 seconds
Screenshot capture  | `Power + Volume-Down`, short press
File feedback       | Power button, hold for 3 seconds, select option in menu
EC reset            | `Power + Volume-Up`, hold for 10 seconds
Recovery mode       | See [firmware menu UI] section
Developer mode      | See [firmware menu UI] section
Battery cutoff      | `Power + Volume-Up`, hold down, remove power for 5 seconds
Warm AP reset       | See [EC debug mode] section
Restart Chrome      | See [EC debug mode] section
Kernel panic/reboot | See [EC debug mode] section

*Notes:*

*   Filing feedback from the power button menu is currently (as of 2018-10-19)
    only [supported](https://crbug.com/845558#c8) on canary (for all users) and
    dev/beta/stable (for Google employees) release channels. All users can
    still file feedback via the browser options menu (`Help > Report an
    issue...`).
*   Battery cutoff shortcut is supported only on devices with a [smart battery]
    (e.g., not Scarlet, Dru).


### Firmware Menu Interface

The [boot firmware screen] is accessible through keyboard shortcuts on devices
with keyboards; on keyboardless devices, the following methods are supported:

*   **Recovery mode**: `Power + Volume-Up + Volume-Down`, hold for 10 seconds.
    If the screen turns off or on during that period, ignore it and make sure
    you keep all three buttons held down for the whole 10 second period, then
    release them.
    *   From here, follow the instructions at
        https://google.com/chromeos/recovery to recover your device using
        external media.
    *   To change the UI language or display additional debugging information,
        you can press either `Volume-Up` or `Volume-Down`. This leads to a menu
        that can be navigated with the volume buttons, and menu items can be
        selected with the power button.
*   **Developer mode**: Enter recovery mode and press `Volume-Up + Volume-Down`
    simultaneously, then select "Confirm Enabling Developer Mode" in the menu.
    *   Note that [Developer Mode] disables security features and may leave your
        device open to attack. Only enable if you understand the risks.
    *   Devices in [Developer Mode] display a similar menu on every boot that
        allows selecting developer boot options. The menu will time out and boot
        after no key has been pressed for 30 seconds, playing a warning beep.
    *   The "USB or SD card" and "Legacy BIOS" boot menu options need to be
        enabled on the command line before they work, same as with the [firmware
        keyboard UI].
    *   In addition to selecting boot options in the menu, the following
        shortcut combinations are available in the developer warning menu:
        *   Press `Volume-Down` for 3+ seconds, then release: Boot the system
            from the internal disk.
        *   Press `Volume-Up` for 3+ seconds, then release: Boot the system
            from an external USB stick or SD card.
        *   If a keyboard is connected, all boot shortcuts from the [firmware
            keyboard UI] are also available in the developer warning menu.

_Note_: An additional, deeper reset (with forced memory retraining) can be
triggered on some devices on the way to recovery mode by holding `Power +
Volume-Up + Volume-Down` for 30 seconds (until the LED starts flashing).

### EC Debug Mode

Not all debugging shortcuts are implemented with a simple combination of
buttons—some are found by utilizing multi-button sequences, initiated after
entering a special EC state called "debug mode". To enter debug mode, press
`Volume-Up + Volume-Down` for 10 seconds. Release once the charging LED starts
flashing. Once in debug mode, any of the following actions can be taken, using
`Volume-Up` (`U`) and `Volume-Down` (`D`) buttons.

Supported codes:

Functionality      | Sequence
------------------ | --------
Restart Chrome     | `UD`
Kernel panic/crash | `UUD`
Warm reset         | `DU`

Debug mode is cancelled when a sequence finishes, or after 10 seconds without a
volume button press.


[official support page]: https://support.google.com/chromebook/answer/183101
[firmware keyboard UI]: #Firmware-Keyboard-Interface
[Magic SysRq]: https://www.kernel.org/doc/html/latest/admin-guide/sysrq.html
[Developer Mode]: ./developer_mode.md
[convertible]: https://en.wikipedia.org/wiki/Laptop#Convertible
[firmware menu UI]: #Firmware-Menu-Interface
[EC debug mode]: #EC-Debug-Mode
[smart battery]: http://sbs-forum.org/specs/sbdat110.pdf
[device-specific developer information]: https://www.chromium.org/chromium-os/developer-information-for-chrome-os-devices
[boot firmware screen]: developer_mode.md
