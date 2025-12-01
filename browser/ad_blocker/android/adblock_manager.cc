// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#include "adblock_manager.h"

#include "base/strings/string_number_conversions.h"
#include "browser/ad_blocker/adblock_rule_service_factory.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/toolbar/jni_headers/AdblockManager_jni.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/content/adblock_state_and_logs.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_known_sources_handler.h"
#include "components/ad_blocker/public/core/adblock_stats_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "ui/web_ui_native_call_utils.h"
#endif

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

static jlong JNI_AdblockManager_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  if (!profile) {
    LOG(ERROR) << "Could not get active user profile for ad blocker.";
    return 0;
  }

  adblock_filter::RuleService* rule_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(profile);
  if (!rule_service) {
    LOG(ERROR) << "Could not get rule service for ad blocker.";
    return 0;
  }

  return reinterpret_cast<intptr_t>(
      new AdblockManager(env, obj, rule_service, profile->GetPrefs()));
}

AdblockManager::AdblockManager(JNIEnv* env,
                               const JavaRef<jobject>& obj,
                               adblock_filter::RuleService* rule_service,
                               PrefService* prefs)
    : rule_service_(rule_service), prefs_(prefs), weak_java_ref_(env, obj) {
  rule_service_->AddObserver(this);
  if (rule_service_->IsLoaded()) {
    OnRuleServiceStateLoaded(rule_service_);
  }
}

AdblockManager::~AdblockManager() {
  if (rule_service_->IsLoaded()) {
    rule_service_->GetRuleManager()->RemoveObserver(this);
    rule_service_->GetStateAndLogs()->RemoveObserver(this);
    rule_service_->GetKnownSourcesHandler()->RemoveObserver(this);
  }
  rule_service_->RemoveObserver(this);
}

void AdblockManager::SetActiveExceptionsList(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jint group,
                                             jint list) {
  CHECK(rule_service_->IsLoaded());
  rule_service_->GetRuleManager()->SetActiveExceptionList(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<adblock_filter::RuleManager::ExceptionsList>(list));
}

jint AdblockManager::GetActiveExceptionsList(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jint group) {
  CHECK(rule_service_->IsLoaded());
  return rule_service_->GetRuleManager()->GetActiveExceptionList(
      static_cast<adblock_filter::RuleGroup>(group));
}

void AdblockManager::AddExceptionForDomain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint group,
    jint list,
    const JavaParamRef<jstring>& domain) {
  CHECK(rule_service_->IsLoaded());
  rule_service_->GetRuleManager()->AddExceptionForDomain(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<adblock_filter::RuleManager::ExceptionsList>(list),
      base::android::ConvertJavaStringToUTF8(env, domain));
}

void AdblockManager::RemoveExceptionForDomain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint group,
    jint list,
    const JavaParamRef<jstring>& domain) {
  CHECK(rule_service_->IsLoaded());
  rule_service_->GetRuleManager()->RemoveExceptionForDomain(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<adblock_filter::RuleManager::ExceptionsList>(list),
      base::android::ConvertJavaStringToUTF8(env, domain));
}

void AdblockManager::RemoveAllExceptions(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jint group,
                                         jint list) {
  CHECK(rule_service_->IsLoaded());
  rule_service_->GetRuleManager()->RemoveAllExceptions(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<adblock_filter::RuleManager::ExceptionsList>(list));
}

ScopedJavaLocalRef<jobjectArray> AdblockManager::GetExceptions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint group,
    jint list) const {
  CHECK(rule_service_->IsLoaded());
  const std::set<std::string>& exceptions =
      rule_service_->GetRuleManager()->GetExceptions(
          static_cast<adblock_filter::RuleGroup>(group),
          static_cast<adblock_filter::RuleManager::ExceptionsList>(list));
  std::vector<std::string> exceptions_vector(exceptions.begin(),
                                             exceptions.end());
  return base::android::ToJavaArrayOfStrings(env, exceptions_vector);
}

