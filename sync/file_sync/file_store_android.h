// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef SYNC_FILE_SYNC_FILE_STORE_ANDROID_H_
#define SYNC_FILE_SYNC_FILE_STORE_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace file_sync {
class SyncedFileStore;
}

class SyncedFileStoreAndroid {
 public:
  SyncedFileStoreAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);
  ~SyncedFileStoreAndroid();
  SyncedFileStoreAndroid(const SyncedFileStoreAndroid&) = delete;
  SyncedFileStoreAndroid& operator=(const SyncedFileStoreAndroid&) = delete;

  void GetFile(JNIEnv* env,
               const base::android::JavaParamRef<jstring>& checksum,
               const base::android::JavaParamRef<jobject> callback);

 private:
  raw_ptr<file_sync::SyncedFileStore> file_store_;
  JavaObjectWeakGlobalRef weak_java_ref_;
};

#endif  // SYNC_FILE_SYNC_FILE_STORE_ANDROID_H_
