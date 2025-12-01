// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_AD_BLOCKER_ANDROID_ADBLOCK_MANAGER_H_
#define BROWSER_AD_BLOCKER_ANDROID_ADBLOCK_MANAGER_H_

#include <memory>
#include <set>

#include "build/build_config.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/content/adblock_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_known_sources_handler.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"
#include "components/ad_blocker/public/core/adblock_stats_store.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_weak_ref.h"
#endif

class PrefService;

namespace adblock_filter {
class RuleService;
}

class AdblockManager : public adblock_filter::RuleService::Observer,
                       public adblock_filter::RuleManager::Observer,
                       public adblock_filter::KnownRuleSourcesHandler::Observer,
                       public adblock_filter::StateAndLogs::Observer {
 public:
  AdblockManager(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 adblock_filter::RuleService* rule_service,
                 PrefService* prefs);
  ~AdblockManager() override;
  AdblockManager(const AdblockManager&) = delete;
  AdblockManager& operator=(const AdblockManager&) = delete;

  void SetActiveExceptionsList(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jint group,
                               jint list);

  jint GetActiveExceptionsList(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jint group);

  void AddExceptionForDomain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint group,
      jint list,
      const base::android::JavaParamRef<jstring>& domain);

  void RemoveExceptionForDomain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint group,
      jint list,
      const base::android::JavaParamRef<jstring>& domain);

  void RemoveAllExceptions(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint group,
                           jint list);

  base::android::ScopedJavaLocalRef<jobjectArray> GetExceptions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint group,
      jint list) const;

  base::android::ScopedJavaLocalRef<jobjectArray> GetBlockedUrlsInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint group,
      const base::android::JavaParamRef<jobject>& web_contents) const;

  base::android::ScopedJavaLocalRef<jstring> GetAdAtributionDomain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& web_contents) const;

  bool IsAdAttributionActive(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& web_contents) const;

  bool IsPartnerAdsShown(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& web_contents) const;

  base::android::ScopedJavaLocalRef<jobjectArray>
  GetAllowedAdAttributionTrackers(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& web_contents) const;

  bool IsExemptOfFiltering(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           const jint group,
                           const base::android::JavaParamRef<jstring>& domain);

  base::android::ScopedJavaLocalRef<jstring> GetRuleSource(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jint group,
      const jlong source_id) const;

  base::android::ScopedJavaLocalRef<jobjectArray> GetRuleSources(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jint group) const;

  jlong AddSourceFromUrl(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const jint group,
                         const base::android::JavaParamRef<jstring>& url);

  jlong AddSourceFromFile(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const jint group,
                          const base::android::JavaParamRef<jstring>& file);

  bool SetSourceSettings(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const jint group,
                         const jlong source_id,
                         bool allow_apb_snippets,
                         bool naked_hostname_is_pure_host,
                         bool use_whole_document_allow);

  bool RemoveSource(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const jint group,
                    const jlong source_id);

  bool EnableSource(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const jint group,
                    const jlong source_id);

  void DisableSource(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const jint group,
                     const jlong source_id);

  bool IsSourceEnabled(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const jint group,
                       const jlong source_id);

  void ResetPresetSources(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const jint group);

  bool IsDocumentBlockingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetDocumentBlockingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      bool enabled);

  void GetBlockingData(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const jint interval);

  // adblock_filter::RuleService::Observer implementation
  void OnRuleServiceStateLoaded(
      adblock_filter::RuleService* rule_service) override;

  // adblock_filter::RuleManager::Observer implementation
  void OnRuleSourceUpdated(
      adblock_filter::RuleGroup group,
      const adblock_filter::ActiveRuleSource& rule_source) override;

  void OnRuleSourceDeleted(uint32_t source_id,
                           adblock_filter::RuleGroup group) override;

  void OnExceptionListStateChanged(adblock_filter::RuleGroup group) override;

  void OnExceptionListChanged(
      adblock_filter::RuleGroup group,
      adblock_filter::RuleManager::ExceptionsList list) override;

  // adblock_filter::KnownRuleSourcesHandler::Observer implementation
  void OnKnownSourceAdded(
      adblock_filter::RuleGroup group,
      const adblock_filter::KnownRuleSource& rule_source) override;

  void OnKnownSourceRemoved(adblock_filter::RuleGroup group,
                            uint32_t source_id) override;

  void OnKnownSourceEnabled(adblock_filter::RuleGroup group,
                            uint32_t source_id) override;

  void OnKnownSourceDisabled(adblock_filter::RuleGroup group,
                             uint32_t source_id) override;

  // adblock_filter::StateAndLogs::Observer implementation
  void OnNewBlockedUrlsReported(
      adblock_filter::RuleGroup group,
      std::set<content::WebContents*> tabs_with_new_blocks) override;
  void OnAllowAttributionChanged(content::WebContents* web_contents) override;
  void OnNewAttributionTrackerAllowed(
      std::set<content::WebContents*> tabs_with_new_attribution_trackers)
      override;

  bool IsInitialized() const;

  void OnStatsDataLoaded(std::unique_ptr<adblock_filter::StatsData> data);

 private:
  raw_ptr<adblock_filter::RuleService> rule_service_ = nullptr;
  raw_ptr<PrefService> prefs_ = nullptr;
  JavaObjectWeakGlobalRef weak_java_ref_;
};

#endif  // BROWSER_AD_BLOCKER_ANDROID_ADBLOCK_MANAGER_H_
