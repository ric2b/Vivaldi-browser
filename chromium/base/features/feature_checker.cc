// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "base/features/feature_checker.h"

#include <algorithm>

#include "base/features/feature_reader.h"
#include "base/features/scoped_test_feature_override.h"

namespace {
// Unary predicate for finding features in collections by name().
struct MatchByName {
  explicit MatchByName(base::StringPiece name) : searched_name(name) {}

  template <typename T>
  bool operator()(const T& feature) const {
    return feature.name() == searched_name;
  }

  template <typename T>
  bool operator()(const T* feature) const {
    return feature->name() == searched_name;
  }

  base::StringPiece searched_name;
};

}  // namespace

namespace base {

FeatureChecker::RegisteredFeature::RegisteredFeature(base::StringPiece name,
                                                     bool enabled_by_default,
                                                     bool settable)
    : name_(name.as_string()),
      enabled_by_default_(enabled_by_default),
      settable_(settable) {
}

FeatureChecker::RegisteredFeature::~RegisteredFeature() = default;

FeatureChecker::FeatureChecker() : read_status_called_(false) {
}

FeatureChecker::~FeatureChecker() = default;

void FeatureChecker::RegisterFeature(base::StringPiece feature_name,
                                     bool enabled_by_default,
                                     bool settable) {
  // Make sure no feature of this name is registered already. This is O(N) so
  // it makes sense to avoid it in Release builds.
  DCHECK(std::find_if(registered_features_.begin(), registered_features_.end(),
                      MatchByName(feature_name)) == registered_features_.end());
  registered_features_.push_back(
      RegisteredFeature(feature_name, enabled_by_default, settable));
}

bool FeatureChecker::IsFeatureEnabled(base::StringPiece feature_name) const {
  // Check test overrides first. Search from the back, as overrides can be
  // nested and the last one with matching name defines the state to use.
  const auto test_override_it =
      std::find_if(test_feature_overrides_.rbegin(),
                   test_feature_overrides_.rend(), MatchByName(feature_name));
  if (test_override_it != test_feature_overrides_.rend())
    return (*test_override_it)->enabled();

  const auto registered_feature_it =
      std::find_if(registered_features_.begin(), registered_features_.end(),
                   MatchByName(feature_name));
  if (registered_feature_it == registered_features_.end())
    // This feature was not registered.
    return false;
  const RegisteredFeature& registered_feature = *registered_feature_it;
  if (!registered_feature.settable())
    // Feature's state is not allowed to change from the default.
    return registered_feature.enabled_by_default();
  // Feature is registered and settable, its state may be explicitly set by the
  // user, so check the overrides.
  DCHECK(read_status_called_)
      << "Feature \"" << feature_name
      << "\" is settable but ReadStatus() wasn't called yet.";
  const auto override_it = std::find_if(overrides_.rbegin(), overrides_.rend(),
                                        MatchByName(feature_name));
  if (override_it != overrides_.rend()) {
    // An override has been set for this feature.
    const FeatureStatusOverride& status_override = *override_it;
    return status_override.enabled();
  }
  // The state of this feature was not altered by any override, return default.
  return registered_feature.enabled_by_default();
}

void FeatureChecker::ReadStatus(const FeatureReader& state_reader) {
  read_status_called_ = true;
  state_reader.ReadOverrides(&overrides_);
}

std::vector<FeatureStatusOverride> FeatureChecker::GetOverrides() const {
  // Translate ScopedTestFeatureOverrides into FeatureStatusOverrides:
  std::vector<FeatureStatusOverride> test_overrides(
      test_feature_overrides_.size());
  std::transform(test_feature_overrides_.begin(), test_feature_overrides_.end(),
                 test_overrides.begin(),
                 [](const ScopedTestFeatureOverride* test_override) {
                   return FeatureStatusOverride(test_override->name(),
                                                test_override->enabled());
                 });
  // Append those test overrides to the ones read in ReadStatus().
  std::vector<FeatureStatusOverride> overrides = overrides_;
  overrides.insert(overrides.end(), test_overrides.begin(),
                   test_overrides.end());
  return overrides;
}

void FeatureChecker::PushTestFeatureOverride(
    const ScopedTestFeatureOverride* feature_override) {
  // One override cannot be pushed twice.
  CHECK(std::find(test_feature_overrides_.begin(),
                  test_feature_overrides_.end(),
                  feature_override) == test_feature_overrides_.end());
  test_feature_overrides_.push_back(feature_override);
}

void FeatureChecker::PopTestFeatureOverride(
    const ScopedTestFeatureOverride* feature_override) {
  // The popped override should be the last one on our list.
  CHECK_EQ(feature_override, *test_feature_overrides_.rbegin());
  test_feature_overrides_.pop_back();
}

}  // namespace base
