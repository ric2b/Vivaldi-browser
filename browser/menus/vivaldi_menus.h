// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_MENUS_VIVALDI_MENUS_H_
#define BROWSER_MENUS_VIVALDI_MENUS_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace vivaldi {
bool GetFixedAcceleratorForCommandId(int command_id, ui::Accelerator* accel);
}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_MENUS_H_