ScopedJavaLocalRef<jobjectArray> AdblockManager::GetBlockedUrlsInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint group,
    const JavaParamRef<jobject>& web_contents) const {
  CHECK(rule_service_->IsLoaded());
  std::vector<std::string> blocked_urls;
  adblock_filter::TabStateAndLogs* tab_state_and_logs =
      rule_service_->GetStateAndLogs()->GetTabHelper(
          WebContents::FromJavaWebContents(web_contents));
  if (tab_state_and_logs) {
    const adblock_filter::TabStateAndLogs::TabBlockedUrlInfo&
        tab_blocked_urls_info = tab_state_and_logs->GetBlockedUrlsInfo(
            static_cast<adblock_filter::RuleGroup>(group));
    std::string str_total_count =
        base::NumberToString(tab_blocked_urls_info.total_count);
    blocked_urls.push_back(str_total_count);
    for (const auto& blocked_tracker_info :
         tab_blocked_urls_info.blocked_trackers) {
      std::string str_blocked_domain;
      if (blocked_tracker_info.second.blocked_count > 1) {
        str_blocked_domain =
            base::NumberToString(blocked_tracker_info.second.blocked_count) +
            " ";
      }
      str_blocked_domain += blocked_tracker_info.first;
      blocked_urls.push_back(str_blocked_domain);
    }
    for (const auto& blocked_urls_info : tab_blocked_urls_info.blocked_urls) {
      std::string str_blocked_url;
      if (blocked_urls_info.second.blocked_count > 1) {
        str_blocked_url =
            base::NumberToString(blocked_urls_info.second.blocked_count) + " ";
      }
      str_blocked_url += blocked_urls_info.first;
      blocked_urls.push_back(str_blocked_url);
    }
  }
  return base::android::ToJavaArrayOfStrings(env, blocked_urls);
}

ScopedJavaLocalRef<jstring> AdblockManager::GetAdAtributionDomain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& web_contents) const {
  CHECK(rule_service_->IsLoaded());
  adblock_filter::TabStateAndLogs* tab_state_and_logs =
      rule_service_->GetStateAndLogs()->GetTabHelper(
          WebContents::FromJavaWebContents(web_contents));
  std::string result = "";
  if (tab_state_and_logs) {
    result = tab_state_and_logs->GetCurrentAdLandingDomain();
  }
  return base::android::ConvertUTF8ToJavaString(env, result);
}

bool AdblockManager::IsAdAttributionActive(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& web_contents) const {
  CHECK(rule_service_->IsLoaded());
  adblock_filter::TabStateAndLogs* tab_state_and_logs =
      rule_service_->GetStateAndLogs()->GetTabHelper(
          WebContents::FromJavaWebContents(web_contents));
  return tab_state_and_logs && tab_state_and_logs->IsOnAdLandingSite();
}

bool AdblockManager::IsPartnerAdsShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& web_contents) const {
  CHECK(rule_service_->IsLoaded());

  return rule_service_->HasDocumentActivationForRuleSource(
      adblock_filter::RuleGroup::kAdBlockingRules,
      WebContents::FromJavaWebContents(web_contents),
      base::Uuid::ParseLowercase(
          adblock_filter::KnownRuleSourcesHandler::kPartnersListUuid));
}

base::android::ScopedJavaLocalRef<jobjectArray>
AdblockManager::GetAllowedAdAttributionTrackers(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& web_contents) const {
  CHECK(rule_service_->IsLoaded());
  adblock_filter::TabStateAndLogs* tab_state_and_logs =
      rule_service_->GetStateAndLogs()->GetTabHelper(
          WebContents::FromJavaWebContents(web_contents));
  std::vector<std::string> allowed_trackers;
  if (tab_state_and_logs) {
    allowed_trackers = std::vector<std::string>(
        tab_state_and_logs->GetAllowedAttributionTrackers().begin(),
        tab_state_and_logs->GetAllowedAttributionTrackers().end());
  }
  return base::android::ToJavaArrayOfStrings(env, allowed_trackers);
}

