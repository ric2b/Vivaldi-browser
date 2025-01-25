// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "chrome/android/chrome_jni_headers/VivaldiAccountManager_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

constexpr char kUsernameKey[] = "username";
constexpr char kPasswordKey[] = "password";
constexpr char kRecoveryEmailKey[] = "recovery_email";

static jlong JNI_VivaldiAccountManager_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  VivaldiAccountManagerAndroid* vivaldi_account_manager_android =
      new VivaldiAccountManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(vivaldi_account_manager_android);
}

VivaldiAccountManagerAndroid::VivaldiAccountManagerAndroid(JNIEnv* env,
                                                const base::android::JavaRef<jobject>& obj)
    : profile_(ProfileManager::GetActiveUserProfile()),
      weak_java_ref_(env, obj) {
  DCHECK(profile_);
  account_manager_ =
      vivaldi::VivaldiAccountManagerFactory::GetForProfile(profile_);
  account_manager_->AddObserver(this);

  SendStateUpdate();
}

VivaldiAccountManagerAndroid::~VivaldiAccountManagerAndroid() {
  account_manager_->RemoveObserver(this);
}

/*static*/
void VivaldiAccountManagerAndroid::CreateNow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VivaldiAccountManager_createNow(env);
}

void VivaldiAccountManagerAndroid::Login(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& password,
    jboolean save_password) {
  account_manager_->Login(base::android::ConvertJavaStringToUTF8(env, username),
                          base::android::ConvertJavaStringToUTF8(env, password),
                          save_password);
}

void VivaldiAccountManagerAndroid::Logout(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  account_manager_->Logout();
}

void VivaldiAccountManagerAndroid::SetSessionName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& session_name) {
  profile_->GetPrefs()->SetString(
      vivaldiprefs::kSyncSessionName,
      base::android::ConvertJavaStringToUTF8(env, session_name));
}

base::android::ScopedJavaLocalRef<jobject>
VivaldiAccountManagerAndroid::GetPendingRegistration(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  const base::Value& pending_registration = profile_->GetPrefs()->GetValue(
      vivaldiprefs::kVivaldiAccountPendingRegistration);

  const std::string* username =
      pending_registration.GetDict().FindString(kUsernameKey);
  const std::string* encoded_password =
      pending_registration.GetDict().FindString(kPasswordKey);
  const std::string* recovery_email =
      pending_registration.GetDict().FindString(kRecoveryEmailKey);

  if (!username || !encoded_password || !recovery_email)
    return nullptr;

  std::string encrypted_password;
  if (!base::Base64Decode(*encoded_password, &encrypted_password) ||
      encrypted_password.empty()) {
    return nullptr;
  }
  std::string password;
  // Android uses the posix implementation, which is non-blocking.
  if (!OSCrypt::DecryptString(encrypted_password, &password)) {
    return nullptr;
  }
  return Java_VivaldiAccountManager_populatePendingRegistration(
      env, obj, base::android::ConvertUTF8ToJavaString(env, *username),
      base::android::ConvertUTF8ToJavaString(env, password),
      base::android::ConvertUTF8ToJavaString(env, *recovery_email));
}

jboolean VivaldiAccountManagerAndroid::SetPendingRegistration(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& password,
    const base::android::JavaParamRef<jstring>& recovery_email) {
  std::string encrypted_password;
  // Android uses the posix implementation, which is non-blocking.
  if (!OSCrypt::EncryptString(
          base::android::ConvertJavaStringToUTF8(env, password),
          &encrypted_password)) {
    return false;
  }
  std::string encoded_password = base::Base64Encode(encrypted_password);
  base::Value pending_registration(base::Value::Type::DICT);

  pending_registration.GetDict().Set(
      kUsernameKey, base::android::ConvertJavaStringToUTF8(env, username));
  pending_registration.GetDict().Set(kPasswordKey, encoded_password);
  pending_registration.GetDict().Set(
      kRecoveryEmailKey,
      base::android::ConvertJavaStringToUTF8(env, recovery_email));

  profile_->GetPrefs()->Set(vivaldiprefs::kVivaldiAccountPendingRegistration,
                            pending_registration);
  return true;
}

void VivaldiAccountManagerAndroid::ResetPendingRegistration(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  profile_->GetPrefs()->ClearPref(
      vivaldiprefs::kVivaldiAccountPendingRegistration);
}

void VivaldiAccountManagerAndroid::SendStateUpdate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);

  ::vivaldi::VivaldiAccountManager::AccountInfo account_info =
      account_manager_->account_info();

  auto last_token_fetch_error = account_manager_->last_token_fetch_error();
  auto last_account_info_fetch_error =
      account_manager_->last_account_info_fetch_error();

  Java_VivaldiAccountManager_onStateUpdated(
      env, obj,
      base::android::ConvertUTF8ToJavaString(env, account_info.account_id),
      base::android::ConvertUTF8ToJavaString(env, account_info.username),
      base::android::ConvertUTF8ToJavaString(env, account_info.picture_url),
      base::android::ConvertUTF8ToJavaString(env, account_info.donation_tier),
      base::android::ConvertUTF8ToJavaString(
          env, profile_->GetPrefs()->GetString(vivaldiprefs::kSyncSessionName)),
      !account_manager_->password_handler()->password().empty(),
      account_manager_->has_refresh_token(),
      account_manager_->has_encrypted_refresh_token(),
      account_manager_->GetTokenRequestTime().InMillisecondsSinceUnixEpoch(),
      account_manager_->GetNextTokenRequestTime().InMillisecondsSinceUnixEpoch(),
      last_token_fetch_error.type,
      base::android::ConvertUTF8ToJavaString(
          env, last_token_fetch_error.server_message),
      last_token_fetch_error.error_code, last_account_info_fetch_error.type,
      base::android::ConvertUTF8ToJavaString(
          env, last_account_info_fetch_error.server_message),
      last_account_info_fetch_error.error_code);
}

void VivaldiAccountManagerAndroid::OnVivaldiAccountUpdated() {
  SendStateUpdate();
}

void VivaldiAccountManagerAndroid::OnTokenFetchSucceeded() {
  SendStateUpdate();
}

void VivaldiAccountManagerAndroid::OnTokenFetchFailed() {
  SendStateUpdate();
}

void VivaldiAccountManagerAndroid::OnVivaldiAccountShutdown() {}
