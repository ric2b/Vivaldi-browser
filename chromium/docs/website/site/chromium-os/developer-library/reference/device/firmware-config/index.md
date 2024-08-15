---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: firmware-config
title: SKU and FW Configuration Fields
---

[TOC]

## Background

There are many different concepts that all have the same name: SKU. For this
document SKU will refer to the Firmware SKU referenced in model.yaml or
config.star and firmware code. This SKU is stored in the
[CBI](./cros_board_info.md) EEPROM connected to the EC; this SKU used to be
stored in resistor straps on the EC or AP. For some devices, like ARM, the
SKU is still resistor-strap based.

We have managed SKUs in many ways over the years and the latest two iterations
are HW bit-fields for Nami and pool-based assignment for Octopus. Both
strategies have advantages and drawbacks. Namely, pool-based IDs require a
firmware change when a new ID is assigned, and HW bit-fields require a project
to track all possible combinations of features the OS and firmware need for
variant differentiation. For pool-based IDs, there was some progress to auto
generate code based on model.yaml to eliminate the manual code change when
allocating new SKUs, but the repo dependency model isn’t great.

SKUs are used at multiple levels in the stack for configuration and
customization: firmware and OS. Firmware uses SKUs to initialize drivers
differently. The OS uses SKUs to determine which model the HW is.

In an effort to allow ChromeOS to scale more effectively, we also want to
decouple device identity from configuration information. This means instead of
writing code like `if (sku_id == 17 || sku_id == 23) enable_subsystem_A();`, we
want to write code like `if (cros_config.is_features_A_enabled())
enable_subsystem_A();`. This poses an issue for firmware since it does not have
access to the master configuration to look up config.

## Proposal

The existing SKU field that is stored in CBI EEPROM should be repurposed as the
DesignConfigId that maps to the master config. Per the master config rules, the
DesignConfigId will be globally unique across a program (e.g. octopus-wide scope
or grunt-wide scope) and a foreign key into the master configuration. **This
DesignConfigId will not be used by FW at all**. Any decision point needed in FW
will be encoded in a new bit flag CBI field. DesignConfigId and today’s firmware
SKU serve a similar purpose, and DesignConfigId is just a rename of the existing
concept, with the exception that the DesignConfigId will not encode features
within its value. It is purely a foreign key into the master configuration.

Introduce a **new 32-bit CBI field value: FW configuration**. The fields will
**only** be used by firmware. Typically the firmware configuration will only be
used to configure un-probable hardware differences between designs; this field
will not be forwarded to the OS; only the DesignConfigId will be forwarded to
the OS. If the firmware does not need to make a code decision based on the
values within firmware configuration, then that field should not be a part of
firmware configuration.

A particular value of DesignConfigId will imply only one value for the FW
Configuration field. Multiple DesignConfigId values can have the same FW
Configuration field value. This new FW configuration field in CBI will be
programmed before leaving the shop floor; while on the shop floor the device
will be in the unprovisioned state until final assembly is decided/performed. It
will be re-programmable in RMA flow. The new device configuration management
system will ultimately be the master of this data, and that system will help the
operator set the appropriate values in the CBI EEPROM. One way to think of the
firmware configuration field is a very limited-scope, transform that the
firmware can access of the larger master config dataset.

### FW Configuration field

The FW configuration field **only** encodes code fork points needed within a
particular firmware build (e.g. phaser is a firmware build within octopus
project). This does not have to be the same across each firmware build, although
there will be similarities within and across projects. Below is a list of
**example** fields that would have been present for most octopus-based firmware
builds:

Bits | Features                              | Use
---- | ------------------------------------- | ---
1    | Enabled sensors in EC (1b : yes)      | EC enables sensor monitoring based on this bit. This should be set to 1b when the sensors are stuffed on a design (e.g. convertible) and 0b when the sensors are not stuffed (e.g. clamshell)
1    | Has internal keypad (1b: yes)         | Update keyscan_config based on presence of keypad on internal keyboard
1    | Has AR camera (1b: yes)               | Enables interrupts and updates drivers for AR based camera.
3    | GPIO customization for SoC            | 8 unique GPIO configurations. Values are assigned on a per-firmware build basis.
2    | SARs customization for SoC            | 4 unique SARs customization files. Values are assigned on a per-firmware build basis
2    | VBT customization for SoC             | 4 unique VBT customization files. Values are assigned on a per-firmware build basis
1    | Touchscreen enabled for SoC (1b: yes) | AP enables bus and configures GPIOs for touchscreen device

The following are restrictions placed on the FW Configuration field:

*   Different values should have different code paths in firmware
*   A DesignConfigId value implies the one correct value for the FW
    Configuration field. The DesignConfigId is the foreign key into the master
    config, which is the ultimate source of truth for FW Configuration data.

### Adding new Configuration Options

If firmware code needs to add a new fork or a new path on an existing fork, then
we will need a new field or value within an existing field, respectively. Before
a hardware configuration releases widely (e.g. dogfooding or FSI), we can add
new fields to firmware configuration and new values to existing fields without
too much issue. We would still need to coordinate with the local team that the
CBI EEPROM contents may need to be updated. Once a hardware configuration has
been circulated more widely, we can still add new fields and values, but we need
to do it carefully. Adding new fields or values after wide-distribution will
probably be rare. The below sections describe how we would need to add fields.

