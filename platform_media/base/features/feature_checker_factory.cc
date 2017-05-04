// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "base/features/feature_checker_factory.h"

namespace base {

FeatureCheckerFactory::FeatureCheckerFactory() = default;
FeatureCheckerFactory::~FeatureCheckerFactory() = default;

// static
FeatureChecker* FeatureCheckerFactory::Get() {
  FeatureCheckerFactory* fcf = GetInstance();
  return &fcf->feature_checker_;
}

// static
FeatureCheckerFactory* FeatureCheckerFactory::GetInstance() {
  return Singleton<FeatureCheckerFactory,
                   LeakySingletonTraits<FeatureCheckerFactory>>::get();
}

}  // namespace base
