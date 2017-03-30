// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_FEATURES_H_
#define BASE_FEATURES_FEATURES_H_

#include "base/base_export.h"
#include "base/features/submodule_features.h"
#include "base/strings/string_piece.h"

namespace base {

class CommandLine;

// Shorthand for retrieving the FeatureChecker from FeatureCheckerFactory and
// calling IsFeatureEnabled.
// Note that features must be registered before checking, otherwise this will
// always return false. See FeatureChecker::RegisterFeature() and
// FeatureCheckerFactory::Get().
//
// May be called from any thread, as long as the features are registered and
// their status is read. Has no side effects.
bool BASE_EXPORT IsFeatureEnabled(base::StringPiece feature_name);

// Same as IsFeatureEnabled() but used to indicate that a feature check is for
// long-lived customizations rather than normal work-in-progress features.
bool BASE_EXPORT IsCustomizationEnabled(base::StringPiece feature_name);

// Appeds all set feature flags to command line to pass features to
// renderer and utility processes.
// This isn't very elegant, design-wise, since we're assuming these other
// processes will be checking feature state via CommandLineFeatureReaders,
// and that is a violation of abstractions, but it's currently the easiest way
// that doesn't involve a lot of patching.
void BASE_EXPORT
AppendExplicitlySetFeaturesToCommandLine(base::CommandLine* cmd_line);

}  // namespace base

#endif  // BASE_FEATURES_FEATURES_H_
