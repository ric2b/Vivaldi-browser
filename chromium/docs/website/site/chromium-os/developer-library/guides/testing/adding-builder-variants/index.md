---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: adding-builder-variants
title: Adding Builder Variants using Profiles
---

## Purpose

This page documents the steps taken to add builders to the ChromeOS testing
infrastructure. More specifically, this document will focus on adding new
builder variants by adding profiles to existing overlays.

Example CLs are linked throughout this document and are also listed here for
quick access:

* [Add debug kernel builder feature profile](https://crrev.com/c/1929549)
* [Add debug_kernel profile for automated kernel
testing](https://crrev.com/i/2147519)
* [Add config for debug-kernel postsubmit builders](https://crrev.com/i/2266825)
* [Specify test suites for octopus-debug-kernel](https://crrev.com/i/2392510)

## Background

When changes to the ChromeOS code base are submitted, they are tested with
applicable overlays and affected hardware. This process involves automatically
building images and running tests with these images on both physical devices and
virtual machines.

When more test coverage is desired, changes to the ChromeOS Infrastructure must
be made. This can be achieved by adding builders to generate new image variants
to be tested. That process is what this document will focus on.

The debug-kernel builders, used for
[automated debug kernel testing](https://go/cros-automated-debug-kernel-testing),
will be used as an example throughout this document.

The images built by these builders have several additional kernel debugging
features enabled. These additional features sacrifice system performance, but
provide a vehicle for detecting an array of hard to detect and reproduce
bugs - race conditions, improper memory accesses, etc.

## The Process

### Update Appropriate Overlays

To automatically test new ChromeOS image variants, you first must be able to
build them locally. This requires adding profiles to board overlays:

#### Plan board coverage

Decide which boards you want to add test coverage for. If you need broad
coverage, think about which boards differ significantly for your purposes and
try to avoid unnecessary overlap. Infrastructure and lab resources are limited,
so it is important to think about which boards you really care about. Board
overlays are defined in the `src/overlays` and `src/private-overlays`
directories.

#### Adding profiles

Add a profile to each overlay you want to create a builder for. This profile
will inherit from the overlay's base profile and set the desired build
variables. If you plan to add profiles to multiple overlays, see the "Feature
Profiles" section below first.

To make your profile inherit from the overlay's base profile, add a `parent`
file containing the line `../base` to your new profile directory (Example CL
[here](https://crrev.com/i/2147519)). `package.use` files define default USE
flag state on a per package basis. For information on other portage profile
file types see the [Gentoo Portage Profile
page](https://wiki.gentoo.org/wiki/Profile_(Portage))

#### Plan variant details

You also must figure out which build variables you need to set to generate your
desired image variants. Typically this will involve setting or masking a USE
flag. These can be specified in the profile's `package.use` file.

For the debug-kernel builders, test coverage was initially only added for
Octopus. The only build variable update was enabling USE="debug" for the
chromeos-kernel package.

#### Feature profiles

Feature profiles set the USE flag for every version of a package. You can then
use this feature profile as a mix-in for each profile you add to an overlay,
ensuring the correct package version will be affected for that overlay. Feature
profiles are located in the
`src/third_party/chromiumos-overlay/profiles/features` directory.

For debug-kernel builders, the [debug kernel feature
profile](https://crrev.com/c/1929549) is located at
`src/third_party/chromiumos-overly/profiles/features/kernel/debug`. This
profile contains a `package.use` file which enables the debug flag for every
version of the sys-kernel/chromeos-kernel package. The debug kernel feature
profile is then mixed in to the 'debug_kernel' profile in the octopus overlay
by adding `chromiumos:features/kernel/debug` to the `parent` file. See
`src/overlays/overlay-octopus/profiles/debug_kernel` for [this
example](https://crrev.com/i/2147519).

Note that profiles should be public unless there's a specific need
to use a private repo (e.g. restrictive licensing, or trying to keep some
upcoming new HW or SW secret). However, if a new profile inherits from an
existing private profile, it must be in the private repo. Be sure to take into
account the fact that settings in private profiles are applied on top of
settings in the matching public profile (e.g. octopus-private:base vs.
octopus:base).

#### Testing locally

To verify that the profile you added works, build an image for the
corresponding board using your new profile.

1. `setup_board --board=$BOARD --profile=$PROFILE`
2. `cros build-packages --board=$BOARD`
3. `cros build-image --board=$BOARD -r`
4. Verify that the generated image reflects the updates you specified in the
   profile.

### Update Infra Config

Now that you have successfully created profiles and can build the corresponding
images locally, it's time to add builders to the ChromeOS Infrastructure.
After the builders are added, your image variants can be generated and used for
testing automatically.

To create a new builder, you must define both a new builder and a builder
config in the infra config codebase.

#### Adding new builders

To define a new builder, create a function in
`infra/config/new_builders.star` which defines the necessary builders and
then call your function in `infra/config/main.star`. There are helper
functions in `new_builders.star` which define common types of builders (cq,
postsubmit, etc.) so be sure to look into those options before doing more work
than necessary.

It may help to find a builder similar to what you are trying to create and use
it as a template. See [this CL](https://crrev.com/i/2266825) for an example.

__NOTE:__ It is important that new builders do not upload binary packages.
`_define_debug_kernel_builder_configs()` enforces this by setting `packages =
None`. New builder variants should follow the same behavior.

#### Adding a new builder config

To define a new builder config, you must add a function in
`infra/config/builderconfig/builder_config.star`. It is here where you can
specify which profile the builder should use. You must call this function in
`main.star` as well, ensuring that you call your function defining the builders
before defining the corresponding builder configs. The function should look
fairly similar to the one which defined the builders themselves.

See `_define_debug_kernel_builder_configs()` for an example.


#### Specify test suites to run

To specify which tests suites to run with images built by your builders, you
must modify `infra/config/builderconfig/target_test_requirements_config.star`.
If these tests are expirimental or purely informational, be sure to mark them as
non-critical. See [this CL](https://crrev.com/i/2392510) for an example.

__NOTE:__ The format of this file has changed substantially since the linked CL was
added.  Searching for "octopus-debug-kernel" in
[target_test_requirements_config.star](http://cs/chromeos_internal/infra/config/testingconfig/target_test_requirements_config.star)
still provides a good example to follow, however.

#### Testing

To test that you have successfully added builders and corresponding builder
configs, run `./regenerate_configs.py` and verify that the changes in the
generated files seem to be appropriate.


If more guidance is required after reading this document, please reach out to
the ChromeOS Infrastructure team at chromeos-infra-discuss@google.com or
message mwiitala@google.com directly.
