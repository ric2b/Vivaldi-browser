---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: setup-ts-environment
title: Setting up your TypeScript dev environment
---

[TOC]

## Overview

This doc will help you setup your development environment for working on
TypeScript projects in Chromium.

## Generating a local tsconfig.json file

When you open a WebUI TypeScript directory in VSCode for the first time, you'll
probably see red squiggly underlines everywhere, especially in import
paths.

To fix this, we'll need to **generate a local `tsconfig.json`.**

Read the full guide here: [Generating a local tsconfig.json file](/chromium-os/developer-library/guides/typescript/generating-local-tsconfig).

## How to set up ESLint with VSCode

Did you know that Chromium has a [base eslintrc.js file](https://source.chromium.org/chromium/chromium/src/+/main:.eslintrc.js)? This file defines the rules that ESLint will use to display helpful Chromium-specific formatting warnings and errors in VSCode. Here's how to setup ESLint with VSCode:

1.  Install the
    [ESLint VSCode extension](https://github.com/Microsoft/vscode-eslint).
2.  In VSCode, press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> to open the
    Command Palette, then find + execute the command "Preferences: Open Settings
    (JSON)".
3.  Add the following lines to `settings.json`:

    ```json
    "eslint.validate": [
        "typescript", "javascript"
    ],
    "eslint.options": {
        "resolvePluginsRelativeTo": "third_party/node/node_modules"
    }
    ```

4.  Next, press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> to open the
    Command Palette, then find + execute the command called "Preferences: Open
    Workspace Settings (JSON)".

5.  Add the following lines to the just-opened `settings.json`:

    ```json
    "eslint.nodePath": "third_party/node",
    ```

    If you have no other settings in this file, the entire file should look like
    the following:

    ```json
    {
      "eslint.nodePath": "third_party/node",
    }
    ```

6.  Press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> to open the Command
    Palette, then find + execute the command called "ESLint: Restart ESLint
    Server".

Congratulations! ESLint should now be showing linting errors in TypeScript and
JavaScript files.