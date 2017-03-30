// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode.h"

#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LaserPointerMode::LaserPointerMode(Delegate* delegate)
    : CommonPaletteTool(delegate) {
}

LaserPointerMode::~LaserPointerMode() {}

PaletteGroup LaserPointerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId LaserPointerMode::GetToolId() const {
  return PaletteToolId::LASER_POINTER;
}

void LaserPointerMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  WmShell::Get()->SetLaserPointerEnabled(true);
}

void LaserPointerMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  WmShell::Get()->SetLaserPointerEnabled(false);
}

gfx::VectorIconId LaserPointerMode::GetActiveTrayIcon() {
  return gfx::VectorIconId::PALETTE_TRAY_ICON_LASER_POINTER;
}

gfx::VectorIconId LaserPointerMode::GetPaletteIconId() {
  return gfx::VectorIconId::PALETTE_MODE_LASER_POINTER;
}

views::View* LaserPointerMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_LASER_POINTER_MODE));
}
}  // namespace ash
