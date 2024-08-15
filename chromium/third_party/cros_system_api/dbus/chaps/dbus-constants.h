// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_CHAPS_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_CHAPS_DBUS_CONSTANTS_H_

#include <stdint.h>
#include <cstdint>

namespace chaps {

inline constexpr uint64_t kTokenLabelSize = 32;

// Chaps-specific attributes:

// PKCS #11 v2.20 section A Manifest constants page 377. PKCS11_ prefix is added
// to avoid name collisions with #define-d constants.
inline constexpr uint32_t PKCS11_CKA_VENDOR_DEFINED = 0x80000000;
inline constexpr uint32_t kKeyBlobAttribute = PKCS11_CKA_VENDOR_DEFINED + 1;
inline constexpr uint32_t kAuthDataAttribute = PKCS11_CKA_VENDOR_DEFINED + 2;
// If this attribute is set to true at creation or generation time, then the
// object will not be stored/wrapped in hardware-backed security element, and
// will remain purely in software.
inline constexpr uint32_t kForceSoftwareAttribute =
    PKCS11_CKA_VENDOR_DEFINED + 4;
// This attribute is set to false if the key is stored in hardware-backed
// security element, and true otherwise.
inline constexpr uint32_t kKeyInSoftwareAttribute =
    PKCS11_CKA_VENDOR_DEFINED + 5;
// If this attribute is set to true at creation or generation time, then the
// object may be generated in software, but still stored/wrapped in the
// hardware-backed security element.
inline constexpr uint32_t kAllowSoftwareGenAttribute =
    PKCS11_CKA_VENDOR_DEFINED + 6;
// If this attribute is set to true at creation or generation time, then the
// object can be wrapped with the kChapsKeyWrapMechanism (define below). The
// attribute can be changed from CK_TRUE to CK_FALSE, but not the other way
// around.
inline constexpr uint32_t kChapsWrappableAttribute =
    PKCS11_CKA_VENDOR_DEFINED + 7;

// Chaps-specific mechanisms:

// PKCS #11 v2.20 section A Manifest constants page 381. PKCS11_ prefix is added
// to avoid name collisions with #define-d constants.
inline constexpr uint32_t PKCS11_CKM_VENDOR_DEFINED = 0x80000000UL;
inline constexpr uint32_t CKM_CHAPS_SPECIFIC_FIRST =
    PKCS11_CKM_VENDOR_DEFINED + 0x10000000;

// The kChapsKeyWrapMechanism mechanism can wrap and unwrap a target key of any
// length and type using chaps' internal random seed during the wrapping/
// unwrapping process. This mechanism is used when we want to securely move a
// key between tokens on the same device (specifically, from the system-token to
// the user-token), while the wrapped_key cannot be decrypted without having
// access to chaps' internal random seed.
//
// The mechanism is designed based on the CKM_AES_KEY_WRAP_KWP, which is using
// a same AES key to wrap/unwrap the target key. However, instead of retrieving
// the wrapping/unwrapping key from the handle, kChapsKeyWrapMechanism uses
// chaps' internal random seed (which is shared between chaps tokens) to derive
// the temporary AES key. As a result, no wrapping/unwrapping key is needed for
// this mechanism.
//
// For wrapping, the mechanism -
//  1. Generates a random blob of length=32.
//  2. Use HmacSha512() with input [random blob] and [Chaps' random seed] to
//     derive a temporary AES key.
//  3. Wraps the target key with the temporary AES key using
//     CKM_AES_KEY_WRAP_KWP ([AES KEYWRAP] section 6.3).
//  4. Zeroizes the temporary AES key
//  5. Fill the [random blob] and the wrapped target key into some protobuf and
//     output the serialized result.
//
// For unwrapping, the mechanism -
//  1. Deserializes the input protobuf and obtains the [random blob] and the
//     wrapped target key.
//  2. Use HmacSha512() with input [random blob] and [Chaps' random seed] to
//     derive a temporary AES key. Note that [Chaps' random seed] is shared
//     across tokens so we'll obtain the same temporary AES key.
//  3. Unwraps the target key with the temporary AES key using
//     CKM_AES_KEY_WRAP_KWP ([AES KEYWRAP] section 6.3).
//  4. Zeroizes the temporary AES key.
//  5. Returns the handle to the newly unwrapped target key.
inline constexpr uint32_t kChapsKeyWrapMechanism = CKM_CHAPS_SPECIFIC_FIRST + 1;

// Chaps-specific return values:

// PKCS #11 v2.20 section A Manifest constants page 382. PKCS11_ prefix is added
// to avoid name collisions with #define-d constants.
inline constexpr uint32_t PKCS11_CKR_VENDOR_DEFINED = 0x80000000UL;
inline constexpr uint32_t CKR_CHAPS_SPECIFIC_FIRST =
    PKCS11_CKR_VENDOR_DEFINED + 0x47474c00;
// Error code returned in case if the operation would block waiting
// for private objects to load for the token. This value is persisted to logs
// and should not be renumbered and numeric values should never be reused.
// Please keep in sync with "ChapsSessionStatus" in
// tools/metrics/histograms/enums.xml in the Chromium repo.
inline constexpr uint32_t CKR_WOULD_BLOCK_FOR_PRIVATE_OBJECTS =
    CKR_CHAPS_SPECIFIC_FIRST + 0;
// Client side error code returned in case the D-Bus client is null.
inline constexpr uint32_t CKR_DBUS_CLIENT_IS_NULL =
    CKR_CHAPS_SPECIFIC_FIRST + 1;
// Client side error code returned in case D-Bus returned an empty response.
inline constexpr uint32_t CKR_DBUS_EMPTY_RESPONSE_ERROR =
    CKR_CHAPS_SPECIFIC_FIRST + 2;
// Client side error code returned in case the D-Bus response couldn't be
// decoded.
inline constexpr uint32_t CKR_DBUS_DECODING_ERROR =
    CKR_CHAPS_SPECIFIC_FIRST + 3;
// Client side error code returned in case a new PKCS#11 session could not be
// opened. It is useful to differentiate from CKR_SESSION_HANDLE_INVALID and
// CKR_SESSION_CLOSED errors because for those the receiver is expected to retry
// the operation immediately and kFailedToOpenSessionError indicates a more
// persistent failure.
inline constexpr uint32_t CKR_FAILED_TO_OPEN_SESSION =
    CKR_CHAPS_SPECIFIC_FIRST + 4;

// D-Bus service constants.
inline constexpr char kChapsInterface[] = "org.chromium.Chaps";
inline constexpr char kChapsServiceName[] = "org.chromium.Chaps";
inline constexpr char kChapsServicePath[] = "/org/chromium/Chaps";

// Methods, should be kept in sync with the
// chaps/dbus_bindings/org.chromium.Chaps.xml file. "OpenIsolate",
// "CloseIsolate", "InitPIN", "SetPIN", "Login", "Logout" methods are excluded
// because they are unlikely to be used.
inline constexpr char kLoadTokenMethod[] = "LoadToken";
inline constexpr char kUnloadTokenMethod[] = "UnloadToken";
inline constexpr char kGetTokenPathMethod[] = "GetTokenPath";
inline constexpr char kSetLogLevelMethod[] = "SetLogLevel";
inline constexpr char kGetSlotListMethod[] = "GetSlotList";
inline constexpr char kGetSlotInfoMethod[] = "GetSlotInfo";
inline constexpr char kGetTokenInfoMethod[] = "GetTokenInfo";
inline constexpr char kGetMechanismListMethod[] = "GetMechanismList";
inline constexpr char kGetMechanismInfoMethod[] = "GetMechanismInfo";
inline constexpr char kInitTokenMethod[] = "InitToken";
inline constexpr char kOpenSessionMethod[] = "OpenSession";
inline constexpr char kCloseSessionMethod[] = "CloseSession";
inline constexpr char kGetSessionInfoMethod[] = "GetSessionInfo";
inline constexpr char kGetOperationStateMethod[] = "GetOperationState";
inline constexpr char kSetOperationStateMethod[] = "SetOperationState";
inline constexpr char kCreateObjectMethod[] = "CreateObject";
inline constexpr char kCopyObjectMethod[] = "CopyObject";
inline constexpr char kDestroyObjectMethod[] = "DestroyObject";
inline constexpr char kGetObjectSizeMethod[] = "GetObjectSize";
inline constexpr char kGetAttributeValueMethod[] = "GetAttributeValue";
inline constexpr char kSetAttributeValueMethod[] = "SetAttributeValue";
inline constexpr char kFindObjectsInitMethod[] = "FindObjectsInit";
inline constexpr char kFindObjectsMethod[] = "FindObjects";
inline constexpr char kFindObjectsFinalMethod[] = "FindObjectsFinal";
inline constexpr char kEncryptInitMethod[] = "EncryptInit";
inline constexpr char kEncryptMethod[] = "Encrypt";
inline constexpr char kEncryptUpdateMethod[] = "EncryptUpdate";
inline constexpr char kEncryptFinalMethod[] = "EncryptFinal";
inline constexpr char kEncryptCancelMethod[] = "EncryptCancel";
inline constexpr char kDecryptInitMethod[] = "DecryptInit";
inline constexpr char kDecryptMethod[] = "Decrypt";
inline constexpr char kDecryptUpdateMethod[] = "DecryptUpdate";
inline constexpr char kDecryptFinalMethod[] = "DecryptFinal";
inline constexpr char kDecryptCancelMethod[] = "DecryptCancel";
inline constexpr char kDigestInitMethod[] = "DigestInit";
inline constexpr char kDigestMethod[] = "Digest";
inline constexpr char kDigestUpdateMethod[] = "DigestUpdate";
inline constexpr char kDigestKeyMethod[] = "DigestKey";
inline constexpr char kDigestFinalMethod[] = "DigestFinal";
inline constexpr char kDigestCancelMethod[] = "DigestCancel";
inline constexpr char kSignInitMethod[] = "SignInit";
inline constexpr char kSignMethod[] = "Sign";
inline constexpr char kSignUpdateMethod[] = "SignUpdate";
inline constexpr char kSignFinalMethod[] = "SignFinal";
inline constexpr char kSignCancelMethod[] = "SignCancel";
inline constexpr char kSignRecoverInitMethod[] = "SignRecoverInit";
inline constexpr char kSignRecoverMethod[] = "SignRecover";
inline constexpr char kVerifyInitMethod[] = "VerifyInit";
inline constexpr char kVerifyMethod[] = "Verify";
inline constexpr char kVerifyUpdateMethod[] = "VerifyUpdate";
inline constexpr char kVerifyFinalMethod[] = "VerifyFinal";
inline constexpr char kVerifyCancelMethod[] = "VerifyCancel";
inline constexpr char kVerifyRecoverInitMethod[] = "VerifyRecoverInit";
inline constexpr char kVerifyRecoverMethod[] = "VerifyRecover";
inline constexpr char kDigestEncryptUpdateMethod[] = "DigestEncryptUpdate";
inline constexpr char kDecryptDigestUpdateMethod[] = "DecryptDigestUpdate";
inline constexpr char kSignEncryptUpdateMethod[] = "SignEncryptUpdate";
inline constexpr char kDecryptVerifyUpdateMethod[] = "DecryptVerifyUpdate";
inline constexpr char kGenerateKeyMethod[] = "GenerateKey";
inline constexpr char kGenerateKeyPairMethod[] = "GenerateKeyPair";
inline constexpr char kWrapKeyMethod[] = "WrapKey";
inline constexpr char kUnwrapKeyMethod[] = "UnwrapKey";
inline constexpr char kDeriveKeyMethod[] = "DeriveKey";
inline constexpr char kSeedRandomMethod[] = "SeedRandom";
inline constexpr char kGenerateRandomMethod[] = "GenerateRandom";

}  // namespace chaps

#endif  // SYSTEM_API_DBUS_CHAPS_DBUS_CONSTANTS_H_
