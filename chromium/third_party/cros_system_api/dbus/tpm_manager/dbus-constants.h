// Copyright 2015 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_

namespace tpm_manager {

// D-Bus service constants.
inline constexpr char kTpmManagerInterface[] = "org.chromium.TpmManager";
inline constexpr char kTpmManagerServiceName[] = "org.chromium.TpmManager";
inline constexpr char kTpmManagerServicePath[] = "/org/chromium/TpmManager";

// 5 minutes timeout for all tpm_manager calls.
// This is a bit on the long side, but we want to be cautious.
inline constexpr int kTpmManagerServiceTimeoutInMs = 5 * 60 * 1000;

// Methods exported by tpm_manager.
inline constexpr char kGetTpmStatus[] = "GetTpmStatus";
inline constexpr char kGetTpmNonsensitiveStatus[] = "GetTpmNonsensitiveStatus";
inline constexpr char kGetVersionInfo[] = "GetVersionInfo";
inline constexpr char kGetSupportedFeatures[] = "GetSupportedFeatures";
inline constexpr char kGetDictionaryAttackInfo[] = "GetDictionaryAttackInfo";
inline constexpr char kGetRoVerificationStatus[] = "GetRoVerificationStatus";
inline constexpr char kResetDictionaryAttackLock[] =
    "ResetDictionaryAttackLock";
inline constexpr char kTakeOwnership[] = "TakeOwnership";
inline constexpr char kRemoveOwnerDependency[] = "RemoveOwnerDependency";
inline constexpr char kClearStoredOwnerPassword[] = "ClearStoredOwnerPassword";
inline constexpr char kClearTpm[] = "ClearTpm";

// Signal registered by tpm_manager ownership D-Bus interface.
inline constexpr char kOwnershipTakenSignal[] = "SignalOwnershipTaken";

// Default dependencies on TPM owner privilege. The TPM owner password will not
// be destroyed until all of these dependencies have been explicitly removed
// using the RemoveOwnerDependency method.
inline constexpr const char* kTpmOwnerDependency_Nvram =
    "TpmOwnerDependency_Nvram";
inline constexpr const char* kTpmOwnerDependency_Attestation =
    "TpmOwnerDependency_Attestation";
inline constexpr const char* kTpmOwnerDependency_Bootlockbox =
    "TpmOwnerDependency_Bootlockbox";

inline constexpr const char* kInitialTpmOwnerDependencies[] = {
    kTpmOwnerDependency_Nvram, kTpmOwnerDependency_Attestation,
    kTpmOwnerDependency_Bootlockbox};

}  // namespace tpm_manager

#endif  // SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_
