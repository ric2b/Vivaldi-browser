---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: use-mojom-union
title: Use Mojom Union in TypeScript and C++
---

[TOC]

**TLDR:** Use Mojom Union in TypeScript and C++ - add union to mojom file - in
frontend, get union value with if statement - in backend, create mojom pointer
object with union type function - check and get union value with union name
functions

## Problem

I need to create a mojom union and use it at both frontend and backend. For
example, I add a `RemappingAction` union in mojom file and I want to use it at
both frontend and backend.

## Solution

### Mojom change

1.  Add the union to `ash/public/mojom/input_device_settings.mojom` file.
`RemappingAction` will only be `AcceleratorAction` or `KeyEvent` or `StaticShortcutAction`.

    ```
    union RemappingAction {
      ash.mojom.AcceleratorAction accelerator_action;
      KeyEvent key_event;
      StaticShortcutAction static_shortcut_action;
    };
    ```

### Frontend change

Key steps:

1.  When getting the union value, we need to include the `remappingAction`
    field.

    ```
    const acceleratorAction =
        this.buttonRemapping_.remappingAction?.acceleratorAction;
    const keyEvent = this.buttonRemapping_.remappingAction?.keyEvent;
    const staticShortcutAction =
        this.buttonRemapping_.remappingAction?.staticShortcutAction;
    ```

    There should be only one value exists, so use if statement to unpack the
    value.

    ```
    if (acceleratorAction !== undefined) {
      ...
    } else if (keyEvent !== undefined) {
      ...
    } else if (staticShortcutAction != undefined) {
      ...
    }
    ```

    The `remappingAction` object will be one of the following:

    ```
    remappingAction: {
      acceleratorAction: value,
    },
    remappingAction: {
      keyEvent: value,
    },
    remappingAction: {
      staticShortcutAction: value,
    },
    ```

### Backend change

Key steps:

1.  When creating the `remapping_action` mojom pointer object, we can directly
    call the function with union type to create the mojom pointer object.

    ```
    mojom::RemappingAction::NewAcceleratorAction(
        ash::AcceleratorAction::kBrightnessDown);
    mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(::ui::KeyboardCode::VKEY_2, 4, 5, 6));
    mojom::RemappingAction::NewStaticShortcutAction(
        mojom::StaticShortcutAction::kCopy);
    ```

    There should be only one type of `remapping_action`, we can check and get it
    with the union name functions.

    ```
    if (remapping_action->is_accelerator_action()) {
      remapping_action->get_accelerator_action();
    } else if (remapping_action->is_key_event()) {
      remapping_action->get_key_event();
    } else if (remapping_action->is_static_shortcut_action()) {
      remapping_action->get_static_shortcut_action();
    }
    ```

## Example CL

*   https://crrev.com/c/4869246
*   https://crrev.com/c/4679744
