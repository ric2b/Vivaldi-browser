// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_FEATURE_CHECKER_FACTORY_H_
#define BASE_FEATURES_FEATURE_CHECKER_FACTORY_H_

#include "base/base_export.h"
#include "base/features/feature_checker.h"
#include "base/memory/singleton.h"

namespace base {

class BASE_EXPORT FeatureCheckerFactory {
 public:
  // Note, when the FeatureChecker is returned the first time, it's not
  // initialized, ie. no features are registered and no FeatureReaders are
  // added. It's the product's/platform's job to set those things early on
  // startup.
  static FeatureChecker* Get();
  static FeatureCheckerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<FeatureCheckerFactory>;
  FeatureCheckerFactory();
  virtual ~FeatureCheckerFactory();

  FeatureChecker feature_checker_;
};

}  // namespace base
#endif  // BASE_FEATURES_FEATURE_CHECKER_FACTORY_H_