bool AdblockManager::IsExemptOfFiltering(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         const jint group,
                                         const JavaParamRef<jstring>& domain) {
  CHECK(rule_service_->IsLoaded());
  return rule_service_->GetRuleManager()->IsExemptOfFiltering(
      static_cast<adblock_filter::RuleGroup>(group),
      url::Origin::Create(
          GURL(base::android::ConvertJavaStringToUTF8(env, domain))));
}

namespace {

struct CombinedRuleSource {
  uint32_t id;
  std::string title;
  base::Time last_update;
  std::string rules_list_checksum;
  bool is_from_url;
  GURL source_url;
  base::FilePath source_file;
  bool removable;
  bool loaded;
  std::string preset_id;
  std::optional<adblock_filter::PresetKind> preset_kind;
  bool allow_abp_snippets;
  bool naked_hostname_is_pure_host;
  bool use_whole_document_allow;
};

const char kDelimiter[] = "##";

// Format of data transferred over JNI:
// id##title##last_update##rules_list_checksum##is_from_url##source##removable
// ##loaded
std::string SerializeRuleSource(const CombinedRuleSource& source) {
  std::string data = base::NumberToString(source.id);
  data += kDelimiter + source.title;
  data += kDelimiter + base::NumberToString(
                           source.last_update.InMillisecondsSinceUnixEpoch());
  data += kDelimiter + source.rules_list_checksum;
  data += kDelimiter;
  data += source.is_from_url ? "1" : "0";
  data += kDelimiter;
  data += source.is_from_url ? source.source_url.spec()
                             : source.source_file.AsUTF8Unsafe();
  data += kDelimiter;
  data += source.removable ? "1" : "0";
  data += kDelimiter;
  data += source.loaded ? "1" : "0";
  data += kDelimiter;
  data += source.preset_id;
  data += kDelimiter;
  data += source.preset_kind
              ? base::NumberToString(static_cast<size_t>(*source.preset_kind))
              : "";
  data += kDelimiter;
  data += source.allow_abp_snippets ? "1" : "0";
  data += kDelimiter;
  data += source.naked_hostname_is_pure_host ? "1" : "0";
  data += kDelimiter;
  data += source.use_whole_document_allow ? "1" : "0";
  return data;
}

}  // namespace

ScopedJavaLocalRef<jstring> AdblockManager::GetRuleSource(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const jlong source_id) const {
  CHECK(rule_service_->IsLoaded());
  auto known_source = rule_service_->GetKnownSourcesHandler()->GetSource(
      static_cast<adblock_filter::RuleGroup>(group), source_id);
  if (!known_source)
    return ScopedJavaLocalRef<jstring>();

  CombinedRuleSource crs;
  crs.id = known_source->core.id();
  crs.is_from_url = known_source->core.is_from_url();
  if (crs.is_from_url) {
    crs.source_url = known_source->core.source_url();
  } else {
    crs.source_file = known_source->core.source_file();
  }
  crs.removable = known_source->removable;
  crs.loaded = false;
  crs.preset_id = known_source->preset_id.AsLowercaseString();
  crs.preset_kind = known_source->preset_kind;
  crs.allow_abp_snippets = known_source->core.settings().allow_abp_snippets;
  crs.naked_hostname_is_pure_host =
      known_source->core.settings().naked_hostname_is_pure_host;
  crs.use_whole_document_allow =
      known_source->core.settings().use_whole_document_allow;

  auto loaded_source = rule_service_->GetRuleManager()->GetRuleSource(
      static_cast<adblock_filter::RuleGroup>(group), source_id);
  if (loaded_source) {
    crs.title = loaded_source->unsafe_adblock_metadata.title;
    crs.last_update = loaded_source->last_update;
    crs.rules_list_checksum = loaded_source->rules_list_checksum;
    crs.loaded = true;
  }
  return base::android::ConvertUTF8ToJavaString(env, SerializeRuleSource(crs));
}

