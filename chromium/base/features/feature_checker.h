// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_FEATURE_CHECKER_H_
#define BASE_FEATURES_FEATURE_CHECKER_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/features/feature_status_override.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_piece.h"

namespace base {

class FeatureReader;
class ScopedTestFeatureOverride;

// Checks whether certain browser features are enabled in runtime.
class BASE_EXPORT FeatureChecker {
 public:
  FeatureChecker();
  ~FeatureChecker();

  // Registers a feature called |feature_name|. The name should be short, ex.
  // "hardware-acceleration" or "turbo". |enabled_by_default| controls whether
  // the feature should be reported enabled or disabled when not explicitly
  // set. |settable| controls whether the state of the feature can be set to
  // anything other than default. Non-settable features will retain their
  // default state regardless of whether they are overriden by a FeatureReader.
  //
  // It's safest to make all calls to RegisterFeature() and ReadStatus() before
  // secondary browser threads are launched. If the product cannot guarantee
  // this behaviour, it's the products responsibility to ensure thread safety.
  void RegisterFeature(base::StringPiece feature_name,
                       bool enabled_by_default,
                       bool settable);

  // Checks whether a feature |feature_name| is currently enabled.
  // Returns true if feature is enabled, false otherwise. If a feature of that
  // name wasn't registered, returns false.
  //
  // May be called from any thread after initialization has finished (ie.
  // all calls to RegisterFeature() and ReadStatus()).
  bool IsFeatureEnabled(base::StringPiece feature_name) const;

  // Reads the status of explicitly set features from |state_reader|. The status
  // is read once and remembered. Multiple calls can be made if more than one
  // FeatureReader is in use.
  void ReadStatus(const FeatureReader& state_reader);

  // Returns the FeatureStatusOverrides currently in action. This includes
  // those set by FeatureReaders (see ReadStatus()) and
  // ScopedTestFeatureOverrides that exist at the moment of calling.
  std::vector<FeatureStatusOverride> GetOverrides() const;

 private:
  friend class ScopedTestFeatureOverride;

  class RegisteredFeature {
   public:
    RegisteredFeature(base::StringPiece name,
                      bool enabled_by_default,
                      bool settable);
    ~RegisteredFeature();

    const std::string& name() const { return name_; }
    bool enabled_by_default() const { return enabled_by_default_; }
    bool settable() const { return settable_; }

   private:
    std::string name_;
    bool enabled_by_default_;
    bool settable_;
  };

  void PushTestFeatureOverride(
      const ScopedTestFeatureOverride* feature_override);
  void PopTestFeatureOverride(
      const ScopedTestFeatureOverride* feature_override);

  std::vector<RegisteredFeature> registered_features_;
  std::vector<FeatureStatusOverride> overrides_;
  std::vector<const ScopedTestFeatureOverride*> test_feature_overrides_;

  // Used to verify that ReadStatus() was called at least once before
  // IsFeatureEnabled is called for a settable feature.
  bool read_status_called_;

  DISALLOW_COPY_AND_ASSIGN(FeatureChecker);
};

}  // namespace base

#endif  // BASE_FEATURES_FEATURE_CHECKER_H_
