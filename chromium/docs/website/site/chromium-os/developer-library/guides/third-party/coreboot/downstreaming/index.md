--------------------------------------------------------------------------------

breadcrumbs: - - /chromium-os/developer-library/guides - ChromiumOS > Developer
Library > Guides page_name: downstreaming

## title: Downstreaming coreboot into the Chromium repo.

[TOC]

## What is downstreaming?

*   We have a need to maintain forks of third-party projects:
    *   Hold back breaking changes (or revert them if they happen to land).
    *   Resolve cross-project dependencies in our code.
    *   Maintain local patches.
*   ChromiumOS is an upstream-first project :-)
    *   You should always land your changes upstream before we should apply them
        to our forks.
*   Terminology:
    *   Upstream: The official source tree of the third-party project.
    *   Downstream: Our fork of that tree.
    *   Dowstreaming: The process of applying commits from upstream to
        downstream.

## What if my change can't be submitted upstream?

*   In general chromeOS has a strong upstream first philosophy. If you can’t
    upstream your change, it’s worth considering if there’s another solution or
    if this is actually desired.
*   Temporary changes:
    *   Temporary changes may be required to workaround known issues during
        bringup.
        *   Ideally we would still submit these changes upstream with
            documentation and a TODO with an open bug number
        *   If that can’t be done (For example the change is in common code) an
            ebuild can be modified within the chromium repo to apply patches
            locally
            *   See [crrev/i/5994551] for an example
*   Permanent changes:
    *   There are few reasons to do this, but some may be to apply blobs for an
        unreleased SoC or to facilitate an embargo. In this case, the changes
        can be applied to a private overlay and appended to the build.
        *   See [crrev/i/5866621] for an example

## Downstreaming Organization

There’s a Google internal [downstreaming rotation] to facilitate the
downstreaming process.

*   You must be an owner of src/third_party/coreboot to help with the downstream
    rotation. You can put a CL up to add yourself to [OWNERS.cros_ap]

## Downstreaming Process

The downstreaming process is separated into two steps.

### Copybot

[Copybot] is a custom tool made by the ChromeOS Zephyr team for handling the
task of pulling commits from an external repo into Gerrit. The tool was
developed as the Zephyr team was having very similar issues with [Copybara] as
coreboot was.

In general, Copybot functions in a very similar manner as Copybara does, with
the exception that it does cherry-picks instead of resetting the tree
iteratively. Copybot also has some controls to make it ignore certain CLs when
you need it to be a little more forgiving.

#### Running Copybot

In general, you probably won't need to run Copybot yourself. It runs nightly
automatically at ~4:30 AM MT. Our instance, with a few others, can be observed
on [go/cros-luci]. See [copybot.star] for configuration information for the
builders.

Should it be required, you can manually trigger a run using the “trigger” button
on the builder page.

### Review/submission

1.  Manual
    1.  Check the status of the coreboot copybot jobs(links above) a. If there
        are failures, resolve them to ensure no CLs are missed (copybot will
        skip commits with merge conflicts)
    1.  Check daily for [new CLs from Copybot].
    1.  Mark the CLs CR+2. From the Gerrit CLI: `bash gerrit label-cr $(gerrit
        --raw search 'hashtag:coreboot-downstream status:open
        -hashtag:copybot-skip') 2`
    1.  Send the CLs to the Commit Queue. From the Gerrit CLI: `bash gerrit
        label-cq $(gerrit --raw search 'hashtag:coreboot-downstream status:open
        -hashtag:copybot-skip') 2`
        1.  Note: LUCI and the CQ can trip-up when there's ~80+ CLs. If there's
            a long backlog, it's recommended to send about 60-80 at a time.
    1.  Monitor for CQ failures or other aspects of attention.
