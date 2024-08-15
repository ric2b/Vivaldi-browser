---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: source-layout
title: Local & Remote Source Tree Layouts
---

Here we discuss the layout of files in the local source checkout as well as the
remote Git server.
The goal is both to document the current state of things and to provide guidance
when adding new repos.

Some repos might exist in places that don't follow these guidelines, but the
goal is to align rather than keep adding exceptions.

[TOC]

## Local Checkout

This is the layout as seen when you get a full CrOS checkout.
We cover the source repos separately from the generated paths to make the
provenance of the paths clearer.

### Source directories

Some of these are marked as *(non-standard)* to indicate that, someday maybe,
the repo would be moved to a more appropriate path.

*   `chromeos-admin/`: *(non-standard)* Internal repo used by the build/infra
    team to hold scripts and tools for managing various infrastructure systems.
*   [`chromite/`][chromite]: All of CrOS's build tools live here.
*   [`crostools/`][crostools]: *(non-standard)* Signing instructions for
    builders.
*   [`docs-internal/`][docs-internal]: Internal documentation.
*   `infra/`: Projects for various CrOS infra systems.
*   `infra_internal/`: Internal projects for various CrOS infra systems.
*   `infra_virtualenv/`: Python virtualenv tooling for CrOS infra projects.
*   [`manifest/`][manifest]: Public ChromiumOS manifest.
*   [`manifest-internal/`][manifest-internal]: Internal ChromeOS manifest.
*   `src/`: Most source code people care about.
    *   `aosp/`: Mirrors of repos from the [Android project][AOSP GoB].
    *   `chromium/`: Mirrors of repos from the Chromium project.
    *   `config/`: Schema definitions and utilities for configuring CrOS
        hardware and software; used to generate config payloads to drive
        CrOS builds, manufacturing, etc.
    *   [`overlays`/][board-overlays]: Single git repo holding all public
        board/etc... overlays.
        *   `baseboard-*/`: Overlays for "base boards".  Content that is for the
            specified board only and is "software stack" independent.
        *   `chipset-*/`: Overlays for specific chipsets/SoCs.  Board or OS
            specific details don't generally belong in here.
        *   `overlay-*/`: Overlays for the actual board.  These often extend a
            baseboard or chipset and blend in a project or OS stack.
        *   `project-*/`: Software stacks for standalone projects.  These are
            used to share code easily among different boards.
    *   `partner_private/`: Separate git repos for internal partner repos.
        These hold source repos that isn't open source or authored by Google.
    *   `platform/`: Separate git repos for Google/ChromiumOS authored
        projects.  Here are a few notable repos.
        *   [`dev/`][dev-util]: Various developer and testing scripts.
            *   [`contrib/`][contrib]: Where developers can share their personal
                scripts/tools/etc...  Note: completely unsupported.
        *   [`signing/`][cros-signing]: Signing server code.
        *   [`vboot_reference/`][vboot_reference]: Code and tools for working
            with verified boot, as well as image signing and key generation
            scripts.
        *   `$PROJECT/`: Each project gets its own directory.
    *   [`platform2/`][platform2]: Combined git repo for ChromiumOS system
        projects.  Used to help share code, libraries, build, and test logic.
    *   `private-overlays/`: Separate git repos for internal partner repos to
        simplify sharing with specific partners.  This mirrors the structure
        of `src/overlays/`.
        *   `baseboard-*-private/`: Internal base boards shared with specific
            partners.
        *   `chipset-*-private/`: Internal chipsets shared with specific
            partners.
        *   [`chromeos-overlay/`][chromeos-overlay]: ChromeOS settings &
            packages (not ChromiumOS) not shared with partners.
        *   [`chromeos-partner-overlay/`][chromeos-partner-overlay]: ChromeOS
            settings & packages shared with all partners.
        *   `overlay-*-private/`: Internal boards shared with specific partners.
        *   `project-*-private/`: Internal projects shared with specific
            partners.
    *   [`repohooks/`][repohooks]: Hooks used by the `repo` tool.  Most notably,
        these are the test run at `repo upload` time to verify CLs.
    *   [`scripts/`][crosutils]: Legacy ChromiumOS build scripts.  Being moved
        to chromite and eventually dropped.  No new code should be added here.
    *   `third_party/`: Separate git repos for third party (i.e. not Google or
        ChromiumOS authored) projects, or projects that are intended to be used
        beyond CrOS.  These are often forks of upstream projects.
        *   [`chromiumos-overlay/`][chromiumos-overlay]: Custom ebuilds for just
            ChromiumOS.  Also where forked Gentoo ebuilds are kept (instead of
            `portage-stable/`).
        *   [`eclass-overlay/`][eclass-overlay]: Overlay for custom eclasses
            that are used by all other overlays (board, project, etc...).
        *   [`kernel/`][kernel]: The Linux kernel.  Separate checkouts are used
            for each version we track.
        *   [`portage-stable/`][portage-stable]: Unmodified ebuilds copied from
            Gentoo.  If you want to make custom changes, packages are moved to
            [chromiumos-overlay]/ instead.  See the [Package Upgrade Process]
            for details on updating these.
        *   [`portage_tool/`][portage_tool]: Fork of Gentoo's portage project
            (i.e. `emerge` and `ebuild`).
        *   `$PROJECT/`: Each project gets its own directory.
    *   `weave/`: Mirrors of repos from the Weave project.

