// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef BASE_FEATURES_SUBMODULE_FEATURES_H_
#define BASE_FEATURES_SUBMODULE_FEATURES_H_

#include "base/base_export.h"

namespace base {

// This header defines constants for use in feature checking within chromium
// code. These features must be registered by each product/platform that
// wishes to use them (see FeatureChecker::RegisterFeature) or left unregistered
// if they are to be ignored by that product/platform. A short description of
// feature in a comment is nice to have.

// An example dummy feature. Does nothing, not checked anywhere.
BASE_EXPORT extern const char kSubmoduleFeatureExample[];

// Controls the default state of passwords saving preference.
BASE_EXPORT extern const char kCustomizationDisablePasswordSavingByDefault[];

// Enables 'audio/mpeg' and 'audio/aac' in MSE if USE_SYSTEM_PROPRIETARY_CODECS
// is set.
BASE_EXPORT extern const char kFeatureMseAudioMpegAac[];

// Enables an alternative native theme implementation.
BASE_EXPORT extern const char kSubmoduleNativeThemeAlt[];

// Allows to switch usage of Google API key.
BASE_EXPORT extern const char kSubmoduleUseGoogleApiKey[];

// Enable the memory allowance system to catch how much discardable shared
// memory is used in the renderer processes. See TVSDK-9903.
BASE_EXPORT extern const char kSubmoduleCountDiscardableSharedMemory[];

// Enable buffered ranges based on PTS in chunk demuxer.
BASE_EXPORT extern const char kSubmoduleBufferedRangesPTS[];

// Encrypt the cookie value with AES encryption when persisted to the disk and
// allows the integration to specify the key. See TVSDK-13605.
BASE_EXPORT extern const char kSubmoduleStorageEncryption[];

// Turns on the feature to reduce GLES memory usage instead of losing
// contexts on OOM.
BASE_EXPORT extern const char kSubmoduleGLESMemoryTuning[];

// This feature enables the EBU-TT-D TTML parser, and disables the old basic
// TTML parser. See TVSDK-11953.
BASE_EXPORT extern const char kSubmoduleEBUTTDTTMLParser[];
}  // namespace base

#endif  // BASE_FEATURES_SUBMODULE_FEATURES_H_
