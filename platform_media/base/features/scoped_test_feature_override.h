// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_SCOPED_TEST_FEATURE_OVERRIDE_H_
#define BASE_FEATURES_SCOPED_TEST_FEATURE_OVERRIDE_H_

#include <string>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace base {

class FeatureChecker;

// Helper class allowing overriding a feature flag in a limited scope.
//
// The override will be valid only while the override object is alive.
// Nested overrides are allowed - the highest one up the stack is the
// effective override.
//
// To be used ONLY in tests.
class BASE_EXPORT ScopedTestFeatureOverride {
 public:
  ScopedTestFeatureOverride(base::StringPiece name, bool enabled);
  ScopedTestFeatureOverride(base::StringPiece name,
                            bool enabled,
                            FeatureChecker* feature_checker);
  ~ScopedTestFeatureOverride();

  const std::string& name() const { return name_; }
  bool enabled() const { return enabled_; }

 private:
  std::string name_;
  bool enabled_;
  FeatureChecker* feature_checker_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestFeatureOverride);
};

}  // namespace base

namespace opera {
// TODO(mpawlowski): Remove (DNA-37006)
using base::ScopedTestFeatureOverride;
}

#endif  // BASE_FEATURES_SCOPED_TEST_FEATURE_OVERRIDE_H_