### Generated directories

These paths are all created on the fly and do not have any relationship to git
repos on the servers.
You should not normally need to manage these directly or clean them up.
In fact, you should avoid trying to do so to avoid breaking the build system.
If you're running out of space, use the `cros clean` command to help trim the
various caches and build directories safely.

*   `.cache/`: Various files cached by the build tools.  Designed to be shared
    between different CrOS checkouts and branch versions.
    *   `common/`: Various artifacts cached by [chromite] tools.
    *   `distfiles/`: Artifacts used while running emerge.  Files are stored
        directly in here to share between the SDK & all targets.
        *   `ccache/`: Compiler cache used by the toolchain to save objects.
        *   `host/`: Legacy symlink to `./` from when caches were separated.
        *   `target/`: Legacy symlink to `./` from when caches were separated.
    *   `sdks/`: Copies of the initial SDK tarball (for `chroot/`).
*   `.repo/`: Internal state used by [repo] to manage the checkout.  You usually
    don't have to mess around with paths under here, so this is more of an
    informational section.  See the upstream [Repo internal filesystem layout]
    document for more details.
    *   [`local_manifests/`][Local manifests]: Local manifests to add custom
        repos to the local checkout.
    *   `manifests/`: Local checkout of the manifest repo using the
        `--manifest-branch` setting from `repo init`.  This is used during
        all repo operations like `repo sync` and `repo upload`.
    *   `manifests.git/`: Local bare clone of the remote manifest repo.
        This is the `--manifest-url` setting used during `repo init`.
    *   `manifest.xml`: Symlink to the manifest in the `manifests/` directory.
        This is the `--manifest-name` setting used during `repo init`.
    *   `project-objects/`: Used by `repo` to hold git objects for all the
        checked out repos.  The `.git/` subdir will often symlink to paths
        under here.
    *   `projects/`: Same as `project-objects/`, but for non-objects git files.
    *   `repo/`: Local checkout of the [repo] tool itself.  This is the code
        behind every `repo` command.  This is the `--repo-url` setting used
        during `repo init`.
*   [`chroot/`][chroot]: The full chroot used by `cros_sdk` where boards are
    built.  See the [filesystem layout] document for more details.
    Use `cros_sdk --delete` to safely remove this directory.
*   `chroot.img`: A disk image for the `chroot/` directory.  Provides hermetic
    guarantees about the filesystem, and helps speed up things like snapshots.
    Use `cros_sdk --delete` to safely remove these paths.
*   `src/`:
    *   `build/`: All final artifacts & images are written here (e.g.
        `cros build-image` outputs).
        *   `images/$BOARD/`: Every board goes into a unique output directory.
            *   `latest/`: A symlink that is automatically updated to point to
                the latest build for this board.
            *   `$VERSION/`: Each version gets a unique directory.

## Git Server Layout

This is the layout as organized on the Git server.
This isn't comprehensive, but should provide enough guidance here.

Note that the layout (and even specific names) of repos on the server do not
need to exactly match the layout of the local source checkout.
The manifest used by [repo] allows for the paths and names to be completely
independent.
Often times they are pretty similar since we try to keep the naming policies
coherent as it's less confusing that way.

