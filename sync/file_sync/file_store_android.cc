#include "sync/file_sync/file_store_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SyncedFileStore_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_factory.h"

static jlong JNI_SyncedFileStore_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  SyncedFileStoreAndroid* synced_file_store =
      new SyncedFileStoreAndroid(env, obj);
  return reinterpret_cast<intptr_t>(synced_file_store);
}

SyncedFileStoreAndroid::SyncedFileStoreAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj)
    : weak_java_ref_(env, obj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  file_store_ = SyncedFileStoreFactory::GetForBrowserContext(profile);
}

SyncedFileStoreAndroid::~SyncedFileStoreAndroid() = default;

void SyncedFileStoreAndroid::GetFile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& checksum,
    const base::android::JavaParamRef<jobject> callback) {
  auto forward_content = [](JNIEnv* env, JavaObjectWeakGlobalRef weak_callback,
                            std::optional<base::span<const uint8_t>> content) {
    auto callback = weak_callback.get(env);
    if (!callback)
      return;
    if (!content) {
      Java_SyncedFileStore_onGetFileCallback(
          env, base::android::JavaRef<jbyteArray>(nullptr), callback);
    } else {
      Java_SyncedFileStore_onGetFileCallback(
          env, base::android::ToJavaByteArray(env, *content), callback);
    }
  };
  file_store_->GetFile(base::android::ConvertJavaStringToUTF8(env, checksum),
                       base::BindOnce(forward_content, env,
                                      JavaObjectWeakGlobalRef(env, callback)));
}
