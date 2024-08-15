---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: dir-metadata
title: Directory metadata
---

CrOS utilizes [DIR_METADATA] files to make it easy for others to report bugs and
to find relevant team contact information. Filling these out in project is
important for getting developers in touch with the right people quickly.

[TOC]

## Formatting

Since [DIR_METADATA] files are [text protobufs], follow that style:

* include the documentation comment block at the top
* 2 space indents
* omit colons when using `{` blocks
* cuddle the `{` to the key
* use colons with scalars

The `cros format` tool can handle some basic formatting checks.

Here's an example:

```
# Metadata information for this directory.
#
# For more information on DIR_METADATA files, see:
#   https://www.chromium.org/chromium-os/developer-library/reference/development/dir-metadata/
#
# For the schema of this file, see Metadata message:
#   https://source.chromium.org/chromium/infra/infra/+/HEAD:go/src/infra/tools/dirmd/proto/dir_metadata.proto

buganizer {
  component_id: 1027774  # ChromeOS > Infra > Build
}

buganizer_public {
  component_id: 1037860  # ChromeOS Public Tracker > Services > Infra > Build
}

team_email: "chromeos-build-discuss@google.com"
```

## Validation

Use `cros lint` to verify the correctness of the file.

## Best Practices

Here is a random list of thoughts.

*   Always include at least one bug tracker.
    *   Include the component name as a comment (as long as it doesn't use
        non-public codewords/names, but those are rare in CrOS).
*   Team e-mail is encouraged, but not required.
*   Every git project must have a DIR_METADATA in its top directory.
*   Having DIR_METADATA files in subdirs is encouraged where it makes sense.
    *   Packages (ebuilds) in overlays should include one.
*   Symlinking within a git project is OK.
    *   Symlinking across git projects doesn't work.

## Limitations

There are a few known issues with the format.

*   Cannot do per-file settings.
*   Cannot "include" other files leading to a lot of duplication.
    *   Symlinking within a single git project helps a little.


[DIR_METADATA]: https://chromium.googlesource.com/infra/infra/+/HEAD/go/src/infra/tools/dirmd/README.md
[text protobufs]: https://developers.google.com/protocol-buffers/docs/text-format-spec
