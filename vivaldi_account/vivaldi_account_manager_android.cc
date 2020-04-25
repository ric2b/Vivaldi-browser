// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/VivaldiAccountManager_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

static jlong JNI_VivaldiAccountManager_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  VivaldiAccountManagerAndroid* vivaldi_account_manager_android =
      new VivaldiAccountManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(vivaldi_account_manager_android);
}

VivaldiAccountManagerAndroid::VivaldiAccountManagerAndroid(JNIEnv* env,
                                                           jobject obj)
    : weak_java_ref_(env, obj) {
  profile_ = ProfileManager::GetActiveUserProfile();
  DCHECK(profile_);
  account_manager_ =
      vivaldi::VivaldiAccountManagerFactory::GetForProfile(profile_);
  account_manager_->AddObserver(this);

  SendStateUpdate();
}

VivaldiAccountManagerAndroid::~VivaldiAccountManagerAndroid() {
  account_manager_->RemoveObserver(this);
}

void VivaldiAccountManagerAndroid::Login(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& password,
    const base::android::JavaParamRef<jstring>& session_name,
    jboolean save_password) {
  profile_->GetPrefs()->SetString(
      vivaldiprefs::kSyncSessionName,
      base::android::ConvertJavaStringToUTF8(env, session_name));
  account_manager_->Login(base::android::ConvertJavaStringToUTF8(env, username),
                          base::android::ConvertJavaStringToUTF8(env, password),
                          save_password);
}

void VivaldiAccountManagerAndroid::Logout(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  account_manager_->Logout();
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
      base::android::ConvertUTF8ToJavaString(
          env,
          profile_->GetPrefs()->GetString(vivaldiprefs::kSyncSessionName)),
      !account_manager_->password_handler()->password().empty(),
      account_manager_->has_refresh_token(),
      account_manager_->has_encrypted_refresh_token(),
      account_manager_->GetTokenRequestTime().ToJavaTime(),
      account_manager_->GetNextTokenRequestTime().ToJavaTime(),
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

void VivaldiAccountManagerAndroid::OnAccountInfoFetchFailed() {
  SendStateUpdate();
}

void VivaldiAccountManagerAndroid::OnVivaldiAccountShutdown() {}
