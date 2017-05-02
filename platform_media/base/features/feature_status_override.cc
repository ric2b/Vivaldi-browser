// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "base/features/feature_status_override.h"

namespace base {

FeatureStatusOverride::FeatureStatusOverride() = default;

FeatureStatusOverride::FeatureStatusOverride(const std::string& name,
                                             bool enabled)
    : name_(name), enabled_(enabled) {
}

FeatureStatusOverride::~FeatureStatusOverride() = default;

const std::string& FeatureStatusOverride::name() const {
  return name_;
}

bool FeatureStatusOverride::enabled() const {
  return enabled_;
}

bool FeatureStatusOverride::operator==(
    const FeatureStatusOverride& other) const {
  return name() == other.name() && enabled() == other.enabled();
}

}  // namespace base
