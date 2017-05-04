// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_manager.h"

namespace arc {

namespace {

const base::Feature kArcEnabledFeature{"EnableARC",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

ArcBridgeService::ArcBridgeService() = default;

ArcBridgeService::~ArcBridgeService() = default;

// static
bool ArcBridgeService::GetEnabled(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kEnableArc) ||
         (command_line->HasSwitch(chromeos::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kArcEnabledFeature));
}

bool ArcBridgeService::GetKioskEnabled(const base::CommandLine* command_line) {
  return GetEnabled(command_line) ||
         command_line->HasSwitch(chromeos::switches::kArcAvailable);
}

bool ArcBridgeService::GetKioskStarted(const base::CommandLine* command_line) {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp() &&
         GetKioskEnabled(command_line);
}

// static
bool ArcBridgeService::GetAvailable(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kArcAvailable);
}

}  // namespace arc
