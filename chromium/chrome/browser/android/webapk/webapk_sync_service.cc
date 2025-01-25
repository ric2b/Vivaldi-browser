// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_service.h"

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sync/base/features.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkSyncService_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace webapk {

namespace {

// Called after getting restorable app info.
void OnGotAppsInfo(const JavaRef<jobject>& java_callback,
                   const std::vector<std::string>& app_ids,
                   const std::vector<std::u16string>& names,
                   const std::vector<int>& last_used_in_days,
                   const std::vector<SkBitmap>& icons) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmaps =
      Java_WebApkSyncService_createBitmapList(env);
  for (const SkBitmap& bitmap : icons) {
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(bitmap);
    Java_WebApkSyncService_addToBitmapList(env, jbitmaps, jbitmap);
  }
  Java_PwaRestorableListCallback_onRestorableAppsAvailable(
      env, java_callback, true,
      base::android::ToJavaArrayOfStrings(env, app_ids),
      base::android::ToJavaArrayOfStrings(env, names),
      base::android::ToJavaIntArray(env, last_used_in_days), jbitmaps);
}

}  // anonymous namespace

// static
WebApkSyncService* WebApkSyncService::GetForProfile(Profile* profile) {
  return WebApkSyncServiceFactory::GetForProfile(profile);
}

WebApkSyncService::WebApkSyncService(Profile* profile) {
  database_factory_ = std::make_unique<WebApkDatabaseFactory>(profile);
  sync_bridge_ = std::make_unique<WebApkSyncBridge>(database_factory_.get(),
                                                    base::DoNothing());
  restore_manager_ = std::make_unique<WebApkRestoreManager>(profile);
}

WebApkSyncService::~WebApkSyncService() = default;

void WebApkSyncService::RegisterDoneInitializingCallback(
    base::OnceCallback<void(bool)> init_done_callback) {
  sync_bridge_->RegisterDoneInitializingCallback(std::move(init_done_callback));
}

void WebApkSyncService::MergeSyncDataForTesting(
    std::vector<std::vector<std::string>> app_vector,
    std::vector<int> last_used_days_vector) {
  sync_bridge_->MergeSyncDataForTesting(
      std::move(app_vector), std::move(last_used_days_vector));  // IN-TEST
}

void WebApkSyncService::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  sync_bridge_->SetClockForTesting(std::move(clock));  // IN-TEST
}

const Registry& WebApkSyncService::GetRegistryForTesting() const {
  return sync_bridge_->GetRegistryForTesting();  // IN-TEST
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
WebApkSyncService::GetDataTypeControllerDelegate() {
  return sync_bridge_->GetDataTypeControllerDelegate();
}

void WebApkSyncService::OnWebApkUsed(
    std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics,
    bool is_install) {
  sync_bridge_->OnWebApkUsed(std::move(app_specifics), is_install);
}

void WebApkSyncService::OnWebApkUninstalled(const std::string& manifest_id) {
  sync_bridge_->OnWebApkUninstalled(manifest_id);
}

void WebApkSyncService::RemoveOldWebAPKsFromSync(
    int64_t current_time_ms_since_unix_epoch) {
  sync_bridge_->RemoveOldWebAPKsFromSync(current_time_ms_since_unix_epoch);
}

void WebApkSyncService::PrepareRestorableAppsInfo(
    WebApkRestoreManager::RestorableAppsCallback result_callback) const {
  restore_manager_->PrepareRestorableApps(
      sync_bridge_->GetRestorableAppsShortcutInfo(),
      std::move(result_callback));
}

WebApkRestoreManager* WebApkSyncService::GetWebApkRestoreManager() const {
  return restore_manager_.get();
}

// static
static void JNI_WebApkSyncService_OnWebApkUsed(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& java_webapk_specifics,
    jboolean is_install) {
  if (!base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return;
  }

  std::string specifics_bytes;
  base::android::JavaByteArrayToString(env, java_webapk_specifics,
                                       &specifics_bytes);

  std::unique_ptr<sync_pb::WebApkSpecifics> specifics =
      std::make_unique<sync_pb::WebApkSpecifics>();
  if (!specifics->ParseFromString(specifics_bytes)) {
    LOG(ERROR) << "failed to parse WebApkSpecifics proto";
    return;
  }
  WebApkSyncService::GetForProfile(profile)->OnWebApkUsed(
      std::move(specifics), static_cast<bool>(is_install));
}

static void JNI_WebApkSyncService_OnWebApkUninstalled(
    JNIEnv* env,
    std::string& java_manifest_id) {
  if (!base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return;
  }

  WebApkSyncService::GetForProfile(profile)->OnWebApkUninstalled(
      java_manifest_id);
}

static void JNI_WebApkSyncService_RemoveOldWebAPKsFromSync(
    JNIEnv* env,
    jlong java_current_time_ms_since_unix_epoch) {
  if (!base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return;
  }

  WebApkSyncService::GetForProfile(profile)->RemoveOldWebAPKsFromSync(
      static_cast<int64_t>(java_current_time_ms_since_unix_epoch));
}

static void JNI_WebApkSyncService_FetchRestorableApps(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& java_callback) {
  if (profile == nullptr ||
      !base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  ScopedJavaGlobalRef<jobject> callback_ref(java_callback);
  WebApkSyncService::GetForProfile(profile)->PrepareRestorableAppsInfo(
      base::BindOnce(&OnGotAppsInfo, callback_ref));
}

}  // namespace webapk
