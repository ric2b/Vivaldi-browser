// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_COMMAND_LINE_FEATURE_READER_H_
#define BASE_FEATURES_COMMAND_LINE_FEATURE_READER_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/features/feature_reader.h"
#include "base/strings/string_piece.h"

namespace base {

// Parses the command line for explicitly enabled or disabled features.
// To enable a feature, pass --with-feature:[feature name].
// To disable a feature, pass --with-feature:[feature name]=off.
class BASE_EXPORT CommandLineFeatureReader : public FeatureReader {
 public:
  static const char kFeaturePrefix[];

  explicit CommandLineFeatureReader(const base::CommandLine& command_line);
  ~CommandLineFeatureReader() override;

  void ReadOverrides(
      std::vector<FeatureStatusOverride>* overrides) const override;

  // Returns the name of a command line flag for a given |feature_name|.
  static std::string GetSwitchForFeature(base::StringPiece feature_name);

 private:
  const base::CommandLine& command_line_;
  DISALLOW_COPY_AND_ASSIGN(CommandLineFeatureReader);
};

}  // namespace base

namespace opera {
// TODO(mpawlowski): Remove (DNA-37006)
using base::CommandLineFeatureReader;
}

#endif  // BASE_FEATURES_COMMAND_LINE_FEATURE_READER_H_
