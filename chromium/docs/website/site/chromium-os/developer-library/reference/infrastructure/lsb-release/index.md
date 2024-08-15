---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: lsb-release
title: /etc/lsb-release File Format
---

The `/etc/lsb-release` file is used in CrOS to hold some system settings.
Unfortunately, nowhere is the format of this file actually documented.
[LSB] itself only documents the `lsb_release` program.

Since we rely upon this file currently, we'll document the format that
CrOS tooling can rely upon, and provide some language examples.

## Usage

The `/etc/lsb-release` has a number of fields that look useful.
However, before you try to use any of the fields in CrOS code, please consult
the [ChromiumOS Configuration](./os_config.md#LSB) document.

For example, you must not use `CHROMEOS_RELEASE_BOARD` from this file.
That config document explains in more detail.

## Format

Fundamentally, the file is a simple key-value store.

Files that don't follow these rules are malformed and not supported.
Their usage/behavior is undefined and must not be relied upon.

* The keys/values are delimited by `=`.

* Line-based comments start with `#`.
* Inline comments are *not* supported.
* Blank lines are trimmed (including whitespace-only lines).

* Whitespace around the key is trimmed.
* Keys should be restricted to `[A-Z0-9_]`.
* Duplicate keys are not supported.

* Leading & trailing whitespace around the value is trimmed.
* Whitespace in the value is permitted and retained.
* Quotes are not treated specially or removed and should be avoided.
* Values may not be line-wrapped or otherwise have newlines embedded.
* All other characters are permitted in the value.

## Example Files

Here is an example `/etc/lsb-release` file.

```
# Normal line.
SOME_KEY=value
# Key and value with leading/trailing whitespace.
  WS_KEY  =    value
# Value with whitespace in the middle.
WS_VALUE = v a l u e
# Value with quotes don't get removed.
DOUBLE_QUOTES = "double"
SINGLE_QUOTES = 'sin gle'
RANDOM_QUOTES = '"
```

This is the JSON if that file was parsed and exported as such.

```js
{
  "SOME_KEY": "value",
  "WS_KEY": "value",
  "WS_VALUE": "v a l u e",
  "DOUBLE_QUOTES": "\"double\"",
  "SINGLE_QUOTES": "'sin gle'",
  "RANDOM_QUOTES": "'\""
}
```

## Coding Samples

If you want to access fields in this file, please do not parse it yourself.

### C++

The `SysInfo` class is provided by libchrome in [base/sys_info.h].
See the methods that start with the `GetLsb` prefix.

[base/sys_info.h]: https://chromium.googlesource.com/chromium/src/+/HEAD/base/system/sys_info.h

```cpp
#include <base/sys_info.h>
...
  std::string value;
  if (!base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_TRACK", &value)) {
    LOG(ERROR) << "Could not load field from lsb-release";
    return false;
  }

  // Do something with |value| now.
...
```

### Python

You can use `ConfigParser.RawConfigParser` to load `/etc/lsb-release`, but it
takes a bit of effort due to missing section header.
It also handles quoting and newlines differently.

The snippet below should handle all the edge cases.

```py
def Load(path='/etc/lsb-release'):
    lines = [x.strip() for x in open(path).readlines()]
    return dict(map(lambda x: x.strip(), line.partition('=')[::2])
                for line in lines if line and not line.startswith('#'))
```

### Shell

If you're writing shell code, you shouldn't be accessing this file.
[Please rewrite your code in a better language][rewrite-shell].

You absolutely must not source the file directly from shell code.
e.g. Running `. /etc/lsb-release` is not guaranteed to ever work.

```sh
lsbval() {
  local key="$1"
  local lsbfile="${2:-/etc/lsb-release}"

  if ! echo "${key}" | grep -Eq '^[a-zA-Z0-9_]+$'; then
    return 1
  fi

  sed -E -n -e \
    "/^[[:space:]]*${key}[[:space:]]*=/{
      s:^[^=]+=[[:space:]]*::
      s:[[:space:]]+$::
      p
    }" "${lsbfile}"
}
```

[LSB]: http://refspecs.linuxbase.org/lsb.shtml
[rewrite-shell]: ./development_basics.md#shell
