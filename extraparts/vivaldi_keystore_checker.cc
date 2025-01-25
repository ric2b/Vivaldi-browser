// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "base/base64.h"

#include "chrome/browser/profiles/profile.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "chrome/browser/ui/views/message_box_dialog.h"
#include "ui/vivaldi_message_box_dialog.h"
#include "ui/vivaldi_ui_utils.h"
#endif

#include "components/os_crypt/sync/os_crypt.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"

namespace vivaldi {
namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_WIN)
constexpr char kCanaryValue[] = "VivaldiKeystoreEncryptionCanary";

bool AskShouldAllowInsecureAccess() {
  if (!ui_tools::IsUIAvailable()) {
    LOG(WARNING) << "KeystoreChecker: AskShouldAllowInsecureAccess: UI Is not available yet. Returning NO to insecure access";
    return false;
  }

  VivaldiMessageBoxDialog::Config config{
      l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_FAILED_TITLE),
      l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_UNCRYPTED),
      chrome::MESSAGE_BOX_TYPE_QUESTION,
      l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_CONTINUE_DATALOSS),
      l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_CANCEL),
      u""  // l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_CONTINUE_CHECKBOX)
  };

  // Some extra configs:

  // Use cancel button as default - pressing enter will cause the dialog to cancel.
  config.cancel_default = true;

  // A reasonable sizing for the messagebox.
  config.size = gfx::Size(700, 250);

  auto result = VivaldiMessageBoxDialog::Show(nullptr, config);

  return result == chrome::MESSAGE_BOX_RESULT_YES;
}

// Describes the result of profile encryption key health analysis.
enum class CanaryStatus {
  kInvalid,
  kValid,
  kNotPresent  // Canary value was not present in the profile.
};

/// Verifies stored canary value, after decrypting, with our canary value.
CanaryStatus VerifyCanary(PrefService* preferences) {
  // also decrypt last stored canary value
  std::string b64 =
      preferences->GetString(vivaldiprefs::kStartupKeystoreCanary);

  // We have no canary, we pretend everything is fine...
  if (b64.empty()) {
    return CanaryStatus::kNotPresent;
  }

  std::string encrypted_canary;
  base::Base64Decode(b64, &encrypted_canary);

  std::string decrypted_canary;

  // Failure to decrypt indicates change in encryption password.
  if (!OSCrypt::DecryptString(encrypted_canary, &decrypted_canary)) {
    LOG(WARNING) << "KeystoreChecker: Decryption of the canary failed. "
                    "Keystore may have changed!";

    return CanaryStatus::kInvalid;
  }

  return (decrypted_canary == kCanaryValue) ? CanaryStatus::kValid
                                            : CanaryStatus::kInvalid;
}

void StoreCanary(PrefService* prefs) {
  std::string encrypted_canary;

  if (!OSCrypt::EncryptString(kCanaryValue, &encrypted_canary)) {
    return;
  }

  LOG(INFO) << "KeystoreChecker: Storing new canary value";

  std::string b64 = base::Base64Encode(encrypted_canary);
  prefs->SetString(vivaldiprefs::kStartupKeystoreCanary, b64);
}
#endif

}  // namespace

bool HasLockedKeystore(Profile* profile) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN)
  // No verification on mobile platforms.
  return false;
#else
  PrefService* preferences = profile->GetPrefs();

  // We intentionally ignore system profile problems.
  // This is due to the fact that System Profile is failback
  // when loading in the profile selection screen and we need
  // to be able to show it.
  if (profile->IsSystemProfile()) {
    return false;
  }

  // Initializing, do not touch the prefs. This happens when creating new
  // profile.
  if (preferences->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    LOG(INFO) << "KeyStoreChecker: Profile still initializing, can't check.";
    return false;
  }

  bool had_pref =
      preferences->HasPrefPath(vivaldiprefs::kStartupWasEncryptionUsed);
  bool was_encrypted =
      preferences->GetBoolean(vivaldiprefs::kStartupWasEncryptionUsed);

  bool is_encrypted = OSCrypt::IsEncryptionAvailable();

  // New profiles start without encryption info, we just store the status.
  if (profile->IsNewProfile() || !had_pref) {
    // Store whether the encryption is now available and a canary.
    preferences->SetBoolean(vivaldiprefs::kStartupWasEncryptionUsed,
                            is_encrypted);
    StoreCanary(preferences);
    return false;
  }

  CanaryStatus canary_status = VerifyCanary(preferences);

  if (!was_encrypted && is_encrypted) {
    // This profile was previously only used unencrypted. Using secure storage
    // could mean the situation below could happend if the user ever switches
    // back to unencrypted.
    LOG(ERROR) << "KeystoreChecker: Profile " << profile->GetBaseName()
               << ": Unencryted keystore was previously used but encryption is "
                  "used now. Upgrading status to secured keystore.";

    // Re-store canary, the encryption key was added and we need have the canary
    // to test for key changes
    StoreCanary(preferences);

    // We test if we dropped out of encryption or the key changed.
  } else if ((was_encrypted && !is_encrypted) ||
             (canary_status == CanaryStatus::kInvalid)) {
    // Was previously encrypted, and is not now. We need to let user know that
    // this will mean logins, cookies and stuff will get lost.
    LOG(ERROR) << "KeystoreChecker: Profile " << profile->GetBaseName()
               << ": Encryted keystore changed or is now unavailable. This may "
                  "result in lost cookies and other problems.";

    if (!AskShouldAllowInsecureAccess()) {
      LOG(ERROR) << "KeystoreChecker: Keystore unlock failed and user "
                    "requested profile switch!";
      return true;
    }
  }

  // Store whether the encryption is now available.
  preferences->SetBoolean(vivaldiprefs::kStartupWasEncryptionUsed,
                          is_encrypted);

  // Store a new canary value unless it was valid.
  if (canary_status != CanaryStatus::kValid) {
    StoreCanary(preferences);
  }
  return false;
#endif
}

bool InitOSCrypt(PrefService *local_state, bool *should_exit) {
#if BUILDFLAG(IS_WIN)
  OSCrypt::InitResult crypt_result = OSCrypt::InitWithExistingKey(local_state);
  if (crypt_result == OSCrypt::kDecryptionFailed) {
    // Ask user if they want the key to be overwritten...
    // This uses native message box internally as vivaldi is not yet
    // prepared to display normal message box.
    // TODO: Maybe customize this behavior via direct call to ui::MessageBox
    VivaldiMessageBoxDialog::Config config{
        l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_FAILED_TITLE),
        l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_UNCRYPTED),
        chrome::MESSAGE_BOX_TYPE_QUESTION,
        l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_CONTINUE_DATALOSS),
        l10n_util::GetStringUTF16(IDS_VIVALDI_KEYSTORE_QUIT),
        u""};
    // This makes the dialog safer and changes the type to warning.
    config.cancel_default = true;
    auto result = VivaldiMessageBoxDialog::Show(nullptr, config);

    // User requested browser exit. We set the flag and return init failed.
    if (!result) {
      *should_exit = true;
      return false;
    }

    // User does not want to terminate, we will rewrite the password
    // by calling OSCrypt::Init...
  }

  // Handle normal init in case it is still needed...
  *should_exit = false;
  bool os_crypt_init = true;
  if (crypt_result != OSCrypt::kSuccess) {
    // In case previous call was not succesful we still have to init the
    // key - this time potentially rewriting it.
    os_crypt_init = OSCrypt::Init(local_state);
  }

  return os_crypt_init;

#else
  return true;
#endif
}

}  // namespace vivaldi