Generating a MP-signed OS image for a particular hardware configuration should
lock down the firmware configuration field value. This ensures we know the final
firmware configuration value for release devices.

#### Adding a new field

If we need to add a new field, which corresponds to a new code fork in firmware,
we should try to make the 0-value correspond to the behavior that previously
existed. This will work the best and have the least firmware code overhead.

If any previously released hardware configurations cannot have the 0-value for
the new field, then firmware needs to manually handle those IDs. This list of
IDs will not grow with time since newly made devices will have the most
up-to-date value programmed into CBI. Once we have added the new field and set
the value correctly for all hardware configurations in the master configuration,
we can compute the list of ID where the firmware configuration value changed
from their original FSI release. As a reminder, if the already released hardware
configurations can use the 0-value, then the list of IDs that changed will be
empty.

For an example, let’s say hardware configuration A and C have already been
released, but new firmware needs to handle them differently than the 0--value in
a field. The logic to select the 0-value in the field will be `if
(field_value == 0 && design_config_id != A && design_config_id != C)`.

Furthermore, if hardware configuration A and C need to have the 1-value in that
field, the logic to select the 1st value code path would be `if (field_value ==
1 || design_config_id == A || design_config_id == C)`.

A more concrete example is omitting PWM support for the EC. The old versions of
the firmware advertise PWM support, and when we need to customize the code for
some hardware configurations, the new hardware configurations needed to remove
PWM support advertisement. This means that the 0-value should advertise PWM
support and the 1-value should omit PWM support; we have to use “negative logic”
to express PWM support in alignment with existing firmware behavior for the
0-value. This also means that there are no existing devices where the firmware
configuration value changed after FSI, so the list of IDs we have to manually
account for is empty.

#### Adding a new value to an existing field

When we add a new value to an existing field, which corresponds to a new path in
an existing code fork, we need to keep track of the minimum firmware version
that supports this new path.

The order of operation for adding a new value is

1.  Firmware code will introduce the new value and handle new path
1.  Successful build of firmware. Firmware Version is X.
1.  Add new value as an option to the master config with the firmware version X
1.  Once the OS is built for a particular hardware configuration has upgraded
    its firmware package to X or later, then the master configuration will allow
    the new option to be used for that hardware configuration.

The master configuration will allow us to write configuration unit tests to
implement the step 4 requirement. This will ensure that we don’t release
firmware with a firmware configuration value that it cannot understand.

### Unprovisioned

We will use the following values to indicate an unprovisioned board. A board
cannot leave the factory with the following values. It must be programed before
board finalization:

*   **DesignConfigId**: 0x7fffffff (similar to today’s SKU value of 255)
*   **FW Configuration Field**: not present in CBI

We will have a factory test that ensures that the programmed value in the FW
Configuration field is correct based on the value of DesignConfigId, since a
particular DesignConfigId implies one particular FW Configuration value.

An unprovisioned board will have an empty FW Configuration Field value, and this
will enable everything it can to support easier factory testing; this is similar
to the 255 SKU ID behavior today. For example, an unprovisioned board would try
to enable sensors on the EC to allow for better SMT testing.

As a reminder, an unprovision board should always have a valid board\_id value
in CBI. This is done by pre-flashing the CBI EEPROM before it is SMT’ed on the
PCB. The only exception to this required pre-flashed CBI EEPROM is an initial
proto revision; in that case, the firmware code will handle an unset CBI
gracefully.

#### Requirements for unprovisioned config

The returned master configuration for an unprovisioned board should provide
model and platform level information. Typical variant-level information, like
audio codec or whether base accelerometer is present, can be missing from the
config with the following exceptions:

*   The equivalent of all
    [hardware features](https://chromium.googlesource.com/chromiumos/overlays/board-overlays/+/HEAD/overlay-trogdor/chromeos-base/chromeos-config-bsp/files/model.yaml#100)
    should be marked as present to support better SMT testing of bare boards.

### OS access to hardware properties

[Hardware-properties](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/chromeos-config/README.md#hardware_properties)
information is currently stored in model.yaml which correlates to SKU ID. This
will be replaced and sourced from the new master config system using
DesignConfigId. Once the new device config management system is tracking
everything, it will be the source of truth for hardware properties. This new
configuration system will be responsible for setting the correct FW
configuration value in CBI EEPROM before leaving the factory floor.

### Whitelabel

Adding a new whitelabel device will not require us to make a firmware change to
handle a new SKU (i.e. whitelabel SKU). We will be able to set the
DesignConfigId and FW configuration fields properly in CBI, sourcing the
information from the new device config management system. The DesignConfigId for
different whitelabel devices should be the same, but that is out of scope for
this document. We only need to make a FW change if there is actually a different
code path that needs to be taken (which should not be the case for whitelabel
devices). [CLs in FW that just add](https://crrev.com/c/1392571) a new SKU ID to
a list won’t be needed anymore.

### RMA Considerations

The FW configuration field in CBI can be updated in an RMA flow if needed. The
hardware write protection pin needs to be deasserted, but that is already
accomplished by entering factory mode on H1.

In practice, an OEM/ODM ships spare motherboards to RMA centers. These
motherboards can be used in various different configurations, e,g, clamshell vs
convertible chassis. RMA centers need the ability to update both DesignConfigId
and the FW Configuration fields based on the final configuration that is
reassembled in the RMA center.
