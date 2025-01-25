// Copyright 2015 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_

namespace user_data_auth {

// Interface exposed by the cryptohome daemon.
inline constexpr char kUserDataAuthServiceName[] = "org.chromium.UserDataAuth";
inline constexpr char kUserDataAuthServicePath[] = "/org/chromium/UserDataAuth";

inline constexpr char kUserDataAuthInterface[] =
    "org.chromium.UserDataAuthInterface";
inline constexpr char kCryptohomePkcs11Interface[] =
    "org.chromium.CryptohomePkcs11Interface";
inline constexpr char kInstallAttributesInterface[] =
    "org.chromium.InstallAttributesInterface";
inline constexpr char kCryptohomeMiscInterface[] =
    "org.chromium.CryptohomeMiscInterface";

// 5 minutes timeout for all cryptohome calls.
// This is a bit on the long side, but we want to be cautious.
inline constexpr int kUserDataAuthServiceTimeoutInMs = 5 * 60 * 1000;

// Methods of the |kUserDataAuthInterface| interface:
inline constexpr char kIsMounted[] = "IsMounted";
inline constexpr char kGetVaultProperties[] = "GetVaultProperties";
inline constexpr char kUnmount[] = "Unmount";
inline constexpr char kRemove[] = "Remove";
inline constexpr char kGetWebAuthnSecret[] = "GetWebAuthnSecret";
inline constexpr char kGetRecoverableKeyStores[] = "GetRecoverableKeyStores";
inline constexpr char kStartMigrateToDircrypto[] = "StartMigrateToDircrypto";
inline constexpr char kNeedsDircryptoMigration[] = "NeedsDircryptoMigration";
inline constexpr char kGetSupportedKeyPolicies[] = "GetSupportedKeyPolicies";
inline constexpr char kGetAccountDiskUsage[] = "GetAccountDiskUsage";
inline constexpr char kStartAuthSession[] = "StartAuthSession";
inline constexpr char kInvalidateAuthSession[] = "InvalidateAuthSession";
inline constexpr char kExtendAuthSession[] = "ExtendAuthSession";
inline constexpr char kCreatePersistentUser[] = "CreatePersistentUser";
inline constexpr char kPrepareGuestVault[] = "PrepareGuestVault";
inline constexpr char kPrepareEphemeralVault[] = "PrepareEphemeralVault";
inline constexpr char kPreparePersistentVault[] = "PreparePersistentVault";
inline constexpr char kPrepareVaultForMigration[] = "PrepareVaultForMigration";
inline constexpr char kPrepareAuthFactor[] = "PrepareAuthFactor";
inline constexpr char kTerminateAuthFactor[] = "TerminateAuthFactor";
inline constexpr char kAddAuthFactor[] = "AddAuthFactor";
inline constexpr char kAuthenticateAuthFactor[] = "AuthenticateAuthFactor";
inline constexpr char kUpdateAuthFactor[] = "UpdateAuthFactor";
inline constexpr char kUpdateAuthFactorMetadata[] = "UpdateAuthFactorMetadata";
inline constexpr char kRelabelAuthFactor[] = "RelabelAuthFactor";
inline constexpr char kReplaceAuthFactor[] = "ReplaceAuthFactor";
inline constexpr char kRemoveAuthFactor[] = "RemoveAuthFactor";
inline constexpr char kListAuthFactors[] = "ListAuthFactors";
inline constexpr char kGetAuthFactorExtendedInfo[] =
    "GetAuthFactorExtendedInfo";
inline constexpr char kGetAuthSessionStatus[] = "GetAuthSessionStatus";
inline constexpr char kLockFactorUntilReboot[] = "LockFactorUntilReboot";
inline constexpr char kModifyAuthFactorIntents[] = "ModifyAuthFactorIntents";
inline constexpr char kCreateVaultkeyset[] = "CreateVaultKeyset";
inline constexpr char kGetArcDiskFeatures[] = "GetArcDiskFeatures";
inline constexpr char kMigrateLegacyFingerprints[] =
    "MigrateLegacyFingerprints";
inline constexpr char kSetUserDataStorageWriteEnabled[] =
    "SetUserDataStorageWriteEnabled";

// Methods of the |kCryptohomePkcs11Interface| interface:
inline constexpr char kPkcs11IsTpmTokenReady[] = "Pkcs11IsTpmTokenReady";
inline constexpr char kPkcs11GetTpmTokenInfo[] = "Pkcs11GetTpmTokenInfo";
inline constexpr char kPkcs11Terminate[] = "Pkcs11Terminate";
inline constexpr char kPkcs11RestoreTpmTokens[] = "Pkcs11RestoreTpmTokens";

// Methods of the |kInstallAttributesInterface| interface:
inline constexpr char kInstallAttributesGet[] = "InstallAttributesGet";
inline constexpr char kInstallAttributesSet[] = "InstallAttributesSet";
inline constexpr char kInstallAttributesFinalize[] =
    "InstallAttributesFinalize";
inline constexpr char kInstallAttributesGetStatus[] =
    "InstallAttributesGetStatus";
inline constexpr char kGetFirmwareManagementParameters[] =
    "GetFirmwareManagementParameters";
inline constexpr char kRemoveFirmwareManagementParameters[] =
    "RemoveFirmwareManagementParameters";
inline constexpr char kSetFirmwareManagementParameters[] =
    "SetFirmwareManagementParameters";

// Methods of the |kCryptohomeMiscInterface| interface:
inline constexpr char kGetSystemSalt[] = "GetSystemSalt";
inline constexpr char kUpdateCurrentUserActivityTimestamp[] =
    "UpdateCurrentUserActivityTimestamp";
inline constexpr char kGetSanitizedUsername[] = "GetSanitizedUsername";
inline constexpr char kGetLoginStatus[] = "GetLoginStatus";
inline constexpr char kLockToSingleUserMountUntilReboot[] =
    "LockToSingleUserMountUntilReboot";
inline constexpr char kGetRsuDeviceId[] = "GetRsuDeviceId";
inline constexpr char kGetPinWeaverInfo[] = "GetPinWeaverInfo";

// Signals of the |kUserDataAuthInterface| interface:
inline constexpr char kDircryptoMigrationProgress[] =
    "DircryptoMigrationProgress";
inline constexpr char kAuthFactorStatusUpdate[] = "AuthFactorStatusUpdate";
inline constexpr char kLowDiskSpace[] = "LowDiskSpace";
inline constexpr char kAuthEnrollmentProgressSignal[] =
    "AuthEnrollmentProgress";
inline constexpr char kPrepareAuthFactorProgressSignal[] =
    "PrepareAuthFactorProgress";
inline constexpr char kAuthenticateStartedSignal[] = "AuthenticateStarted";
inline constexpr char kAuthenticateAuthFactorCompletedSignal[] =
    "AuthenticateAuthFactorCompleted";
inline constexpr char kMountStartedSignal[] = "MountStarted";
inline constexpr char kMountCompletedSignal[] = "MountCompleted";
inline constexpr char kEvictedKeyRestoredSignal[] = "EvictedKeyRestored";
inline constexpr char kAuthFactorAddedl[] = "AuthFactorAdded";
inline constexpr char kAuthFactorRemoved[] = "AuthFactorRemoved";
inline constexpr char kAuthFactorUpdted[] = "AuthFactorUpdated";
inline constexpr char kAuthSessionExpiring[] = "AuthSessionExpiring";
inline constexpr char kRemoveCompleted[] = "RemoveCompleted";

}  // namespace user_data_auth

