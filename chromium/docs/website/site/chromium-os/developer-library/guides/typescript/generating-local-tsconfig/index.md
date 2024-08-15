---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: generating-local-tsconfig
title: Generating a local tsconfig.json file
---

[TOC]

## Why you need a local `tsconfig.json`

> **Want to skip the theory?**
> [Click here](#how-to-generate-a-local-tsconfigjson-file) to jump directly to
> [How to generate a local `tsconfig.json` file](#how-to-generate-a-local-tsconfigjson-file).

When you open a WebUI TypeScript directory in VSCode for the first time, you'll
probably see red underlines—a.k.a. "squigglies"—everywhere, especially in import
paths.

This means that TypeScript's **language server** can't understand how to resolve
absolute import paths, such as those that start with `chrome://` (compared to
relative import paths, e.g. `../myfile.js`).

Note: A **language server** is a process that reads configuration files and
provides IDEs/editors with language features like auto-complete and go-to
definition. See
[Language Server Protocol - Wikipedia](https://en.wikipedia.org/wiki/Language_Server_Protocol)
and
[Official page for Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
for more information.

For example, if the path `import {loadTimeData} from
'chrome://resources/js/load_time_data.m.js';` can't be resolved, it means that
the TypeScript language server can't find the file at
`chrome://resources/js/load_time_data.m.js`. If it *could* access that file,
we'd have access to IntelliSense: the TypeScript language server would
understand what `loadTimeData` is, and it would be able to autocomplete methods
on that class (e.g. `loadTimeData.getString(...)`) and warn about incorrect
usage.

To fix this, we'll need to **generate a local `tsconfig.json`.** This
`tsconfig.json` will live in the current project's directory, and **is not
checked into git.** This is because the file contains directory information that
is unique to your filesystem.

For example, here is part of what a `tsconfig.json` might look like. These
`paths` tell the TypeScript compiler where the **generated file** can be found
in your personal filesystem (e.g.
`/usr/local/google/home/YOUR_USERNAME/`).

```json
"paths": {
  "chrome://resources/*": [
    "/usr/local/google/home/YOUR_USERNAME/chromium/src/out/Default/gen/ui/webui/resources/preprocessed/*"
  ],
  "//resources/*": [
    "/usr/local/google/home/YOUR_USERNAME/chromium/src/out/Default/gen/ui/webui/resources/preprocessed/*"
  ],
  "chrome://resources/polymer/v3_0/*": [
    "/usr/local/google/home/YOUR_USERNAME/chromium/src/third_party/polymer/v3_0/components-chromium/*"
  ],
  ...
```

## How to generate a local `tsconfig.json` file

There are **two reasons** you may want to generate a `tsconfig.json`, each with
a different set of instructions:

*   I want IntelliSense in an
    [**existing TypeScript project**](#adding-intellisense-to-an-existing-ts-project)
*   I'm
    [**currently migrating my project from JavaScript to TypeScript**](#migrating-a-project-from-js-to-ts)

### Adding IntelliSense to an existing TS project

Use these instructions **before you start working on the project**; you should
be running everything against the state of the latest git HEAD. (This way, there
shouldn't be any build issues).

1.  Build your project by running a `build_ts` command, e.g. `autoninja -C
    out/Default ash/webui/firmware_update_ui/resources:build_ts`
2.  When the build succeeds, proceed to the instructions at
    [**Using the `gen_tsconfig.py` script**](#using-the-gen-tsconfigpy-script).

### Migrating a project from JS to TS

When you're in the process of migrating a JavaScript file to a TypeScript file,
having access to IntelliSense is very helpful, because it can fix TypeScript
compiler errors. However, if the **project doesn't build** because of those
**very same compiler errors**, then you're stuck!

Here's how to fix this:

1.  Comment out the contents of the file(s) causing the compiler errors.
2.  Run your build command, e.g. `autoninja -C out/Default
    ash/webui/firmware_update_ui/resources:build_ts`
    *   If the build fails, fix the issues or comment out more code until the
        build succeeds.
3.  When the build succeeds, proceed to the instructions at
    [**Using the `gen_tsconfig.py` script**](#using-the-gen-tsconfigpy-script).

### Using the `gen_tsconfig.py` script

There is a checked-in script called
[`gen_tsconfig.py`](https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/personalization_app/tools/gen_tsconfig.py)
that uses the output of the build_ts command to generate a local `tsconfig.json`
file.

Here's how to run that script:

```shell
$ ash/webui/personalization_app/tools/gen_tsconfig.py \
--root_out_dir out/Default \
--gn_target PROJECT_PATH/resources:build_ts
```

Replace `PROJECT_PATH` with your project's directory, e.g. `ash/webui/firmware_update_ui/resources:build_ts`. In this example, the full command to run would be `ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/Default --gn_target ash/webui/firmware_update_ui/resources:build_ts`.

Note: The `gen_tsconfig.py` script happens to exist in the
`ash/webui/personalization_app/tools/` directory, but it should still work no
matter where your project exists.

After running this command, you should see a new `tsconfig.json` file appear in
your project's directory.

You'll probably also want to create a new `.gitignore`
file in your project's directory with the following contents
([example file](https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/firmware_update_ui/resources/.gitignore)):

```
# Generated from ash/webui/personalization_app/tools/gen_tsconfig.py
tsconfig.json
```

> **Important**: After generating the local `tsconfig.json`, you will probably need to
restart the TypeScript language server so that the changes can take effect. To
do so, in VSCode press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> to open the
Command Palette, then find + execute the command called "TypeScript: Restart TS
server".