// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "base/features/command_line_feature_reader.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace base {

const char CommandLineFeatureReader::kFeaturePrefix[] = "with-feature:";

CommandLineFeatureReader::CommandLineFeatureReader(
    const base::CommandLine& command_line)
    : command_line_(command_line) {
}

CommandLineFeatureReader::~CommandLineFeatureReader() {
}

void CommandLineFeatureReader::ReadOverrides(
    std::vector<FeatureStatusOverride>* overrides) const {
  for (auto cl_switch : command_line_.GetSwitches()) {
    if (StartsWith(cl_switch.first, kFeaturePrefix, base::CompareCase::SENSITIVE)) {
      std::string feature_name = cl_switch.first;
      ReplaceFirstSubstringAfterOffset(&feature_name, 0, kFeaturePrefix, "");
      const std::string switch_value =
          command_line_.GetSwitchValueASCII(cl_switch.first);
      if (!switch_value.empty() && switch_value != "off" &&
          switch_value != "on") {
        LOG(WARNING)
            << cl_switch.first << " has unhandled value: \"" << switch_value
            << "\" did you mean \"on\" or \"off\"? Using default setting.";
        continue;
      }
      overrides->push_back(
          FeatureStatusOverride(feature_name, switch_value != "off"));
    }
  }
}

// static
std::string CommandLineFeatureReader::GetSwitchForFeature(
    base::StringPiece feature_name) {
  std::string switch_name(kFeaturePrefix);
  feature_name.AppendToString(&switch_name);
  return switch_name;
}

}  // namespace base
