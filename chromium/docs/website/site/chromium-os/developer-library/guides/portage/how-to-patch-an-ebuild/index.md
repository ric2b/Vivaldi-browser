---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: how-to-patch-an-ebuild
title: How to Patch an Ebuild
---

## Background

ChromeOS uses a package management system from Gentoo Linux called [Portage](https://wiki.gentoo.org/wiki/Portage).

Sometimes, an upstream package does not quite serve our specific needs. When that is the case, you can [patch](https://wiki.gentoo.org/wiki/Patches) the package in order to bend it to your will.

## Variables used in this document

| Testing        | Blah                                                                      |
| -------------- | ------------------------------------------------------------------------- |
| `$REPO_BASE`   | The path to your chromiumos checkout.                                     |
| `$BOARD`       | The board for which you are building, such as grunt or octopus.           |
| `$CATEGORY`    | The category of your Gentoo package, such as chromeos-base or dev-python. |
| `$PKG`         | The name of your Gentoo package, such as autotest or protobuf-python.     |
| `$REV`         | The revision number of your Gentoo package.                               |
| `$EBUILD_DIR`  | `$REPO_BASE/src/third_party/chromiumos-overlay/$CATEGORY/$PKG/`           |
| `$EBUILD_FILE` | `$EBUILD_DIR/$PKG(-$REV).ebuild`                                          |
| `$PATCH_NAME`  | The basename of your patch, such as `foobar.patch`                        |


## Process

### 1. Unpack a local copy of the ebuild

In order to modify the ebuild, you need to unpack a local copy. This is easy.

Create a local copy in `/build/$BOARD/tmp/portage/$CATEGORY/$PKG(-$REV)/work/$PKG(-$REV)`:

```
$ ebuild-$BOARD $(equery which $CATEGORY/$PKG) unpack
```

Move that local copy somewhere that it won’t get clobbered:

```
$ cp /build/$BOARD/tmp/portage/$CATEGORY/$PKG(-$REV)/work/$PKG(-$REV) /tmp/my_cool_pkg/ -R
```

Initialize a git repo in your local copy, so that you can diff it later:

```
$ cd /tmp/my_cool_pkg/
$ git init
$ git add *
$ git commit -m “Initial commit”
```

### 2. Make some changes and create a patch

You can modify the files in `/tmp/my_cool_pkg` as you please.

When you’re ready to give your change a try, it’s time to create a patch file. Conveniently, patch files look exactly like git diffs. (That is why we ran `git init` above).

```
$ git diff | tee /tmp/$PATCH_NAME
```

### 3. Include your patch in the ebuild
Create the directory `$EBUILD_FILE/files/`. Your ebuild knows that directory as `${FILESDIR}`. Your patch file should go in there.

```
$ mkdir $EBUILD_DIR/files
$ cp /tmp/$PATCH_NAME $EBUILD_DIR/files/$PATCH_NAME
```

If the ebuild is EAPI 6 or higher and calls a default `src_prepare()` function, then `src_prepare()` will automatically apply every patch file declared in an array named `PATCHES`. If that describes your situation, then declare an array called `PATCHES` before any functions are declared:

```
PATCHES = (
    “${FILESDIR}”/$PATCH_NAME
)
```

Otherwise&mdash;which is to say, if your ebuild is EAPI 5 or lower, or it does not call a default `src_prepare()` function&mdash;then you will need to manually apply your patch:

1. Inherit `eutils`. If the package already inherits some eclasses, add `eutils` to that list (example: `inherit cros-workon linux-info eutils`). If the package does *not* yet inherit any eclasses, then add the following line at the beginning of the ebuild, after initial comments:

```
inherit eutils
```

2. At the beginning of the `src_prepare()` step, add the following line:

```
epatch "${FILESDIR}"/$PATCH_NAME
```

### 4. Test your change

```
$ emerge-$BOARD $CATEGORY/$PKG
```

If your patch worked, goto step 5. Else, goto step 2.

### 5. Comment your patch, uprev it, and commit it

You can add comments at the start of your patch by prefixing them with octothorpes (`#`). Do that.

You will need to uprev the package. If the ebuild already has a basename like `$PKG-r1.ebuild`, just increment the number after the `r`. If there is no `-r#`, then create a symlink to the original ebuild:

```
ln -s $PKG.ebuild $PKG-r1.ebuild
```

Then rejoice, for your patch is ready for the CQ.
