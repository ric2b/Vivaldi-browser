// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_FEATURE_STATUS_OVERRIDE_H_
#define BASE_FEATURES_FEATURE_STATUS_OVERRIDE_H_

#include <string>

#include "base/base_export.h"

namespace base {

// Represents an overriden (explicitly set) state of a feature.
class BASE_EXPORT FeatureStatusOverride {
 public:
  FeatureStatusOverride();
  FeatureStatusOverride(const std::string& name, bool enabled);
  ~FeatureStatusOverride();

  const std::string& name() const;
  bool enabled() const;
  bool operator==(const FeatureStatusOverride& other) const;

 private:
  std::string name_;
  bool enabled_;
};

}  // namespace base

#endif  // BASE_FEATURES_FEATURE_STATUS_OVERRIDE_H_