### Public Chromium GoB

This is the public https://chromium.googlesource.com/ site.

The majority of repos should live under either `platform/` or `third_party/`.
It's very uncommon to create projects outside of those paths.
If you want to create a new project somewhere else, please check with the
build/infra team first using [chromium-os-dev@chromium.org][Contact].

*   `aosp/`: Mirrors of Android projects.  Should use the same layout as on the
    [AOSP GoB] host.
*   `apps/`: Various ChromiumOS applications.  Usually these are "Chrome Apps"
    written in HTML/JavaScript/CSS.
*   `chromium/`: Chromium (the browser) related projects.  These are managed
    entirely by the browser team.
*   `chromiumos/`: ChromiumOS related projects.  Most ChromeOS people will
    only ever work on projects under these paths.
    *   [`chromite/`][chromite]: All of CrOS's build tools live here.
    *   `containers/`: Projects for the [VM/containers] project.  Mainly for
        projects that don't integrate directly into CrOS builds, otherwise the
        projects are stored under `platform2/vm_tools/`.
        *   `$PROJECT/`: Each project gets its own repo.
    *   `infra/`: Projects for various CrOS infra systems.
    *   [`manifest/`][manifest]: The manifest use by `repo init` and `repo sync`
        to get a public ChromiumOS checkout.
    *   `overlays/`: Various ebuild overlays used to build ChromiumOS.  These
        are where all the packages and their compile/install logic live.
        *   [`board-overlays/`][board-overlays]: All public board overlays.
        *   [`chromiumos-overlay/`][chromiumos-overlay]: Custom ebuilds for just
            ChromiumOS.  Also where forked Gentoo ebuilds are kept (instead of
            [portage-stable]/).
        *   [`eclass-overlay/`][eclass-overlay]: Overlay for custom eclasses
            that are used by all other overlays (board, project, etc...).
        *   [`portage-stable/`][portage-stable]: Unmodified ebuilds from Gentoo.
            When making custom changes, packages are moved to
            [chromiumos-overlay]/ instead.  See the [Package Upgrade Process]
            for details on updating these.
    *   `platform/`: Projects written by the ChromiumOS project, or some Google
        authored projects.  Many of these have since been merged into
        [platform2]/.
        *   `$PROJECT/`: Each project gets its own repo.
    *   [`platform2/`][platform2]: Combined git repo for ChromiumOS system
        projects.  Used to help share code, libraries, build, and test logic.
    *   [`repohooks/`][repohooks]: Hooks used by the `repo` tool.  Most notably,
        these are the test run at `repo upload` time to verify CLs.
    *   `third_party/`: Projects that were not written by ChromiumOS project,
        or projects that are intended to be used beyond CrOS.  See the [Forking
        upstream projects] section for more details.
        *   `$PROJECT/`: Each project gets its own repo.
*   `external/`: Read-only mirrors of various external projects.
    Do **not** create forks of upstream projects here.  See the [Forking
    upstream projects] section for more details.
    *   `gob/`: Mirrors of other GoB instances.  Uses a $GOB_NAME subdir
        followedby the path as it exists on the remote GoB server.
    *   `$HOST/`: Domain name for the hosting site.  e.g. `github.com`.
        Subpaths here tend to reflect the structure of the hosting site.

### Internal Chrome GoB

This is the internal https://chrome-internal.googlesource.com/ site.

The majority of repos should live under `overlays/`, `platform/`, or
`third_party/`.
It's very uncommon to create projects outside of those paths.
If you want to create a new project somewhere else, please check with the
build/infra team first using [chromeos-chatty-eng@google.com][Contact].

*   `android/`: Mirrors of internal Android projects.  Should use the same
    layout as on the [AOSP GoB] host.
*   `chrome/`: Chrome (the browser) related projects.  These are managed
    entirely by the browser team.