1.  Script [coreboot_downstream.py] performs steps 1-3 from the manual steps
    above. Run it from the full chromite path
    1.  Dry run what the script would do with `bash ./coreboot_downstream
        --dry-run`
    2.  To clear all the downstreaming CLs from your attention list: `bash
        ./coreboot_downstream clear_attention` (or) `bash for cr in $(gerrit
        --raw search 'hashtag:coreboot-downstream attention:me'); do gerrit
        attention --ne $cr ~$USER@google.com; done`
    3.  When CQ fails midstack Downstream up to that commit using: `bash
        coreboot_downstream --limit {NUMBER_OF_CLS}` or `bash
        coreboot_downstream --stop-at {GERRIT_CHANGE_ID}` ## Errors ### Merge
        Conflicts If there's a merge conflict when cherry-picking, the
        downstreamer will get a "build failed" email. If you are not getting the
        emails, ensure you are included in the coreboot_notify_list inside
        [copybot.star]. The downstreamer should click the link in the failure
        e-mail, and there'll be a list of failed commits at the very bottom of
        the log. Please manually cherry-pick these CLs, resolve the merge
        conflicts, and upload them.

*   If there is a reason to not downstream the conflicted CL, manually upload
    the conflicted change(resolved or not) and apply the skip configuration (See
    Skipping CLs below).

*   Merge conflicts may manifest as missing CLs in the downstream queue. If you
    feel a change is missing, file a bug or reach out to the current
    downstreamer.

### Skipping CLs

Suppose you've identified a problematic CL in the chain that you want to pull
out or manually tweak to get working. Add the Gerrit hashtag `copybot-skip` to
the CL in the Chromium Gerrit, and be sure to leave the upstream commit hash
somewhere in the commit message (if the CL was originally created by copybot, it
should already have this).

On the next run of copybot, it will skip over the CL and not attempt to upload
over your changes. Should you want copybot to go back to doing what it normally
does, just remove the hashtag.

### Out of sequence cherry-pick

Merge conflicts may occur due to out of sequence cherry-picking. This is due to
the method in which copybot finds the last merged CL in Chromium, and finds it
in the upstream repository. Once the CL is found, copybot will attempt to
cherry-pick each upstream CL into the chromium repo until it reaches the
upstream HEAD. Should this occur, find the first unmerged CL in the upstream
repo and manually cherry-pick it into chromium and be sure to leave the upstream
commit hash somewhere in the commit message. Once this change lands, copybot
should once again work normally.

### Hypothetical issue:

1.  copybot runs at 9 AM and copies commit chain A over
1.  copybot is manually triggered at noon and copies commit chain B over

1.  commit chain B lands in chromium before commit chain A

1.  Since commit chain A is earlier in the history than commit chain B, copybot
    will start at the end of commit chain A, and attempt to cherry-pick the
    entire chain B again causing empty conflicts

#### Log example:

