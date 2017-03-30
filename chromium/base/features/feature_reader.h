// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_FEATURE_READER_H_
#define BASE_FEATURES_FEATURE_READER_H_

#include <vector>

#include "base/base_export.h"
#include "base/features/feature_status_override.h"

namespace base {

// Reads state of a feature from some medium to inform whether it's been
// explicitly enabled or disabled.
class BASE_EXPORT FeatureReader {
 public:
  virtual ~FeatureReader() {}

  // Fills |overrides| with states of features that were explicitly enabled or
  // disabled. |overrides| must not be null.
  virtual void ReadOverrides(
      std::vector<FeatureStatusOverride>* overrides) const = 0;
};

}  // namespace base

#endif  // BASE_FEATURES_FEATURE_READER_H_