*   `chromeos/`: ChromeOS related projects.  Most ChromeOS people will only
    ever work on projects under these paths.
    *   `overlays/`: Separate git repos for internal partner repos to simplify
        sharing with specific partners.
        *   `baseboard-*-private/`: Internal base boards shared with specific
            partners.
        *   `chipset-*-private/`: Internal chipsets shared with specific
            partners.
        *   [`chromeos-overlay/`][chromeos-overlay]: ChromeOS settings &
            packages (not ChromiumOS) not shared with partners.
        *   [`chromeos-partner-overlay/`][chromeos-partner-overlay]: ChromeOS
            settings & packages shared with all partners.
        *   `overlay-*-private/`: Internal boards shared with specific partners.
        *   `project-*-private/`: Internal projects shared with specific
            partners.
    *   `platform/`: Internal projects written by the ChromeOS project, or some
        Google-authored projects.  Most projects should be open source though and
        live in the [Chromium GoB] instead.
    *   `third_party/`: Internal projects that were not written by ChromiumOS.
        Often used for early work before being made public, or to hold some code
        from partners.
    *   `vendor/`: Various partner-specific projects like bootloaders or firmware.
*   `external/`: Read-only mirrors of other projects.  Uses the same conventions
    as the [public Chromium GoB].

## Branch naming

*** note
**Note**: If forking an upstream repository, please see the [Forking upstream
projects] section for additional rules on top of what's described here.
***

We have a few conventions when it comes to branch names in our repos.
Not all repos follow all these rules, but moving forward new repos should.

When cloning/syncing git repos, only `heads/` and `tags/` normally get synced.
Any other refs stay on the server and are accessed directly.

Note: All the paths here assume a `refs/` prefix on them.
So when using `git push`, make sure to use `refs/heads/xxx` and not just `xxx`
to refer to the remote ref.

*   `tags/`: CrOS doesn't use git tags normally.  Repos that do tend to do so
    for their own arbitrary usage.  If they are used, they should follow the
    [SemVerTag] format (e.g. `v1.2.3`).
*   `heads/`: The majority of CrOS development happens under these branches.
    *   `factory-xxx`: Every device gets a unique branch for factory purposes.
        Factory developers tend to be the only ones who work on these.
        Typically it uses `<device>-<OS major version>.B` names.
    *   `firmware-xxx`: Every device gets a unique branch for maintaining a
        stable firmware release.  Firmware developers tend to be the only ones
        who work on these.  Typically it uses `<device>-<OS major version>.B`
        names.
    *   `main`: The normal [ToT] branch where current development happens.
    *   `master`: The historical [ToT] branch where development happened.  Not
        used in new projects, and being migrated to `main` in existing projects.
    *   `release-xxx`: Every release gets a unique branch.  When developers
        need to cherry pick back changes to releases to fix bugs, these are
        the branches they work on.  Typically it uses
        `R<milestone>-<OS major version>.B` names.
    *   `stabilize-xxx`: Temporary branches used for testing new releases.
        Developers pretty much never work with these and they're managed by
        TPMs.  Typically it uses `<OS major version>.B` names.
    *   `upstream/`: Read-only mirrors of upstream refs.
*   `sandbox/`: Arbitrary developer scratch space.  See the [sandbox]
    documentation for more details.

### Automatic m/ repo refs

Locally, repo provides some pseudo refs to help developers.
It uses the `m/xxx` style where `xxx` is the branch name used when running
`repo init`.
It often matches the actual git branch name used in the git repo, but there is
no such requirement.

For example, when getting a repo checkout of the `main` branch (i.e. after
running `repo init -b main`), every git repo will have a pseudo `m/main`
that points to the branch associated with that project in the [manifest].
In chromite, `m/main` will point to `heads/main`, but in bluez, `m/main`
will point to `heads/chromeos`.

In another example, when getting a repo checkout of the R70 release branch (i.e.
after running `repo init -b release-R70-11021.B`), every git repo will have a
pseudo `m/release-R70-11021.B` that points to the branch associated with that
project in the [manifest].
In chromite, `m/release-R70-11021.B` will point to `heads/release-R70-11021.B`,
but in kernel/v4.14, `m/release-R70-11021.B-chromeos-4.14`.

### Other refs

There are a few additional paths that you might come across, although they
aren't commonly used, or at least directly.

*   `changes/`: Read-only paths used by Gerrit to expose uploaded CLs and their
    various patchsets.  See the [Gerrit refs/for] docs for more details.
*   `for/xxx`: All paths under `for/` are a Gerrit feature for uploading commits
    for Gerrit review in the `xxx` branch.  See the [Gerrit refs/for] docs for
    more details.
