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
    case IDC_BACK:
      *accel = ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
      return true;
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
    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      *accel =
          ui::Accelerator(ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
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
    case IDC_FORWARD:
      *accel = ui::Accelerator(ui::VKEY_RIGHT, ui::EF_ALT_DOWN);
      return true;
    case IDC_PRINT:
      *accel = ui::Accelerator(ui::VKEY_P, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_RELOAD:
      *accel = ui::Accelerator(ui::VKEY_R, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_SAVE_PAGE:
      *accel = ui::Accelerator(ui::VKEY_S, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_VIEW_SOURCE:
      *accel = ui::Accelerator(ui::VKEY_U, ui::EF_CONTROL_DOWN);
      return true;
    default:
      return false;
  }
}

}  // namespace vivaldi
