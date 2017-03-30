// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "base/features/scoped_test_feature_override.h"

#include "base/features/feature_checker.h"
#include "base/features/feature_checker_factory.h"

namespace base {

ScopedTestFeatureOverride::ScopedTestFeatureOverride(base::StringPiece name,
                                                     bool enabled)
    : name_(name.as_string()),
      enabled_(enabled),
      feature_checker_(FeatureCheckerFactory::Get()) {
  feature_checker_->PushTestFeatureOverride(this);
}

ScopedTestFeatureOverride::ScopedTestFeatureOverride(
    base::StringPiece name,
    bool enabled,
    FeatureChecker* feature_checker)
    : name_(name.as_string()),
      enabled_(enabled),
      feature_checker_(feature_checker) {
  feature_checker_->PushTestFeatureOverride(this);
}

ScopedTestFeatureOverride::~ScopedTestFeatureOverride() {
  feature_checker_->PopTestFeatureOverride(this);
}

}  // namespace base