ScopedJavaLocalRef<jobjectArray> AdblockManager::GetRuleSources(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const jint group) const {
  CHECK(rule_service_->IsLoaded());
  std::vector<std::string> rule_sources;

  auto known_sources = rule_service_->GetKnownSourcesHandler()->GetSources(
      static_cast<adblock_filter::RuleGroup>(group));
  for (const auto& [id, known_source] : known_sources) {
    CombinedRuleSource crs;
    crs.id = id;
    crs.is_from_url = known_source.core.is_from_url();
    if (crs.is_from_url) {
      crs.source_url = known_source.core.source_url();
    } else {
      crs.source_file = known_source.core.source_file();
    }
    crs.removable = known_source.removable;
    crs.preset_id = known_source.preset_id.AsLowercaseString();
    crs.preset_kind = known_source.preset_kind;
    crs.loaded = false;

    auto loaded_source = rule_service_->GetRuleManager()->GetRuleSource(
        static_cast<adblock_filter::RuleGroup>(group), id);

    if (loaded_source) {
      crs.title = loaded_source->unsafe_adblock_metadata.title;
      crs.last_update = loaded_source->last_update;
      crs.rules_list_checksum = loaded_source->rules_list_checksum;
      crs.loaded = true;
    }
    rule_sources.push_back(SerializeRuleSource(crs));
  }
  return base::android::ToJavaArrayOfStrings(env, rule_sources);
}

jlong AdblockManager::AddSourceFromUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const base::android::JavaParamRef<jstring>& url) {
  CHECK(rule_service_->IsLoaded());
  auto core = adblock_filter::RuleSourceCore::FromUrl(
      GURL(base::android::ConvertJavaStringToUTF8(env, url)));
  if (!core) {
    return 0;
  }
  auto source_id = core->id();
  if (!rule_service_->GetKnownSourcesHandler()->AddSource(
          static_cast<adblock_filter::RuleGroup>(group), *core)) {
    return 0;
  }
  return source_id;
}

jlong AdblockManager::AddSourceFromFile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const base::android::JavaParamRef<jstring>& file) {
  CHECK(rule_service_->IsLoaded());
  auto core =
      adblock_filter::RuleSourceCore::FromFile(base::FilePath::FromUTF8Unsafe(
          base::android::ConvertJavaStringToUTF8(env, file)));
  if (!core) {
    return 0;
  }
  auto source_id = core->id();
  if (!rule_service_->GetKnownSourcesHandler()->AddSource(
          static_cast<adblock_filter::RuleGroup>(group), *core)) {
    return 0;
  }
  return source_id;
}

bool AdblockManager::SetSourceSettings(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const jlong source_id,
    bool allow_apb_snippets,
    bool naked_hostname_is_pure_host,
    bool use_whole_document_allow) {
  CHECK(rule_service_->IsLoaded());
  return rule_service_->GetKnownSourcesHandler()->SetSourceSettings(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<uint32_t>(source_id),
      adblock_filter::RuleSourceSettings{
          .allow_abp_snippets = allow_apb_snippets,
          .naked_hostname_is_pure_host = naked_hostname_is_pure_host,
          .use_whole_document_allow = use_whole_document_allow});
}

bool AdblockManager::RemoveSource(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const jlong source_id) {
  CHECK(rule_service_->IsLoaded());
  return rule_service_->GetKnownSourcesHandler()->RemoveSource(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<uint32_t>(source_id));
}

bool AdblockManager::EnableSource(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const jlong source_id) {
  CHECK(rule_service_->IsLoaded());
  return rule_service_->GetKnownSourcesHandler()->EnableSource(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<uint32_t>(source_id));
}

void AdblockManager::DisableSource(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const jlong source_id) {
  CHECK(rule_service_->IsLoaded());
  rule_service_->GetKnownSourcesHandler()->DisableSource(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<uint32_t>(source_id));
}

bool AdblockManager::IsSourceEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group,
    const jlong source_id) {
  CHECK(rule_service_->IsLoaded());
  return rule_service_->GetKnownSourcesHandler()->IsSourceEnabled(
      static_cast<adblock_filter::RuleGroup>(group),
      static_cast<uint32_t>(source_id));
}

void AdblockManager::ResetPresetSources(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint group) {
  CHECK(rule_service_->IsLoaded());
  rule_service_->GetKnownSourcesHandler()->ResetPresetSources(
      static_cast<adblock_filter::RuleGroup>(group));
}

