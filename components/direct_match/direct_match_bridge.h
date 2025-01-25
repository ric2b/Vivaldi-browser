#ifndef COMPONENTS_DIRECTMATCH_DIRECT_MATCH_BRIDGE_H_
#define COMPONENTS_DIRECTMATCH_DIRECT_MATCH_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/direct_match/direct_match_service.h"

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"

class Profile;

// The delegate to fetch Direct Match information for Android native
class DirectMatchBridge : public direct_match::DirectMatchService::Observer {
 public:
  DirectMatchBridge(JNIEnv* env,
              const base::android::JavaRef<jobject>& obj,
              const base::android::JavaRef<jobject>& j_profile);
  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);
  void GetPopularSites(JNIEnv* env,
     const base::android::JavaParamRef<jobject>& j_result_obj);
  jni_zero::ScopedJavaLocalRef<jobject> CreateJavaDirectMatchItem(
  const direct_match::DirectMatchService::DirectMatchUnit* unit);
  const std::vector<const direct_match::DirectMatchService::DirectMatchUnit *>
            GetDirectMatchItemList(int category);
  void AddItemsToDirectMatchItemList(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_result_obj,
    const std::vector<const direct_match::DirectMatchService::DirectMatchUnit*>& nodes);
  void GetDirectMatchesForCategory(JNIEnv* env, int category_id,
    const base::android::JavaParamRef<jobject>& j_result_obj);
  void DirectMatchUnitsDownloaded();
  void DirectMatchIconsDownloaded();

void OnFinishedDownloadingDirectMatchUnits();
void OnFinishedDownloadingDirectMatchUnitsIcon();

 private:
  ~DirectMatchBridge();
  DirectMatchBridge(const DirectMatchBridge&) = delete;
  DirectMatchBridge& operator=(const DirectMatchBridge&) = delete;

  const raw_ptr<Profile> profile_;
  direct_match::DirectMatchService* directmatch_service;
  JavaObjectWeakGlobalRef weak_java_ref_;
};

#endif  // COMPONENTS_DIRECTMATCH_DIRECT_MATCH_BRIDGE_H_
