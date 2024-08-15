---
breadcrumbs:
- - /chromium-os/developer-library
  - ChromiumOS
page_name: glossary
title: Glossary
---

## Acronyms

*   __3PL__: Third Party Labs.
*   __ACLs__: Access Control Lists.
*   __AFE__: Auto Test Front End.
*   __AP__: Application Processor.
*   __AU__: Auto Updates.
*   __AVL__: Approved Vendor List.
*   __BCS__: Binary Component Server.
*   __BFT__: Board Function Testing.
*   __BOM__: Bill of Materials.
*   __BSP__: Board support package.
*   __BVT__: Build & Verification Test.
*   __CL__: "Change List", a set of changes to files (akin to a
    single git commit).
*   __CPFE__: ChromeOS Partner Front End.
*   __CQ__: "Commit Queue", infrastructure to automatically
    check/build/test/verify/etc... CLs before merging into the tree.
    See also the [Chromium CQ](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/infra/cq.md).
*   __CRX file__: [CRX files](https://developer.chrome.com/docs/extensions/how-to/distribute/host-on-linux#packaging)
    are ZIP files with a special header and the .crx file extension used to
    package Extensions and Apps.
*   __CTS__: Android Compatibility Test Suite.
*   __CWS__: ["Chrome Web Store"](https://chrome.google.com/webstore/), used to
    host & distribute Chrome extensions.
*   __DDOC__: Design Document.  Describes & outlines a project and everything
    related to it to help others review & decide whether & how to move forward.
*   __DPTF__: (Intel's) Dynamic Platform & Thermal Framework.
*   __DUT__: "Device under test", used to refer to the system running
    Chromium [OS] and where tests are being executed.
*   __DVT__: Design Validation and Testing.
*   __EC__: Embedded Controller.
*   __EVT__: Engineering Validation and Testing.
*   __FAFT__: Fully Automated Firmware Test.
*   __FCS__: Final Customer Ship.
*   __FFT__: Final Function Testing.
*   __FSI__: Final Shipping Image.
*   __FSP__: Firmware Support Package.
*   __GBB__: Google Binary Block, a chunk of data stored in the NVRAM.
    Contains variables related to boot and identity.
*   __GERBER__: When ODM gives full set of files for board vendor
     to make appropriate holes etc.
*   __GRT__: Google Required Tests.
*   __GS__: "Google Storage", used to refer to Google Storage Buckets
    (e.g. gs:// URIs).
*   __GTM__: Go To Market.
*   __GTTF__: "Green Tree Task Force".
*   __GoB__: "Git-on-Borg" or "Gerrit-on-Borg" or "Gitiles-on-Borg"
    depending on the context. Used as an umbrella term to refer to
    the git related services on [chromium-review.googlesource.com](https://chromium-review.googlesource.com)
    and [chromium.googlesource.com](https://chromium.googlesource.com).
*   __HDCP__: High Bandwidth Digital Content Protection.
*   __HEDT__: High End Desktop.
*   __HW WP__: Hardware Write Protect. Physical mechanism to prevent
    disabling software write protect. Typically a signal grounded by
    a screw.
*   __IQC__: Incoming Quality Control.
*   __LGTM__: "Looks good to me", commonly used to approve a code
    review.
*   __LKCR__: "Last known compilable revision" - similar to LKGR
    (below), the last build that compiled.
*   __LKGM__: "Last known good manifest", the last manifest version
    that passed a minimal set of tests.
*   __LKGR__: "Last known good revision", the last build that passed
    all tests.
*   __LOEM__: Local OEM, process model that different OEMs share
    exactly same device (with no difference) that uses same firmware
    code and disk image. Only OEM is different.
*   __LSC__: Large Scale Change
*   __MLB__: Main Logic Board (aka motherboard).
*   __MVP__: "Minimum viable product", used to refer to the subset of
    a feature we want to ship initially.
*   __NRE__: Non-Recoverable Engineering cost.
*   __OGR__: OEM Gate Review Meetings.
*   __OOBE__: Out-of-box experience.
*   __OQC__: Ongoing Quality Control, check for sampling.
*   __PCB__: Printed Circuit Board.
*   __PCIe__: Peripheral Component Interconnect Express expansion bus
    standard for connecting devices.
*   __PCRs__: Platform Configuration Registers.
*   __PDD__: Privacy Design Document.  Outlines everything privacy related to
    make sure the project is doing the right thing.
*   __PDG__: Platform Design Guide.
*   __PFQ__: "Preflight queue", used to describe bot configurations
    in the waterfall that run to test/gate changes before they're
    allowed into the tree for everyone to see.
*   __PRD__: Product Requirements Document.  Used to justify a feature/project,
    not to design it.
*   __PS__: Patchset.  Never used to mean "patch series".
*   __PSR__: Panel Self Refresh (eDP).
*   __PTAL__: "Please take a[nother] look", often used when someone
    is happy with the state of a CL and want reviewers to look
    [again].
*   __PVT__: Production Validation and Testing.
*   __PoR__: Process of Record / Plan of Record.
*   __QAV__: Quality Assurance Verification.
*   __RSLGTM__: "Rubber stamp looks good to me", used when the
    reviewer is merely granting OWNERS approval without doing a
    proper code review.
*   __RVG__: The "Restrict-View-Google" label used to restrict issues in
    monorail to Google employees (in addition to the reporter or people cc-ed).
*   __SGTM__: Secret Google Time Machine "Sounds good to me".
*   __SI__: Signal Integrity.
*   __SMT__: Surface-mount Technology.
*   __SW WP__: Software Write Protect.
*   __Servo__: a debugging board that connects via USB to a host
    machine and a device under test.
*   __TBR__: "To be reviewed". In
    [specific circumstances](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/code_reviews.md#TBR-To-Be-Reviewed)
    used to land code and have it reviewed later.
*   __TCPC__: Type C Port Controller.
*   __TPM__: ["Trusted Platform Module"](https://en.wikipedia.org/wiki/Trusted_Platform_Module),
    Tamper-resistant chip that the CPU can talk to. Securely stores
    keys and does cryptographic ops. We use this to encrypt the keys
    used to encrypt user files (to make passphrase recovery more
    difficult). See also TpmQuickRef.
*   __ToT__: "Tip of Tree" or "Top of Tree", as in the latest
    revision of the source tree.
*   __UFS__: Universal Flash Storage.
*   __UMA__: User Metrics Analysis.
*   __VPD__: Vital Product Data.
*   __WAI__: "Working As Intended", e.g. the behavior described is
    not a bug, but working as it is supposed to. This is not to say
    the intention cannot change (as a feature request), simply that
    it is not a bug.
*   __WIP__: "Work In Progress" - e.g. a patch that's not finished,
    but may be worth an early look.
*   __Zerg__: Process model for partner to build multiple new devices that only
    had slight variance from the reference board (touch/no touch, etc…). These
    devices share single firmware code and disk image.

## English Acronyms and Abbreviations

*   __AFAICT__: as far as I can tell
*   __AFAIK__: as far as I know
*   __DTRT__: Do(ing) The Right Thing.
*   __e.g.__: (latin) for example
*   __FWIW__: for what it's worth
*   __IANAL__: I am not a lawyer
*   __IIRC__: if I recall/remember correctly
*   __IIUC__: if I understand correctly
*   __IMO__: in my opinion
*   __IMHO__: in my honest opinion
*   __IOW__: in other words
*   __i.e.__: (latin) in other words
*   __nit__: short for "nitpick"; refers to a trivial suggestion such as style
    issues
*   __PSA__: public service announcement
*   __WRT__: with respect to

## Chrome Concepts

*   __Chrome Component__: Components of chrome that can be updated
    independently from Chrome its self. Examples are PDF Viewer, Flash Plugin.
*   __Component App / Component Extension__: App or Extension built and
    shipped with Chrome. Examples are
    [Bookmark Manager](https://cs.chromium.org/search/?sq=package:chromium&type=cs&q=bookmark_manager),
    [File manager](https://cs.chromium.org/search/?sq=package:chromium&type=cs&q=file_manager).
*   __Default Apps__: Apps or Extensions that are shipped with Chrome as .CRX
    files and installed on first run.
*   [__Extension__](https://developer.chrome.com/extensions):  Third party
    developed code that modifies the browser.
*   [__Packaged App__](https://developer.chrome.com/apps/about_apps):  Packaged
    apps run outside of the browser, are built
    using web technologies and privileged APIs.
*   __Packaged App (old)__: Older packaged apps (pre 2013) still ran in tabs,
    but with offline packaged resources.
*   __Shared Modules__: Extensions or Apps that export resources accessible
    from other Ext/Apps. Dependencies are installed automatically.
*   __Aura__: The unified graphics compositor ([docs](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/aura/index.md)).
*   __Ash__: The Aura shell (e.g. the ChromiumOS look); see [Ash](https://chromium.googlesource.com/chromium/src/+/HEAD/ash/README.md) for more
    info.

## Building

*   __buildbot__: A column in the build waterfall, or the slave (machine)
    connected to that column, or the
    [build waterfall infrastructure](https://dev.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot)
    as a whole.
*   __clobber__: To delete your build output directory.
*   __component build__: A shared library / DLL build, not a static library
    build.
*   __land__: Landing a patch means to commit it.
*   __slave__: A machine connected to the buildbot master, running a sequence
    of build and test steps.
*   [__tryserver__](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_arm):
    A machine that runs a subset of all tests on all platforms.
*   __sheriff__: The person currently charged with watching over the build
    waterfall to make sure it stays green (not failing). There are usually two
    sheriffs at one time. The current sheriffs can be seen in the upper left
    corner of the waterfall page.
*   __symbolication__: The process of resolving stack addresses and backtraces
    to human readable source code methods/lines/etc...
*   __tree__: This means the source tree in subversion. Often used in the
    context of "the tree is closed" meaning commits are currently disallowed.
*   __try__: To try a patch means to submit it to the tryserver before
    committing.
*   [__waterfall__](https://ci.chromium.org/p/chromium/g/chromium/console):
    The page showing the status of all the buildbots.

## General

*   __Flakiness__: Intermittent test failures (including crashes and hangs),
    often caused by a poorly written test.
*   __Jank/Jankiness__: User-perceptible UI lag.
*   __Chumping__: Bypassing the CQ and committing your change directly to the
    tree. Generally frowned upon as it means automatic testing was bypassed
    before the CL hits developer systems.

## User Interface

*   __Bookmark bubble__: A "modal" bubble that appears when the user adds a
    bookmark allowing them to edit properties or cancel the addition.
*   __Download bar__: The bar that appears at the bottom of the browser during
    or after a file has been downloaded.
*   __Extensions bar__: Similar to the download bar, appears at the bottom of
    the screen when the user has installed an extension.
*   __Infobar__: The thing that drops down below asking if you want to save a
    password, did you mean to go to another URL, etc.
*   __NTB__: New Tab button (the button in the tab strip for creating a new
    tab)
*   __NTP or NNTP__: The New Tab Page, or the freshly rebuilt new tab
    functionality dubbed New New Tab Page.
*   __Status bubble__: The transient bubble at the bottom left that appears
    when you hover over a url or a site is loading.

## Video

*   __channels__: The number of audio channels present. We use "mono" to refer
    to 1 channel, "stereo" to refer to 2 channels, and "multichannel" to refer
    to 3+ channels.
*   __clicking__: Audio artifacts caused by bad/corrupted samples.
*   __corruption__: Visible video decoding artifacts. Usually a result of
    decoder error or seeking without fully flushing decoder state. Looks similar
    to this.
*   __FFmpeg__: The open source library Chromium uses for decoding audio and
    video files.
*   __sample__: A single uncompressed audio unit. Changes depending on the
    format but is typically a signed 16-bit integer.
*   __sample bits__: The number of bits per audio sample. Typical values are
    8, 16, 24 or 32.
*   __sample rate__: The number of audio samples per second. Typical values
    for compressed audio formats (AAC/MP3/Vorbis) are 44.1 kHz or 48 kHz.
*   __stuttering__: Short video or audio pauses. Makes the playback look/sound
    jerky, and is often caused by insufficient data or processor.
*   __sync__: Audio/video synchronization.

## Display
*   __active region__: The region of a frame where the video data is being transmitted.
*   __Adaptive Sync__: VRR standard developed by VESA.
*   __amdgpu__: Name of the AMD gpu/display driver in Linux kernel.
*   __bpp__: Bits per pixel.
*   __bridge__: DRM (driver) object representing hardware which can convert one display protocol to another (ie: HDMI to DP).
*   __Chameleon__: A device which can mimic a connected display. Used for automated display testing.
*   __Chamelium__: See Chameleon.
*   __connector__: DRM (driver) object representing the physical connector on the device. Is used to communicate with the panel.
*   __CRTC__: DRM (driver) object which refers to the engine responsible for scanning framebuffers from memory to encoder(s). Originally stood for Cathode Ray Tube Controller.
*   __DP__: DisplayPort.
*   __DP++__: DisplayPort Dual-Mode.
*   __DRM (content protection)__: Digital Rights Management.
*   __DRM (driver)__: Direct Rendering Manager, the Linux subsystem for kernel gpu/display drivers.
*   __DSC__: VESA Display Stream Compression. Used to save bandwidth between the transmitting/source device and the display.
*   __DSI__: MIPI Display Serial Interface.
*   __DSI command mode__: A MIPI DSI operational mode which sends pixels on demand.
*   __DSI video mode__: A MIPI DSI operational mode which sends pixels continuously.
*   __eDP__: Embedded DisplayPort.
*   __encoder__: DRM (driver) object which sits between the CRTC and bridge/connector. Responsible for transcoding the output of a CRTC into a format digestible by the bridge/connector.
*   __format__: An agreed upon layout of pixel data in memory.
*   __format modifier__: Denotes that the format has undergone modification (most commonly compression).
*   __fourcc__: A code representing a format.
*   __framebuffer__: DRM (driver) object representing a collection of memory which can be scanned out to the display.
*   __HBR__: DisplayPort High Bit Rate (2.70 Gbit/s).
*   __HBR2__: DisplayPort High Bit Rate 2 (5.4 Gbit/s).
*   __HBR3__: DisplayPort High Bit Rate 3 (8.1 Gbit/s).
*   __HDCP__: High Definition Content Protection.
*   __HDMI__: High Definition Multimedia Interface.
*   __horizontal active (hactive)__: The width of the active region.
*   __horizontal blank (hblank)__: The region before and after each row of scanout where there is no video being transmitted.
*   __horizontal front porch__: The portion of a blanking region which appears after the active region of a row.
*   __horizontal back porch__: The portion of a blanking region which appears before the active region of a row.
*   __horizontal sync (hsync)__: An interrupt signalling the panel has started scanning out the horizontal back porch for a row.
*   __i915__: Name of the Intel gpu/display driver in Linux kernel.
*   __lane__: A data channel which transmits video.
*   __layer__: See plane (display).
*   __MIPI__: Mobile Industry Processor Interface.
*   __msm__: Name of the Qualcomm gpu/display driver in Linux kernel.
*   __MST__: DisplayPort Multi-Stream Transport.
*   __overlay__: See plane (display).
*   __PAVP__: Intel Protected Audio/Video Pipeline.
*   __PCLK__: Pixel Clock.
*   __PBN__: DisplayPort Payload Bandwidth Number.
*   __pitch__: Number of bytes in the active region of single row.j
*   __plane (display)__: DRM (driver) object which represents a portion of the screen filled with framebuffer data. Multiple planes can make up a scene and they can be layered on top of each other.
*   __plane (framebuffer)__: A buffer containing a single layer of data in a multi-planar framebuffer.
*   __PSR__: Panel Self Refresh.
*   __RBR__: DisplayPort Reduced Bit Rate (1.62 Gbit/s).
*   __SST__: DisplayPort Single-Stream Transport.
*   __TCON__: Panel timing controller.
*   __UHBR10__: DisplayPort Ultra High Bit Rate 10 (10 Gbit/s).
*   __UHBR13.5__: DisplayPort Ultra High Bit Rate 13.5 (13.5 Gbit/s).
*   __UHBR20__: DisplayPort Ultra High Bit Rate 20 (20 Gbit/s).
*   __VCPI__: DisplayPort Virtual Channel Payload Identifier.
*   __VRR__: Variable Refresh Rate.
*   __vertical active (vactive)__: The height of the active region.
*   __vertical back porch__: The portion of a blanking region which appears before the active region.
*   __vertical blank (vblank)__: The region before and after the active region where no video is being transmitted.
*   __vertical front porch__: The portion of a blanking region which appears after the active region.
*   __vertical sync (vsync)__: An interrupt signalling the panel has started scanning out the vertical back porch.
*   __VESA__: Standards body responsible for DP, DSC and other common display standards.


## Toolchain (compiler/debugger/linker/etc...)

*   __ASan, LSan, MSan, TSan__: [AddressSanitizer](https://www.chromium.org/developers/testing/addresssanitizer), [LeakSanitizer](https://www.chromium.org/developers/testing/leaksanitizer),
    [MemorySanitizer](https://www.chromium.org/developers/testing/memorysanitizer), and [ThreadSanitizer](https://www.chromium.org/developers/testing/threadsanitizer-tsan-v2) bug detection tools used in
    Chromium testing. ASan detects addressability issues (buffer overflow, use
    after free etc), LSan detects memory leaks, MSan detects use of
    uninitialized memory and TSan detects data races.
*   __AFDO__: Automatic FDO; see FDO & PGO.
*   __FDO__: Feedback-Directed Optimization; see AFDO & PGO.
*   __fission__: A new system for speeding up processing of debug information
    when using GCC; see [this page](https://gcc.gnu.org/wiki/DebugFission) for
    more details.
*   __gold__: The GNU linker; a newer/faster open source linker written in C++
    and supporting threading.
*   __ICE__: Internal Compiler Error; something really bad happened and you
    should file a bug.
*   __PGO__: Profile Guided Optimization; see AFDO & FDO.

## ChromiumOS

*   __board__: The name of the system you're building ChromiumOS for; see the
    [official ChromeOS device list](https://www.chromium.org/chromium-os/developer-information-for-chrome-os-devices)
    for examples.
*   __build_target__: The new, preferred term for board.
*   __model__: Model generally refers to a ChromeOS device that is unique in
    the market. A model typically maintains the major hardware components of
    its parent board but may vary in minor elements of one or more of: physical
    design, OEM, or ODM.
*   __platform__: Platform may refer to any of the following: (1) a System on a
    Chip family, (2) a ChromeOS device, (3) the ChromeOS Platform Team.
*   __image__: A set of binary files created from the ChromeOS source code
    that contains everything needed to run ChromeOS on a device. There are
    numerous types of images, including unsigned build images (which can be
    deployed on a device for development and testing), signed build images
    (which support secure verified boot), and Final Shipping Images (FSI, which
    are deployed to a device during manufacturing).
*   __manifest__: Refers to ChromeOS's [Repo](https://www.chromium.org/chromium-os/developer-library/reference/tools/repo-tool/)
    manifest. A Repo manifest is an XML file or set of XML files that describes
    the Git repositories and refs that make up a ChromeOS image. See
    [Local & Remote Source Tree Layouts](https://www.chromium.org/chromium-os/developer-library/reference/development/source-layout/).
*   __buildspec__: A manifest where every git repository is pinned to a specific
    git revision, used to represent a specific build (version) of ChromeOS.
*   __snapshot__: A manifest where every git repository is pinned to a specific
    git revision, created regularly by annealing at regular time intervals from
    tip-of-tree.
*   __firmware__: Software that provides low-level control of a device's
    hardware. Perhaps the two most important pieces of firmware on a Chromebook
    are for the Application Processor (AP) and Embedded Controller (EC), but
    there are many others, for example in WiFi adapters, fingerprint sensors,
    touchscreen controllers, and many more. External peripherals such as USB-C
    docks and Bluetooth accessories may also have updatable firmware.
*   __devserver__: System for updating packages on a ChromiumOS device
    without having to use a USB stick or doing a full reimage. See the
    [Dev Server page](https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/docs/devserver.md).
*   __powerwash__: Wiping of the stateful partition (system & all users) to
    get a device back into a pristine state. The TPM is not cleared, and Lockbox
    is kept intact (thus it is not the same as a factory reset). See the
    [Powerwash design doc](https://www.chromium.org/chromium-os/chromiumos-design-docs/powerwash).


## ChromiumOS Build

Terms related to building ChromiumOS.

*   __buildroot__: The buildroot refers to the
    source root of the checkout that is being built. This term is rarely used,
    but in certain contexts is necessary to distinguish between the current
    process' source root, and that of the checkout that is actually being built.
*   __chroot__: A chroot jail is used to isolate the CrOS SDK. The term chroot
    refers to an instance of the SDK for our contexts.
*   __sysroot__: The term sysroot comes from portage. With respect to ChromeOS,
    a sysroot is where a build target (board) is built. When completed, sysroots
    essentially have the filesystem you will find on a device's root partitions.
    By default, they can be found in /build/ inside your chroot.
*   __uprev__: The process of updating the revision (or version) of a package.
    The purpose is to produce a new ebuild with a higher overall version number
    to allow the build system to pick up changes made to the package. Packages
    that inherit from `cros-workon` can be automatically uprevved by the
    builders. There is a generic process that covers most packages, and some
    specialized processes for a few specific packages (e.g. chrome, android).
*   __overlay__: Directory on disk with Portage metadata, forming a collection
    of packages and profiles. Portage uses the set of overlays visible for
    a given board and composes it all into a build.
    See [Gentoo Docs for overlays](http://wiki.gentoo.org/wiki/Ebuild_repository).
    Examples:
    [`chromiumos`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/HEAD:src/third_party/chromiumos-overlay/),
    [`chipset-stnyridge`](http://cs/chromeos_public/src/overlays/chipset-stnyridge/) (Google-internal),
    [`amd64-generic`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/HEAD:src/overlays/overlay-amd64-generic/).
*   __private overlay__: Per-device specialized packages on top of an
    existing overlay. Could contain ebuild/config for proprietary targets
    such as firmware, media drivers, etc. Example:
    [`grunt-private`](http://cs/chromeos_internal/src/private-overlays/overlay-grunt-private/)
    (Google-internal).
*   __profile__: One or more files where the majority of board customization
    happens -- `USE` customizations across all packages in `make.defaults`,
    or `USE` and `KEYWORDS` on a per-package basis (via the `package.use`
    and `package.keywords` files, respectively).
    See [Gentoo Docs for profiles](https://wiki.gentoo.org/wiki/Profile_(Portage)).
    Example: [amd64-generic:base](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/HEAD:src/overlays/overlay-amd64-generic/profiles/base/)
*   __stepping stone__: An intermediate OS version that a device must update to
    before it can upgrade to newer versions.  e.g. CrOS R50 must update to R72
    before it can upgrade to R100.
*   __variant__:  A CrOS-specific deprecated mechanism, still in-use by some
    boards, to share settings across similar boards.
*   __`USE` flag__: A Portage mechanism to enable/disable features for packages,
    e.g. support for X11.
*   __Keyword flag__: Used in `cros_workon` for uprevving packages.