namespace cryptohome {

// Error code
enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_FATAL = 1,
  MOUNT_ERROR_KEY_FAILURE = 2,
  MOUNT_ERROR_INVALID_ARGS = 3,
  MOUNT_ERROR_MOUNT_POINT_BUSY = 4,
  MOUNT_ERROR_EPHEMERAL_MOUNT_BY_OWNER = 5,
  MOUNT_ERROR_CREATE_CRYPTOHOME_FAILED = 6,
  MOUNT_ERROR_REMOVE_INVALID_USER_FAILED = 7,
  MOUNT_ERROR_TPM_COMM_ERROR = 8,
  MOUNT_ERROR_UNPRIVILEGED_KEY = 9,
  MOUNT_ERROR_SETUP_PROCESS_KEYRING_FAILED = 10,
  MOUNT_ERROR_UNEXPECTED_MOUNT_TYPE = 11,
  MOUNT_ERROR_KEYRING_FAILED = 12,
  MOUNT_ERROR_DIR_CREATION_FAILED = 13,
  MOUNT_ERROR_SET_DIR_CRYPTO_KEY_FAILED = 14,
  MOUNT_ERROR_MOUNT_ECRYPTFS_FAILED = 15,
  MOUNT_ERROR_TPM_DEFEND_LOCK = 16,
  MOUNT_ERROR_SETUP_GROUP_ACCESS_FAILED = 17,
  MOUNT_ERROR_MOUNT_HOMES_AND_DAEMON_STORES_FAILED = 18,
  MOUNT_ERROR_TPM_UPDATE_REQUIRED = 19,
  // DANGER: returning this MOUNT_ERROR_VAULT_UNRECOVERABLE may cause vault
  // destruction. Only use it if the vault destruction is the
  // acceptable/expected behaviour upon returning error.
  MOUNT_ERROR_VAULT_UNRECOVERABLE = 20,
  MOUNT_ERROR_MOUNT_DMCRYPT_FAILED = 21,
  MOUNT_ERROR_RECOVERY_TRANSIENT = 22,
  MOUNT_ERROR_RECOVERY_FATAL = 23,
  // A login attempt has led to the user getting locked out (the attempt that
  // locks them out). If the user attempts to log in while they are locked out,
  // the error should be set to MOUNT_ERROR_TPM_DEFEND_LOCK.
  MOUNT_ERROR_CREDENTIAL_LOCKED = 24,
  MOUNT_ERROR_CREDENTIAL_EXPIRED = 25,
  MOUNT_ERROR_KEY_RESTORE_FAILED = 26,
  MOUNT_ERROR_USER_DOES_NOT_EXIST = 32,
  MOUNT_ERROR_TPM_NEEDS_REBOOT = 64,
  // Encrypted in old method, need migration before mounting.
  MOUNT_ERROR_OLD_ENCRYPTION = 128,
  // Previous migration attempt was aborted in the middle. Must resume it first.
  MOUNT_ERROR_PREVIOUS_MIGRATION_INCOMPLETE = 256,
  // The operation to remove a key failed.
  MOUNT_ERROR_REMOVE_FAILED = 512,
  MOUNT_ERROR_RECREATED = 1 << 31,
};
// Status code signaled from MigrateToDircrypto().
enum DircryptoMigrationStatus {
  // 0 means a successful completion.
  DIRCRYPTO_MIGRATION_SUCCESS = 0,
  // Negative values mean failing completion.
  // TODO(kinaba,dspaid): Add error codes as needed here.
  DIRCRYPTO_MIGRATION_FAILED = -1,
  // Positive values mean intermediate state report for the running migration.
  // TODO(kinaba,dspaid): Add state codes as needed.
  DIRCRYPTO_MIGRATION_INITIALIZING = 1,
  DIRCRYPTO_MIGRATION_IN_PROGRESS = 2,
};

// Interface for key delegate service to be used by the cryptohome daemon.

inline constexpr char kCryptohomeKeyDelegateInterface[] =
    "org.chromium.CryptohomeKeyDelegateInterface";

// Methods of the |kCryptohomeKeyDelegateInterface| interface:
inline constexpr char kCryptohomeKeyDelegateChallengeKey[] = "ChallengeKey";

}  // namespace cryptohome

#endif  // SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_