*   `infra/config`: Branch used by [LUCI] for configuring that service.
*   `meta/config`: Branch for storing per-repo Gerrit settings & ACLs.  Usually
    people use the [Admin UI] in each GoB to manage these settings indirectly,
    but users can manually check this out and upload CLs by hand for review.
    See the [Gerrit project config] docs for more details.

### Local heads/ namespaces

You might see that, depending on the repo, the remote branches look like
`remotes/cros/xxx` or `remotes/cros-internal/xxx` or `remotes/aosp/xxx`.
The `cros` and such names come from the `remote` name used in the [manifest]
for each `project` element.

For example, the [manifest] has:
```xml
<manifest>
  <remote  name="cros"
           fetch="https://chromium.googlesource.com"
           review="https://chromium-review.googlesource.com" />

  <default revision="refs/heads/main"
           remote="cros" sync-j="8" />

  <project path="chromite" name="chromiumos/chromite" ?>
  <project path="src/aosp/external/minijail"
           name="platform/external/minijail"
           remote="aosp" />
</manifest>
```

The `default` element sets `remote` to `cros`, so that's why it shows up in
the chromite (whose `project` omits `remote`) repo as `remotes/cros/xxx`.

The minijail project has an explicit `remote=aosp`, so that's why it shows up
as `remotes/aosp/xxx` in the local checkout.

## Forking upstream projects

Sometimes we want to fork upstream projects to do significant development.
We have some guidelines to keep things consistent.

### Local checkout paths

You generally want to put projects under `src/third_party/$PROJECT/`.
See the [Local Checkout] section for more details.

### Server paths

Forked repositories live under `/chromiumos/third_party/` on the [Public
Chromium GoB] and under `/chromeos/third_party/` on the [Internal Chrome GoB].

Normally the project is placed directly in that namespace, but for groups of
upstream projects, using similar name grouping is acceptable.  For example,
coreboot uses `/chromiumos/third_party/coreboot.git`, and it has additional
repos under `/chromiumos/third_party/coreboot/`.  If cloning a number of repos
from a GitHub project, that layout may be replicated here: `github.com/xxx/yyy`
turns into `/chromiumos/third_party/xxx/yyy`.  We don't require people to mangle
names just to keep a flat namespace under `/chromiumos/third_party/`.

Do **not** create forks in the `/external/` namespace.  Our GoB hosts are shared
between many projects & teams, and this namespace is also shared.  Think of them
like global variables: one team (ab)using it as a fork can surprise or upset
another team who was expecting it to be a read-only upstream copy.

### Ref (branches & tags) management

These rules exist on top of the common [Branch naming] rules, not in place of.
We often need to fork these repositories as part of our release process.

Note: All the paths here assume a `refs/` prefix on them.

*   `tags/`: Upstream tags (if they exist) should use the same names as they
    exist upstream.  They should **not** be moved under e.g. `tags/upstream/`.
    This makes it easier to sync & compare with upstream.  If you want to use
    CrOS-specific tags, it's recommended you use a convention similar to the
    respective upstream project, and with a qualifier to avoid clashing with
    future upstream tags.  Discretion is left to the developers maintaining
    the project, but feel free to ask for advice on the CrOS mailing lists.
*   `heads/`: CrOS branches should live here directly.  Upstream refs should
    **not** be mirrored directly here.
    *   `main` & `master`: *(Not recommended)*
        If you only ever maintain one branch forked from upstream, you may use
        the same name as upstream does.  If they're using `master`, you may
        also use `master`.  But you should not try to rename to `main` if
        upstream is using `master`, or vice-versa.  This is unnecessarily
        confusing to developers trying to compare the two.
    *   `chromeos`: *(Recommended)*
        When forking an upstream project, this is used for [ToT] development.
        It is intended to be periodically synced with the corresponding upstream
        branch (e.g. `master` or `main`), although that is not a requirement.
        This name clearly indicates that it contains CrOS-specific changes.
    *   `chromeos-xxx`: When forking a project with periodic major releases,
        this convention allows for tracking each release in parallel.  For
        example, `chromeos-2.0` would track upstream's 2.0 release, and then
        `chromeos-3.3` would track upstream's 3.3 release.  Normally teams
        will do a full rebase when creating the new series, but that's not
        strictly a requirement.
    *   `upstream/`: This namespace is reserved for mirroring of all upstream
        heads.  So `refs/heads/*` is copied to `refs/heads/upstream/*`.  This
        makes it easy to compare, rebase, cherry-pick, etc... the local CrOS
        branches with the upstream work.

