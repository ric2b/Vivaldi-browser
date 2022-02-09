// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/menus/vivaldi_menus.h"

#include "chrome/app/chrome_command_ids.h"
#include "ui/base/accelerators/accelerator.h"

namespace vivaldi {

// Returns true if an accelerator has been asigned. The accelerator can be empty
// meaning we do not want an accelerator.
bool GetFixedAcceleratorForCommandId(int command_id, ui::Accelerator* accel) {
  // Accelerators match hardcoded shortcuts in chromium. We have to use these
  // as long as we have no way of defining those ourselves.
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_UNDO:
      *accel = ui::Accelerator(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_REDO:
      *accel = ui::Accelerator(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_CUT:
      *accel = ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_COPY:
      *accel = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_PASTE:
      *accel = ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      *accel = ui::Accelerator();
      return true;
    case IDC_CONTENT_CONTEXT_SELECTALL:
      *accel = ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_ROTATECCW:
      *accel = ui::Accelerator(ui::VKEY_OEM_4, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_ROTATECW:
      *accel = ui::Accelerator(ui::VKEY_OEM_6, ui::EF_CONTROL_DOWN);
      return true;
    // This shortcut is for Save As for media (video etc), but it (the shortcut)
    // will when activated trigger the regular page save action while the menu
    // entry will trigger the media save action. So I am disabling this shortcut
    // in the menu for now as it is wrong (same behaviour in chrome and viv).
    // https://www.w3schools.com/html/html5_video.asp has simple test.
    // case IDC_CONTENT_CONTEXT_SAVEAVAS:
    //  *accel = ui::Accelerator(ui::VKEY_S, ui::EF_CONTROL_DOWN);
    //  return true;
    default:
      return false;
  }
}

}  // namespace vivaldi
