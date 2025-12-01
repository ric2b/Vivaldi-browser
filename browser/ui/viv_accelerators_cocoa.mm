// Copyright 2025 Vivaldi Technologies. All rights reserved.

#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"

void AcceleratorsCocoa::OverrideAcceleratorForCommand(
    int command_id,
    ui::Accelerator accelerator) {
  if (!accelerator.IsEmpty()) {
    accelerators_.insert_or_assign(command_id, accelerator);
  } else {
    accelerators_.erase(command_id);
  }
}
