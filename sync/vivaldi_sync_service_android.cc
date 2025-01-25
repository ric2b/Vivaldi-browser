// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_service_android.h"

#include "app/vivaldi_apptools.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/VivaldiSyncService_jni.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sync/service/sync_token_status.h"
#include "sync/vivaldi_sync_service_impl.h"
#include "sync/vivaldi_sync_ui_helpers.h"

static jlong JNI_VivaldiSyncService_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  VivaldiSyncServiceAndroid* vivaldi_account_manager_android =
      new VivaldiSyncServiceAndroid(env, obj);
  if (!vivaldi_account_manager_android->Init(env)) {
    delete vivaldi_account_manager_android;
    return 0;
  }
  return reinterpret_cast<intptr_t>(vivaldi_account_manager_android);
}

VivaldiSyncServiceAndroid::VivaldiSyncServiceAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj)
    : weak_java_ref_(env, obj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  sync_service_ =
      SyncServiceFactory::GetForProfile(profile);

  SendCycleData();
}

VivaldiSyncServiceAndroid::~VivaldiSyncServiceAndroid() {
  sync_service_->RemoveObserver(this);
}

bool VivaldiSyncServiceAndroid::Init(JNIEnv* env) {
  if (sync_service_) {
    sync_service_->AddObserver(this);
    return true;
  }
  return false;
}

jboolean VivaldiSyncServiceAndroid::SetEncryptionPassword(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& password) {
  return vivaldi::sync_ui_helpers::SetEncryptionPassword(sync_service_,
      base::android::ConvertJavaStringToUTF8(env, password));
}

void VivaldiSyncServiceAndroid::ClearServerData(JNIEnv* env) {
  if (vivaldi::IsVivaldiRunning())
    sync_service_->ClearSyncData();
}

jboolean VivaldiSyncServiceAndroid::HasServerError(JNIEnv* env) {
  return sync_service_->GetSyncTokenStatusForDebugging().connection_status ==
         syncer::CONNECTION_SERVER_ERROR;
}

jboolean VivaldiSyncServiceAndroid::IsSetupInProgress(JNIEnv* env) {
  return sync_service_->IsSetupInProgress();
}

base::android::ScopedJavaLocalRef<jstring>
VivaldiSyncServiceAndroid::GetBackupEncryptionToken(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, vivaldi::sync_ui_helpers::GetBackupEncryptionToken(sync_service_));
}

jboolean VivaldiSyncServiceAndroid::RestoreEncryptionToken(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& token) {
  return vivaldi::sync_ui_helpers::RestoreEncryptionToken(sync_service_,
      base::android::ConvertJavaStringToUTF8(env, token));
}

jboolean VivaldiSyncServiceAndroid::CanSyncFeatureStart(JNIEnv* env) {
  return sync_service_->CanSyncFeatureStart();
}

void VivaldiSyncServiceAndroid::SendCycleData() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);

  vivaldi::sync_ui_helpers::CycleData cycle_data =
      vivaldi::sync_ui_helpers::GetCycleData(sync_service_);

  Java_VivaldiSyncService_onCycleData(
      env, obj, cycle_data.download_updates_status, cycle_data.commit_status,
      cycle_data.cycle_start_time.InMillisecondsSinceUnixEpoch(),
      cycle_data.next_retry_time.InMillisecondsSinceUnixEpoch());
}

void VivaldiSyncServiceAndroid::OnSyncCycleCompleted(
    syncer::SyncService* sync) {
  SendCycleData();
}