```bash
2023-09-07 03:36:40,384 INFO: Last merged revision: 1b25422215279191da7f71840da7214fbcb22b9c << This is where copybot will start(Note that this hash is truncated)
2023-09-07 03:36:41,325 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager log 0cd2a50727093d06b53039e7be5f341800c31fbe --format=%H`
2023-09-07 03:36:42,390 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 0cd2a50727093d06b53039e7be5f341800c31fbe`
2023-09-07 03:36:42,403 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 7c04d0e6fdaedaf6ee336485df939963fa6c0c1a`
2023-09-07 03:36:42,417 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only bfd85218a72ef057a7c150358a0f6983639f86dc`
2023-09-07 03:36:42,431 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only feb683d1b9fbf33fc22c9263b4d7ad935f90aeaf`
2023-09-07 03:36:42,445 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 8ba64cd608e4e88e8ca34e04132cc0f39a1af5a2`
2023-09-07 03:36:42,460 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 0d3745b67c9e457ad1401f3f1c322c1161231df3`
2023-09-07 03:36:42,474 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 184329c77a7750d86cfe1d848000dae0e72a3889`
2023-09-07 03:36:42,488 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 7285c375fca1e16a9dececd375e5a3853a222042`
2023-09-07 03:36:42,501 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only c5c293cad118b4ef62a63e966701e04f2e0ebc04`
2023-09-07 03:36:42,515 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only f8beac6b7ada62e9ad8e5dc2eabd381071a8482e`
2023-09-07 03:36:42,528 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 6b69af8f774bb4131b61227cdb0bc30ff9025e01`
2023-09-07 03:36:42,542 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 730c3ba6d81aa26b6c3438a613fe964fc9e4ea3f`
2023-09-07 03:36:42,556 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 5a87c824286bb3928ebc416a039c1e8844d69e08`
2023-09-07 03:36:42,570 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 7dccc596f0768651f03701257d1cd1307dfe5257`
2023-09-07 03:36:42,585 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 966d652ed42bc2a639ae4e6cca63cab959367354`
2023-09-07 03:36:42,602 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only eba8952de13987ee070f914b2e84ffa97f414d7e`
2023-09-07 03:36:42,615 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 69f0289608c983ba187e4dc479fbdd27855cbab2`
2023-09-07 03:36:42,629 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 820a31263dd6e291bdf740be7a526f0c0e686d7c`
2023-09-07 03:36:42,643 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 525d8d86c399d751caaf6535084d775075499986`
2023-09-07 03:36:42,658 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only d426176e2469130a96717b3f2594daeab5d3fe34`
2023-09-07 03:36:42,672 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only e48f24d7f29b1211ac4667707aded013126faa61`
2023-09-07 03:36:42,685 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 926d55cddd3353993604e08a7eb5d7671fd87d2f`
2023-09-07 03:36:42,699 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 6dadf7f4823a93a20d8ac7b24f93beea78f69c09`
2023-09-07 03:36:42,714 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only e6a5e6cefbc78ef55cc0471b91aa2af2734c1139`
2023-09-07 03:36:42,728 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 1b96bff27ea98593f28e1bd60b3ee8e727841d2a` << Chromium ToT
2023-09-07 03:36:42,742 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 5c35d30ffc7382af46b62044a5cf5326b1e57708`
2023-09-07 03:36:42,755 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 7de2fa3c7fabd2ff02e261c66f09c5e2ff989c07`
2023-09-07 03:36:42,769 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only c0986a4b9fc10bb7f397eca62b8824977244e99a`
2023-09-07 03:36:42,783 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only bfcd046e694d159e0cda674a581e727e32cb59e9`
2023-09-07 03:36:42,797 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only a52d38b637d08334cd5cf4e11750f2cf9d3dd849`
2023-09-07 03:36:42,811 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only fd6908a748ad00aa6557e08309b9aad9abcdceb2`
2023-09-07 03:36:42,824 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only f03a6ef1131a4f758f8bfa8aabea345e36ceb5bd`
2023-09-07 03:36:42,837 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 21db65d95b74052588ba11275836a095d1498b09`
2023-09-07 03:36:42,851 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 5879b61302863c1b168c11c924e3e0ee575e7116`
2023-09-07 03:36:42,865 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only b35429457a8ebcaab52dde7a7fc0219a7472a126`
2023-09-07 03:36:42,879 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 208cbdb6af078851915a491b8415c914646eb0c8`
2023-09-07 03:36:42,893 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only eefdfb5c179cb42556d4f82b9f56eb08f1dd0700` << Bottom of CL chain for chromium ToT
2023-09-07 03:36:42,907 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 96f7bd13180f284ae0dd700f3bd73a6b61139846`
2023-09-07 03:36:42,920 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 2aeb6e405aea8740f87185158598f2af50390904`
2023-09-07 03:36:42,934 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 53adf21174242a977aed984a4e9ff063be8f4f83`
2023-09-07 03:36:42,948 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 898678d8a22cd5b371f721728859e23b495c8bcd`
2023-09-07 03:36:42,962 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only ecf2b42e73d955737f9af7adf2bfcf9bcf29f41f`
2023-09-07 03:36:42,976 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only bed01d794f0b94f2a3cf4c1a3a45f7536b05d288`
2023-09-07 03:36:42,989 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 5849f5bd323d496977f50d2ffa1dbdc5ba3ecaf3`
2023-09-07 03:36:43,005 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 458e2553f5a3385b776ba50b948b07e00c2fdd27`
2023-09-07 03:36:43,018 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only b757facac292b11f32a5ecdb8ce58eb60d1a555f`
2023-09-07 03:36:43,032 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 6cba9769897dd75086e3304550cb4e4b8167b8d3`
2023-09-07 03:36:43,047 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 0dc607f68d318d670f3edc084280b8e5c339847e`
2023-09-07 03:36:43,061 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 4d0b18480d7d0a85bef0d1adcf4837549118473e`
2023-09-07 03:36:43,074 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only d888f61f083cc68ff891b4b21191e1a8c5647307`
2023-09-07 03:36:43,088 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 6d3682ee9b19b9e6833f38046891132be665c93c`
2023-09-07 03:36:43,102 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 372c4151d4423788e597c1fdf90e003ce4226649`
2023-09-07 03:36:43,118 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only db48680ebcfbd3617fef954129b3234a7aebbc4e`
2023-09-07 03:36:43,131 INFO: Skip commit db48680ebcfbd3617fef954129b3234a7aebbc4e due to empty file list after filtering (before filtering was ['3rdparty/amd_blobs'])
2023-09-07 03:36:43,132 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 4c88d105d0f4182a9fdbd79d08aa432cbc75196a`
2023-09-07 03:36:43,146 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only b41d48a09c27ec24ea780d9415ed61369f31fb59`
2023-09-07 03:36:43,160 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only dd3d260e6a2290ac94091d0d774e67a664fb7715`
2023-09-07 03:36:43,174 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 4011ee0cb7f14ca834c04304e084f772c7bef875`
2023-09-07 03:36:43,187 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only b7cac4c3750c7d5448307e2dd05cef90ecec4a9d`
2023-09-07 03:36:43,201 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 97112481f508cf031dbb58a0976d2144b6e90873`
2023-09-07 03:36:43,215 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 69cef8e6942ab62abc02062522f62cc1bb337f55`
2023-09-07 03:36:43,229 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 6bc06983ed1d6c545056b584eabc2f7864cb39cd`
2023-09-07 03:36:43,243 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only d0de6c2183af5f6c435e53632fe92793be4d3783`
2023-09-07 03:36:43,257 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 5f5f7ca93c6809f916bcf28bf36a595957a3236b`
2023-09-07 03:36:43,271 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only f3ae1a120973da374ecbd2488b56b6a8fbbc82b5`
2023-09-07 03:36:43,286 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only e54c13e13cb1636f697187a844af9350974330a3`
2023-09-07 03:36:43,303 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 02e4d32524e99906100db2c3aa0c51e748916231`
2023-09-07 03:36:43,317 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only bc54f72d5da9c1c752aacf0c77c94a03771c3a90`
2023-09-07 03:36:43,331 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 465064f7d434b295d000d7c3cc5d2dcb07ddd0c9`
2023-09-07 03:36:43,345 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 7cdc4296f0fb88ae631ceecb80496447e7b2e53f`
2023-09-07 03:36:43,360 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only a637fa931056d734eb67ddbd9a1d8d2f9407302b`
2023-09-07 03:36:43,374 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager show --pretty= --name-only 6aca25c887ba9e7cce92e18ab77fc49c352ad12a`
2023-09-07 03:36:43,392 INFO: Run `git -C /b/s/w/ir/x/t/tmpc93n4kr9.copybot --no-pager checkout ad1b5f1fef86f929626575bda7b038c939af80cc` << Merged after the above listed chromium ToT
```

[Copybara]: https://g3doc.corp.google.com/devtools/copybara/g3doc/index.md?cl=head
[Copybot]: https://chromium.googlesource.com/chromiumos/platform/dev-util/+/HEAD/contrib/copybot
[copybot.star]: https://source.corp.google.com/h/chrome-internal/chromeos/codesearch/+/main:infra/config/misc_builders/copybot.star
[coreboot_downstream.py]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:chromite/contrib/copybot_downstream_config/coreboot_downstream.py
[crrev/i/5866621]: https://chrome-internal-review.googlesource.com/c/chromeos/overlays/chipset-phoenix-private/+/5866621
[crrev/i/5994551]: https://chrome-internal-review.googlesource.com/c/chromeos/overlays/overlay-myst-private/+/5994551
[downstreaming rotation]: https://rotations.corp.google.com/rotation/6734101192114176
[go/cros-luci]: https://luci-scheduler.appspot.com/jobs/chromeos
[new CLs from Copybot]: https://chromium-review.googlesource.com/q/hashtag:coreboot-downstream+status:open
[OWNERS.cros_ap]: https://source.corp.google.com/h/chrome-internal/chromeos/codesearch/+/main:owners/firmware/OWNERS.cros_ap