#### Automatic syncing

By following the conventions outlined above, we can easily support automatic
syncing with the respective upstream projects.
That way you do not need to manually fetch & push tags, or fetch & rename &
push refs from upstream's `refs/heads/` to our forked `refs/heads/upstream/`.

### Switching major releases

Some projects track major release series and switch between them periodically.
If you use the `chromeos-xxx` ref naming convention, this makes things easier:

*   Multiple versions can be checked out by the manifest in parallel.
*   Multiple versions can be built by a single ebuild.
    *   Alternatively, you can have multiple ebuilds so it's one-to-one mapping
        between source repos & an ebuild.
*   USE flags may be used to control version selection which allows for slow
    rollout across boards (if desired).

### Testing upstream directly

*** note
This process is undergoing evaluation by teams.
***

It is possible to have an ebuild track the upstream branch directly as an
informational or local developer testing.
The requirement is that it not be part of the normal release or build series.

For working examples, see these ebuilds:

*   https://chromium.googlesource.com/chromiumos/manifest/+/4a6baccabc7fba397be9a059b7fd545654e7b4e0/full.xml#346
*   https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/ed40a4628ae76226addd95fc1686758607f776da/net-wireless/bluez/bluez-9999.ebuild

## FAQ

### How do I create a new repository on the server?

File a bug for the [Infra>Git>Admin rotation].
All these bugs are marked [RVG] by default so providing non-public info is OK.
~24hr response time can be expected.

Please do not ask specific people to create repositories for you, even if you
happen to know they might have credentials to do so.
The bug queue above goes into an oncall rotation queue to help distribute load.

If you are not a committer, make sure to mention someone by their @chromium.org
or @google.com address who can help vouch/verify the request.

Be sure to fill out the template, and to provide these details:
*   The name of the new repository on the server.
    *   See the [Server Layout] section above for examples.
*   Whether it should be a read-only mirror, a fork of an upstream project, or
    a completely new (empty) project.
