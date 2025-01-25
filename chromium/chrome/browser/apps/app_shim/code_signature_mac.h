// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_CODE_SIGNATURE_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_CODE_SIGNATURE_MAC_H_

#include <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/types/expected.h"

namespace apps {

enum class SignatureValidationType {
  // Verify that the running application has a valid code signature and
  // that it is unchanged from the copy on disk.
  DynamicAndStatic,

  // Verify that the running application has a valid code signature.
  // Do not verify that the application matches the copy on disk.
  // The contents of the Info.plist of the process must be provided.
  DynamicOnly,
};

// Returns whether `process` has a valid code signature that fulfills
// `requirement`.
OSStatus ProcessIsSignedAndFulfillsRequirement(
    audit_token_t process,
    SecRequirementRef requirement,
    SignatureValidationType validation_type =
        SignatureValidationType::DynamicAndStatic,
    std::string_view info_plist_xml = {});

// Returns whether `process` has a valid code signature that fulfills
// `requirement`.
//
// NOTE: Prefer the version of this function that takes an `audit_token_t`.
// Process IDs do not uniquely identify a process so it is impossible to make
// trust decisions based on them.
OSStatus ProcessIsSignedAndFulfillsRequirement(
    pid_t process,
    SecRequirementRef requirement,
    SignatureValidationType validation_type =
        SignatureValidationType::DynamicAndStatic,
    std::string_view info_plist_xml = {});

// Reason why no requirement could be returned.
enum class MissingRequirementReason {
  NoOrAdHocSignature,
  Error,
};

// Returns the framework bundle's designated requirement as a string.
//
// If a value is returned, it will be non-null. If there was no designated
// requirement or the designated requirement could not be loaded, an error
// will be returned.
base::expected<base::apple::ScopedCFTypeRef<CFStringRef>,
               MissingRequirementReason>
FrameworkBundleDesignatedRequirementString();

// Create a SecRequirementRef from a requirement string.
//
// Returns a null reference if the requirement string was invalid.
base::apple::ScopedCFTypeRef<SecRequirementRef> RequirementFromString(
    CFStringRef requirement_string);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_CODE_SIGNATURE_MAC_H_
