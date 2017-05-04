// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "base/features/features.h"

#include <vector>

#include "base/command_line.h"
#include "base/features/command_line_feature_reader.h"
#include "base/features/feature_checker_factory.h"

namespace base {

bool IsFeatureEnabled(base::StringPiece feature_name) {
  return FeatureCheckerFactory::Get()->IsFeatureEnabled(feature_name);
}

bool IsCustomizationEnabled(base::StringPiece feature_name) {
  return IsFeatureEnabled(feature_name);
}

void AppendExplicitlySetFeaturesToCommandLine(base::CommandLine* cmd_line) {
  const std::vector<FeatureStatusOverride>& features_set_by_user =
      FeatureCheckerFactory::Get()->GetOverrides();
  for (auto i = features_set_by_user.begin(); i != features_set_by_user.end();
       ++i) {
    cmd_line->AppendSwitchASCII(
        CommandLineFeatureReader::kFeaturePrefix + i->name(),
        i->enabled() ? "on" : "off");
  }
}

}  // namespace base