*   Any non-standard permissions/access to the repository.
    NB: We try to avoid this, but we may consider anything that is reasonable
    and allowed by our [security policy](/chromium-os/developer-library/guides/development/contributing/#policies).

Feel free to provide any other information you think would help with processing
the request.
We don't usually need full details (e.g. [PRD]/design docs), but usually "too
much information" is not the problem we have :).

### How do I mirror an upstream repository onto the git server?

See the [How do I create a new repository on the server?](#server-new-repo)
FAQ above.

### How do I add a repo to the manifest?

*** note
The repository must already exist on the server.
See [How do I create a new repository on the server?](#server-new-repo) first.
***

If the repo is public (i.e. exists on the [Chromium GoB]), then update the
[full.xml][internal full.xml] file in the internal [manifest-internal] repo.
Do not modify the [full.xml] file in the public [manifest] repo. The public
[manifest] repo will automatically sync with the internal [manifest-internal]
repo.

If the repo is private (i.e. exists on the [Chrome GoB]), then update the
[internal_full.xml] file in the internal [manifest-internal] repo.

You can follow the examples in the files already.

### How do I change branches for an existing repo?

Set the `revision` attribute in the relevant `<project .../>` element in the
manifest files.

See the previous question about adding a repo for details on which repos you
will need to update.

### How do I add a new repo from a new host to the manifest?

Our source code is hosted exclusively on the [Chromium GoB] & [Chrome GoB]
(with a few exceptions on [AOSP GoB], but we don't need to get into that).
We do not want to add any more GoB hosts to our manifests.

This is problematic for many infra, release, and developer reasons:
*    We (Chrome Operations & CrOS Infra) need admin access to the GoB host.
     Without it, we cannot address oncall issues, and instead we have to track
     down whoever has admin access to the hosts in question, and that is often
     not documented anywhere.  These prolong outages.
*    Access control management is different for bots & for developers.  We
     need to make sure everyone is able to access the source code, and the
     way ACLs are configure affects resource accounting.
*    Resource guarantees (i.e. SLAs) changes based on the project.  CrOS is
     quite large nowadays, and we can easily use up all the quota allocated
     for smaller projects.  That upsets both us (CrOS) & them (non-CrOS).
*    We need to guarantee the repositories stay accessible.  Projects might
     not realize or remember that CrOS is pulling their source code, so if
     the repositories get locked down, all our bots & devs immediately break.
     Even if we stopped using the repository in ToT, we have to guarantee our
     historical manifests continue to work (for firmware branches, these can be
     8+ years old!).
*    We need to guarantee the repositories don't change history.  See above.
*    We need to guarantee the repositories don't rename or delete branches.
     See above.
*    When creating release branches, we need to create a branch in every
     repository so we can backport/cherry pick fixes.  When there is no release
     branch in a repository, it makes it extremely more difficult to address
     security or critical bugs (like data loss).

These are not hypotheticals, they are failure modes the project has run into
in the past and caused widespread outages.

If you want to use a project that's hosted on another GoB instance, see
[How do I mirror an upstream repository onto the git server?].

Do not attempt to add any new remotes to CrOS manifests without first starting
a discussion on [chops-cros-help@](/chromium-os/developer-library/guides/who-do-i-notify/contact/) and getting approval from CrOS
infra.

### How do I test a manifest change?

The [manifest] and [manifest-internal] repos in the checkout are purely for
developer convenience in making & uploading changes.
They are not used for anything else.

When you run `repo sync` in your checkout, that only looks at the manifest under
the `.repo/manifests/` directory.
That is an independent checkout of the manifest that was specified originally
when running `repo init`.

So the flow is usually:

*   Write the CLs in  [manifest-internal].
*   Extract the change from the repo, using `git format-patch -1` to create a patch you can apply.
*   Apply the change to the local directory.
    *   Run `cd .repo/manifests/`.
    *   Use `git am` to apply the patch from above.
*   Run `repo sync` to use the modified manifest.
*   Do whatever testing/validation as makes sense.

At this point, your checkout is in a modified state.
That means any CLs other people are landing to the manifest will be pulled in
when you run `repo sync`.
If those changes cause conflicts, your checkout won't sync properly.
Thus your final step should be to remove your changes.

*   Reset the branch back to the remote state.
    This will throw away all local changes!
    *   You can usually run the following commands:
        ```
        # This might say "No rebase in progress" which is OK.
        git rebase --abort
        git reset --hard HEAD^
        git checkout -f
        ```
    *   If those "simple" commands didn't work, try this more complicated one:
        ```
        git reset --hard "remotes/$(git config --get branch.default.remote)/$(basename "$(git config --get branch.default.merge)")"
        ```
*   If those commands don't work, you basically want to figure out what the
    remote branch is currently at and reset to it.

If your manifest CL hasn't landed yet, then when you run `repo sync`, the
changes you made will be lost.
So if you were adding a repo, it will be deleted again from the checkout.
If you were removing a repo, it will be added again to the checkout.

### How do I initialize a new repository?

Once the repository is in the manifest (see the previous FAQs), you can
`repo sync` to get it locally, and then add/commit/upload files like normal.

The manifest CLs do not need to be merged first -- if you want to generate
content ahead of time, you can use the testing steps above to add it to your
local manifest, and then start uploading CLs right away.

### How do I create a new branch/tag in a repository on the git server?

You don't :).
File a bug for the [Infra>Git>Admin rotation] with your exact request and you
should hear back within ~24hrs.

### Where do I put ebuilds only for Googlers and internal ChromeOS builds?

The [chromeos-overlay] holds all packages & settings for this.
It is not made public or shared with any partners, so this is for restricted
packages and features only.

### Where do I put ebuilds to share with partners?

The [chromeos-partner-overlay] holds all packages & settings for this.
It is not made public, but it is shared among *all* partners.

If you want to share a package with specific partners for a specific board,
then put it in the respective `src/private-overlays/xxx-private/` overlay.

### Can I put ebuilds for private changes into public overlays?

No.
All ebuilds in public overlays must be usable by the public.
Even if the ebuild itself contains no secret details (e.g. it just installs a
binary from a tarball), if the tarball is not publicly available, then we do
not put the ebuild in the public overlays.

Use one of the respective private overlays instead.
See the previous questions in this FAQ for more details.


[Admin UI]: https://chromium-review.googlesource.com/admin/repos
[AOSP GoB]: https://android.googlesource.com/
[Branch naming]: #branch-naming
[Chrome GoB]: https://chrome-internal.googlesource.com/
[chromeos-overlay]: https://chrome-internal.googlesource.com/chromeos/overlays/chromeos-overlay/
[chromeos-partner-overlay]: https://chrome-internal.googlesource.com/chromeos/overlays/chromeos-partner-overlay/
[chromite]: https://chromium.googlesource.com/chromiumos/chromite/
[Chromium GoB]: https://chromium.googlesource.com/
[chromiumos-overlay]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/
[chroot]: filesystem_layout.md#SDK
[Cq-Depend]: /chromium-os/developer-library/guides/development/contributing/#CQ-DEPEND
[Contact]: /chromium-os/developer-library/guides/who-do-i-notify/contact/
[contrib]: https://chromium.googlesource.com/chromiumos/platform/dev-util/+/HEAD/contrib/
[cros-signing]: https://chrome-internal.googlesource.com/chromeos/cros-signing/
[crostools]: https://chrome-internal.googlesource.com/chromeos/crostools/
[crosutils]: https://chromium.googlesource.com/chromiumos/platform/crosutils/
[dev-util]: https://chromium.googlesource.com/chromiumos/platform/dev-util/
[docs-internal]: https://chrome-internal.googlesource.com/chromeos/docs/
[eclass-overlay]: https://chromium.googlesource.com/chromiumos/overlays/eclass-overlay/
[Forking upstream projects]: #forking-upstream
[Infra>Git>Admin rotation]: https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git
[Internal Chrome GoB]: #gob-chrome
[internal full.xml]: https://chrome-internal.googlesource.com/chromeos/manifest-internal/+/HEAD/full.xml
[filesystem layout]: filesystem_layout.md
[full.xml]: https://chromium.googlesource.com/chromiumos/manifest/+/HEAD/full.xml
[Gerrit project config]: https://gerrit-review.googlesource.com/Documentation/config-project-config.html
[Gerrit refs/for]: https://gerrit-review.googlesource.com/Documentation/concept-refs-for-namespace.html
[internal_full.xml]: https://chrome-internal.googlesource.com/chromeos/manifest-internal/+/HEAD/internal_full.xml
[kernel]: https://chromium.googlesource.com/chromiumos/third_party/kernel/
[Local Checkout]: #local-layout
[Local manifests]: https://gerrit.googlesource.com/git-repo/+/HEAD/docs/manifest-format.md#Local-Manifests
[LUCI]: https://chromium.googlesource.com/infra/luci/luci-py/
[manifest]: https://chromium.googlesource.com/chromiumos/manifest/
[manifest-internal]: https://chrome-internal.googlesource.com/chromeos/manifest-internal/
[board-overlays]: https://chromium.googlesource.com/chromiumos/overlays/board-overlays/
[Package Upgrade Process]: /chromium-os/developer-library/guides/portage/package-upgrade-process/
[platform2]: https://chromium.googlesource.com/chromiumos/platform2/
[portage-stable]: https://chromium.googlesource.com/chromiumos/overlays/portage-stable/
[portage_tool]: https://chromium.googlesource.com/chromiumos/third_party/portage_tool/
[PRD]: /chromium-os/developer-library/glossary/#PRD
[Public Chromium GoB]: #gob-chromium
[Repo internal filesystem layout]: https://gerrit.googlesource.com/git-repo/+/HEAD/docs/internal-fs-layout.md
[repo]: https://gerrit.googlesource.com/git-repo
[repohooks]: https://chromium.googlesource.com/chromiumos/repohooks/
[RVG]: /chromium-os/developer-library/glossary/#RVG
[sandbox]: /chromium-os/developer-library/guides/development/contributing/#sandbox
[Server Layout]: #server-layout
[SemVerTag]: https://semver.org/spec/v1.0.0.html#tagging-specification-semvertag
[ToT]: /chromium-os/developer-library/glossary/#ToT
[vboot_reference]: https://chromium.googlesource.com/chromiumos/platform/vboot_reference/
[VM/containers]: /chromium-os/developer-library/guides/containers/containers-and-vms/
