// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_PROFILE_SYNC_SERVICE_ANDROID_H_
#define SYNC_VIVALDI_PROFILE_SYNC_SERVICE_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/sync/driver/sync_service_observer.h"

namespace vivaldi {
class VivaldiSyncServiceImpl;
}

class Profile;

class VivaldiSyncServiceAndroid : public syncer::SyncServiceObserver {
 public:
  VivaldiSyncServiceAndroid(JNIEnv* env, jobject obj);
  ~VivaldiSyncServiceAndroid() override;
  VivaldiSyncServiceAndroid(const VivaldiSyncServiceAndroid&) = delete;
  VivaldiSyncServiceAndroid& operator=(const VivaldiSyncServiceAndroid&) =
      delete;

  bool Init();

  jboolean SetEncryptionPassword(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& password);

  void ClearServerData(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  void StopAndClear(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);

  jboolean HasServerError(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  jboolean IsSetupInProgress(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jstring> GetBackupEncryptionToken(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean RestoreEncryptionToken(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& token);

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  Profile* profile_;
  vivaldi::VivaldiSyncServiceImpl* sync_service_;

  void SendCycleData();

  JavaObjectWeakGlobalRef weak_java_ref_;
};

#endif  // SYNC_VIVALDI_PROFILE_SYNC_SERVICE_ANDROID_H_