bool AdblockManager::IsDocumentBlockingEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return prefs_->GetBoolean(
      vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking);
}

void AdblockManager::SetDocumentBlockingEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool enabled) {
  prefs_->SetBoolean(vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking,
                     enabled);
}

void AdblockManager::OnStatsDataLoaded(
    const std::unique_ptr<adblock_filter::StatsData> data) {
  const int64_t adsBlocked = data->TotalAdsBlocked();
  const int64_t trackersBlocked = data->TotalTrackersBlocked();
  webUINativeCalls::createPrivacyReportNotification(adsBlocked,
                                                    trackersBlocked);
}

void AdblockManager::GetBlockingData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint interval) {
  CHECK(rule_service_->IsLoaded());
  const auto* stats_store = rule_service_->GetStatsStore();
  auto callback = base::BindOnce(&AdblockManager::OnStatsDataLoaded,
                                 base::Unretained(this));
  base::Time interval_time = base::Time::Now() - base::Days(interval);
  stats_store->GetStatsData(interval_time, base::Time::Now(),
                            std::move(callback));
}

void AdblockManager::OnRuleServiceStateLoaded(
    adblock_filter::RuleService* rule_service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;

  rule_service->GetRuleManager()->AddObserver(this);
  rule_service->GetStateAndLogs()->AddObserver(this);
  rule_service->GetKnownSourcesHandler()->AddObserver(this);

  Java_AdblockManager_onAdblockServiceLoaded(env, obj);
}

void AdblockManager::OnRuleSourceUpdated(
    adblock_filter::RuleGroup group,
    const adblock_filter::ActiveRuleSource& rule_source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onRuleSourceUpdated(
      env, obj, rule_source.core.id(), static_cast<int>(group),
      static_cast<int>(rule_source.last_fetch_result));
}

void AdblockManager::OnRuleSourceDeleted(uint32_t source_id,
                                         adblock_filter::RuleGroup group) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onRuleSourceDeleted(env, obj, source_id,
                                          static_cast<int>(group));
}

void AdblockManager::OnKnownSourceAdded(
    adblock_filter::RuleGroup group,
    const adblock_filter::KnownRuleSource& rule_source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onKnownSourceAdded(env, obj, static_cast<int>(group),
                                         rule_source.core.id());
}

void AdblockManager::OnKnownSourceRemoved(adblock_filter::RuleGroup group,
                                          uint32_t source_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onKnownSourceRemoved(env, obj, static_cast<int>(group),
                                           source_id);
}

void AdblockManager::OnKnownSourceEnabled(adblock_filter::RuleGroup group,
                                          uint32_t source_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onKnownSourceEnabled(env, obj, static_cast<int>(group),
                                           source_id);
}

void AdblockManager::OnKnownSourceDisabled(adblock_filter::RuleGroup group,
                                           uint32_t source_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onKnownSourceDisabled(env, obj, static_cast<int>(group),
                                            source_id);
}

void AdblockManager::OnExceptionListStateChanged(
    adblock_filter::RuleGroup group) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onExceptionListStateChanged(env, obj,
                                                  static_cast<int>(group));
}

void AdblockManager::OnExceptionListChanged(
    adblock_filter::RuleGroup group,
    adblock_filter::RuleManager::ExceptionsList list) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onExceptionListChanged(env, obj, static_cast<int>(group),
                                             static_cast<int>(list));
}

void AdblockManager::OnNewBlockedUrlsReported(
    adblock_filter::RuleGroup group,
    std::set<content::WebContents*> tabs_with_new_blocks) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onNewBlockedUrlsReported(env, obj,
                                               static_cast<int>(group));
}

void AdblockManager::OnAllowAttributionChanged(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onAllowAttributionChanged(env, obj);
}

void AdblockManager::OnNewAttributionTrackerAllowed(
    std::set<content::WebContents*> tabs_with_new_attribution_trackers) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AdblockManager_onNewAttributionTrackerAllowed(env, obj);
}
