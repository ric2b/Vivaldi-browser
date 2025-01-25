// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_known_sources_handler_impl.h"

#include <map>
#include <utility>

#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_rule_service.h"

namespace adblock_filter {

namespace {
const char kDuckDuckGoList[] =
    "https://downloads.vivaldi.com/ddg/tds-v2-current.json";
const char kEasyList[] =
    "https://downloads.vivaldi.com/easylist/easylist-current.txt";
const char kAdblockPlusAntiCv[] =
    "https://downloads.vivaldi.com/lists/abp/abp-filters-anti-cv-current.txt";
const char kAdblockPlusAntiAdblock[] =
    "https://downloads.vivaldi.com/lists/abp/antiadblockfilters-current.txt";
const char kPartnersList[] =
    "https://downloads.vivaldi.com/lists/vivaldi/partners-current.txt";

const char kRussianList[] =
    "https://easylist-downloads.adblockplus.org/advblock.txt";

struct PermanentSource {
  std::string_view url;
  RuleSourceSettings settings;
};

const PermanentSource kPermanentKnownTrackingSources[] = {
    {kDuckDuckGoList, {}},
    {"https://downloads.vivaldi.com/easylist/easyprivacy-current.txt", {}}};

const PermanentSource kPermanentKnownAdBlockSources[] = {
    {kEasyList, {}},
    {kPartnersList, {.allow_attribution_tracker_rules = true}},
    {kAdblockPlusAntiCv, {.allow_abp_snippets = true}},
    {kAdblockPlusAntiAdblock, {.allow_abp_snippets = true}}};

struct PresetSourceInfo {
  std::string_view url;
  std::string_view id;
};

// NOTE: When removing preset sources:
//       If the source is removed because it's permanently unavailable, change
//       the URL to an empty string. This will force its removal from the users
//       list of source regardless of whether it's enabled, which avoids issues
//       if a malicious lists appears at the same address later on.
//       If the source is removed because we choose to stop offering it while it
//       is still valid, simply remove it from the list. This will cause it to
//       remain in the user's sources list if the user enabled it.
const PresetSourceInfo kPresetAdBlockSources[] = {
    {"https://raw.githubusercontent.com/heradhis/indonesianadblockrules/master/"
     "subscriptions/abpindo.txt",
     "f7bc721e-5cd1-440c-8036-50813c063929"},
    {"https://raw.githubusercontent.com/abpvn/abpvn/master/filter/abpvn.txt",
     "092a3a7f-b452-47e2-bbd7-b61e902ad0fd"},
    {"http://stanev.org/abp/adblock_bg.txt",
     "e5d554e9-8249-47c1-abf8-004cd29f4172"},
    {"https://easylist-downloads.adblockplus.org/easylistchina.txt",
     "439f5af1-9c74-4606-9b9e-b46863ac611c"},
    {"https://raw.githubusercontent.com/cjx82630/cjxlist/master/"
     "cjx-annoyance.txt",
     "923b5982-519e-4c7f-9854-3bd354b368b8"},
    {"https://raw.githubusercontent.com/tomasko126/easylistczechandslovak/"
     "master/filters.txt",
     "5c9b517d-5182-401a-aee6-ae32414ca708"},
    {"https://easylist-downloads.adblockplus.org/easylistdutch.txt",
     "acf379b6-2c46-4802-88c9-6dd46bedfb32"},
    {"https://easylist.to/easylistgermany/easylistgermany.txt",
     "933d897d-cb29-4282-a4f9-2451d83d1885"},
    {"https://raw.githubusercontent.com/easylist/EasyListHebrew/master/"
     "EasyListHebrew.txt",
     "22263ec8-d105-418a-a187-36f5c9808dcf"},
    {"https://easylist-downloads.adblockplus.org/easylistitaly.txt",
     "364fff45-270d-4a62-a449-982856057678"},
    {"https://raw.githubusercontent.com/EasyList-Lithuania/easylist_lithuania/"
     "master/easylistlithuania.txt",
     "4f1dbb65-d152-46c8-81db-b5f2cd6d66d5"},
    {"https://easylist-downloads.adblockplus.org/easylistpolish.txt",
     "ef6d3c42-e166-4901-9b03-58f124fbebf3"},
    {"https://easylist-downloads.adblockplus.org/easylistportuguese.txt",
     "b1d9732d-c0f3-4c74-8596-e1518b42b356"},
    {"https://easylist-downloads.adblockplus.org/easylistspanish.txt",
     "3eae7230-473c-4ccd-a15f-f08e4bb86f71"},
    {"https://easylist-downloads.adblockplus.org/indianlist.txt",
     "98ed727f-d9c0-4bc6-bded-19b14b52d167"},
    {"https://easylist-downloads.adblockplus.org/koreanlist.txt",
     "629f497d-0660-4b7d-8c82-afaf89345681"},
    {"https://notabug.org/latvian-list/adblock-latvian/raw/master/lists/"
     "latvian-list.txt",
     "1810bcfd-dad7-4c42-82bb-0fc33ebe7892"},
    {"https://easylist-downloads.adblockplus.org/Liste_AR.txt",
     "01b357a7-eddb-4dce-9c3f-4e90099bbfcd"},
    {"https://easylist-downloads.adblockplus.org/liste_fr.txt",
     "9be6251e-631e-4177-abec-d5dbef6be4f7"},
    {"https://www.zoso.ro/pages/rolist.txt",
     "434d57a1-51ac-480f-a5af-cc1c127f0313"},
    {kRussianList, "a3d2a41d-6659-4465-9819-ba8317185118"},
    {"https://raw.githubusercontent.com/yecarrillo/adblock-colombia/master/"
     "adblock_co.txt",
     "d0b816af-f803-4efa-9b8b-39bd1a0d5c75"},
    {"https://raw.githubusercontent.com/DandelionSprout/adfilt/master/"
     "NorwegianExperimentalList%20alternate%20versions/NordicFiltersABP.txt",
     "a93efa90-ebea-4df2-a1a4-972445bc6d0f"},
    {"https://adblock.gardar.net/is.abp.txt",
     "9bd24163-31fe-4889-b7e3-99e5bf742150"},
    {"https://www.void.gr/kargig/void-gr-filters.txt",
     "9cc5cd12-945e-4948-8ae4-266a21c9165c"},
    {"https://raw.githubusercontent.com/k2jp/abp-japanese-filters/master/"
     "abpjf.txt",
     "2450843a-66fb-4e8c-9c65-bdc530623690"},
    {"https://cdn.rawgit.com/SlashArash/adblockfa/master/adblockfa.txt",
     "0979cdbb-6581-4f56-a57b-f7dc16fb47f8"},
    // Removed Feb 2021 - Was Czech List
    // "http://adblock.dajbych.net/adblock.txt". Completely gone.
    {"", "73449266-40be-4c68-b5e8-ad68c8544e21"},
    {"https://adblock.ee/list.php", "288bb849-ca3b-4a6c-8c26-8f0f41e88af7"},
    {"https://gurud.ee/ab.txt", "d8d2b8a5-f918-4a5f-b03c-0ee921aec48f"},
    //  Removed Feb 2021 - Was Filtros Nauscopicos (Spanish)
    // "http://abp.mozilla-hispano.org/nauscopio/filtros.txt". Inaccessible due
    // to an HSTS issue and unmaintained. Expecting it to disappear.
    {"", "8e4f4bf9-5cba-40fc-b0f0-91d395c23dc7"},
    {"https://raw.githubusercontent.com/hufilter/hufilter/refs/heads/gh-pages/"
     "hufilter.txt",
     "5ec4c886-a4b7-4fd4-9654-a7a138bf74bf"},
    {"https://pgl.yoyo.org/adservers/"
     "serverlist.php?hostformat=adblockplus&mimetype=plaintext",
     "9c486dda-1589-4886-a40c-1be6484eb43d"},
    //  Removed Feb 2021 - Was Squid Black List
    // "https://www.squidblacklist.org/downloads/sbl-adblock.acl". Completely
    // gone.
    {"", "acf18485-785d-4a3e-9a58-321e6ae7f392"},
    {"https://raw.githubusercontent.com/gioxx/xfiles/master/filtri.txt",
     "53e46eb7-be5f-41b7-994c-d3155fc2025e"},
    {"https://raw.githubusercontent.com/yous/YousList/master/youslist.txt",
     "aa16a0f8-9ecf-40c1-9062-d72c153145af"},
    {"https://raw.githubusercontent.com/finnish-easylist-addition/"
     "finnish-easylist-addition/master/Finland_adb.txt",
     "c43fb9ca-bf75-4f07-ad52-1c79cd67a454"},
    {"https://raw.githubusercontent.com/eEIi0A5L/adblock_filter/master/"
     "mochi_filter.txt",
     "88f940b8-990c-4caa-abff-bcdb0bfd9276"},
    {"https://raw.githubusercontent.com/eEIi0A5L/adblock_filter/master/"
     "tamago_filter.txt",
     "366ed9e8-aa6e-4fd2-b3ff-bdc151f48fa9"},
    {"https://secure.fanboy.co.nz/fanboy-turkish.txt",
     "c29c4544-679b-4335-94f2-b27c7d099803"},
     // Removed Jan 2025 - Was I don't care about cookies
     // https://www.i-dont-care-about-cookies.eu/abp/
     // Not maintained anymore and expired certificate.
    {"", "c1e5bcb8-edf6-4a71-b61b-ca96a87f30e3"},
    {"https://secure.fanboy.co.nz/fanboy-cookiemonster.txt",
     "78610306-e2ab-4147-9a10-fb6072e6675e"},
    {"https://secure.fanboy.co.nz/fanboy-annoyance.txt",
     "269f589f-0a17-4158-a961-ee5252120dad"}};

struct PresetSourceInfoWithSize {
  /* Not a rwa ptr, because used for static data.*/
  RAW_PTR_EXCLUSION const PresetSourceInfo* preset_source_info;
  const size_t size;
};

const PresetSourceInfoWithSize kPresetRuleSources[] = {
    {nullptr, 0},
    {kPresetAdBlockSources, std::size(kPresetAdBlockSources)}};

}  // namespace

KnownRuleSourcesHandlerImpl::KnownRuleSourcesHandlerImpl(
    RuleService* rule_service,
    int storage_version,
    const std::string& locale,
    const std::array<std::vector<KnownRuleSource>, kRuleGroupCount>&
        known_sources,
    std::array<std::set<std::string>, kRuleGroupCount> deleted_presets,
    base::RepeatingClosure schedule_save)
    : rule_service_(rule_service),
      deleted_presets_(std::move(deleted_presets)),
      schedule_save_(std::move(schedule_save)) {
  for (const auto& permanent_source : kPermanentKnownTrackingSources) {
    KnownRuleSource source(
        *RuleSourceCore::FromUrl(GURL(permanent_source.url)));
    source.removable = false;
    source.core.set_settings(permanent_source.settings);
    known_sources_[static_cast<size_t>(RuleGroup::kTrackingRules)].insert(
        {source.core.id(), source});
  }

  for (const auto& permanent_source : kPermanentKnownAdBlockSources) {
    KnownRuleSource source(
        *RuleSourceCore::FromUrl(GURL(permanent_source.url)));
    source.removable = false;
    source.core.set_settings(permanent_source.settings);
    known_sources_[static_cast<size_t>(RuleGroup::kAdBlockingRules)].insert(
        {source.core.id(), source});
  }

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    for (const auto& source : known_sources[static_cast<size_t>(group)]) {
      known_sources_[static_cast<size_t>(group)].insert(
          {source.core.id(), source});
    }
  }

  if (storage_version < 2)
    ResetPresetSources(RuleGroup::kAdBlockingRules);
  else
    UpdateSourcesFromPresets(RuleGroup::kAdBlockingRules, false,
                             storage_version < 4);

  if (storage_version < 1) {
    EnableSource(RuleGroup::kTrackingRules,
                 RuleSourceCore::FromUrl(GURL(kDuckDuckGoList))->id());
    EnableSource(RuleGroup::kAdBlockingRules,
                 RuleSourceCore::FromUrl(GURL(kEasyList))->id());
  }
  if (storage_version < 3) {
    EnableSource(RuleGroup::kAdBlockingRules,
                 RuleSourceCore::FromUrl(GURL(kPartnersList))->id());
  }

  if (storage_version < 5 &&
      (locale == "ru" || locale == "be" || locale == "uk")) {
    EnableSource(RuleGroup::kAdBlockingRules,
                 RuleSourceCore::FromUrl(GURL(kRussianList))->id());
  }

  if (storage_version < 6) {
    EnableSource(RuleGroup::kAdBlockingRules,
                 RuleSourceCore::FromUrl(GURL(kAdblockPlusAntiCv))->id());
  }

  if (storage_version < 7) {
    bool skip = false;
    // Avoid enabling our cached version of the list if the user added it
    // already by its original URL
    for (const auto& known_source : GetSourceMap(RuleGroup::kAdBlockingRules)) {
      if (known_source.second.core.is_from_url() &&
          known_source.second.core.source_url() ==
              GURL("https://easylist-downloads.adblockplus.org/"
                   "antiadblockfilters.txt")) {
        skip = true;
        break;
      }
    }
    if (!skip) {
      EnableSource(
          RuleGroup::kAdBlockingRules,
          RuleSourceCore::FromUrl(GURL(kAdblockPlusAntiAdblock))->id());
    }
  }

  if (storage_version < 10) {
    uint32_t partner_list_id =
        RuleSourceCore::FromUrl(GURL(kPartnersList))->id();
    if (IsSourceEnabled(RuleGroup::kAdBlockingRules, partner_list_id)) {
      // This forces the partner list to be reloaded with the ad attribution
      // option enabled.
      DisableSource(RuleGroup::kAdBlockingRules, partner_list_id);
      EnableSource(RuleGroup::kAdBlockingRules, partner_list_id);
    }
  }
}

KnownRuleSourcesHandlerImpl::~KnownRuleSourcesHandlerImpl() = default;

KnownRuleSources& KnownRuleSourcesHandlerImpl::GetSourceMap(RuleGroup group) {
  return known_sources_[static_cast<size_t>(group)];
}

const KnownRuleSources& KnownRuleSourcesHandlerImpl::GetSourceMap(
    RuleGroup group) const {
  return known_sources_[static_cast<size_t>(group)];
}

const KnownRuleSources& KnownRuleSourcesHandlerImpl::GetSources(
    RuleGroup group) const {
  return GetSourceMap(group);
}

const std::set<std::string>& KnownRuleSourcesHandlerImpl::GetDeletedPresets(
    RuleGroup group) const {
  return deleted_presets_[static_cast<size_t>(group)];
}

bool KnownRuleSourcesHandlerImpl::AddSource(RuleGroup group,
                                            RuleSourceCore source_core) {
  return AddSource(group, KnownRuleSource(std::move(source_core)), true);
}

bool KnownRuleSourcesHandlerImpl::AddSource(RuleGroup group,
                                            KnownRuleSource known_source,
                                            bool enable) {
  KnownRuleSources& known_sources = GetSourceMap(group);
  auto result = known_sources.try_emplace(known_source.core.id(),
                                          std::move(known_source));

  // since the id is just a hash of the file path, if a source with the same id
  // exist, we have a source with the exact same path already.
  if (!result.second) {
    return false;
  }

  schedule_save_.Run();

  for (Observer& observer : observers_)
    observer.OnKnownSourceAdded(group, result.first->second);

  if (enable)
    EnableSource(group, known_source.core.id());

  return true;
}

std::optional<KnownRuleSource> KnownRuleSourcesHandlerImpl::GetSource(
    RuleGroup group,
    uint32_t source_id) {
  KnownRuleSources& known_sources = GetSourceMap(group);
  const auto known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return std::nullopt;
  return known_source->second;
}

bool KnownRuleSourcesHandlerImpl::RemoveSource(RuleGroup group,
                                               uint32_t source_id) {
  KnownRuleSources& known_sources = GetSourceMap(group);
  const auto known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return true;

  if (!known_source->second.removable)
    return false;

  DisableSource(group, source_id);
  if (!known_source->second.preset_id.empty())
    deleted_presets_[static_cast<size_t>(group)].insert(
        known_source->second.preset_id);
  known_sources.erase(known_source);

  schedule_save_.Run();

  for (Observer& observer : observers_)
    observer.OnKnownSourceRemoved(group, source_id);
  return true;
}

bool KnownRuleSourcesHandlerImpl::EnableSource(RuleGroup group,
                                               uint32_t source_id) {
  KnownRuleSources& known_sources = GetSourceMap(group);
  const auto known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return false;

  if (IsSourceEnabled(group, source_id))
    return true;

  bool result = rule_service_->GetRuleManager()->AddRulesSource(
      group, known_source->second.core);

  DCHECK(result);

  for (Observer& observer : observers_)
    observer.OnKnownSourceEnabled(group, source_id);

  return result;
}

void KnownRuleSourcesHandlerImpl::DisableSource(RuleGroup group,
                                                uint32_t source_id) {
  KnownRuleSources& known_sources = GetSourceMap(group);
  const auto known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return;

  rule_service_->GetRuleManager()->DeleteRuleSource(group,
                                                    known_source->second.core);

  for (Observer& observer : observers_)
    observer.OnKnownSourceDisabled(group, source_id);
}

bool KnownRuleSourcesHandlerImpl::IsSourceEnabled(RuleGroup group,
                                                  uint32_t source_id) {
  return rule_service_->GetRuleManager()
      ->GetRuleSource(group, source_id)
      .has_value();
}

bool KnownRuleSourcesHandlerImpl::SetSourceSettings(
    RuleGroup group,
    uint32_t source_id,
    RuleSourceSettings settings) {
  if (IsSourceEnabled(group, source_id)) {
    return false;
  }

  KnownRuleSources& known_sources = GetSourceMap(group);
  const auto known_source = known_sources.find(source_id);
  if (known_source == known_sources.end()) {
    return false;
  }

  if (!known_source->second.removable) {
    return false;
  }

  known_source->second.core.set_settings(settings);
  schedule_save_.Run();
  return true;
}

void KnownRuleSourcesHandlerImpl::ResetPresetSources(RuleGroup group) {
  UpdateSourcesFromPresets(group, true, false);
}

void KnownRuleSourcesHandlerImpl::UpdateSourcesFromPresets(
    RuleGroup group,
    bool add_deleted_presets,
    bool store_missing_as_deleted) {
  // Doesn't make sense to do both at the same time.
  DCHECK(!add_deleted_presets || !store_missing_as_deleted);
  KnownRuleSources& known_sources = GetSourceMap(group);

  if (add_deleted_presets)
    deleted_presets_[static_cast<size_t>(group)].clear();

  std::map<std::string, uint32_t> known_presets;

  if (!kPresetRuleSources[static_cast<size_t>(group)].size)
    return;

  for (const auto& known_source : known_sources) {
    if (!known_source.second.preset_id.empty())
      known_presets[known_source.second.preset_id] = known_source.first;
  }

  for (size_t i = 0; i < kPresetRuleSources[static_cast<size_t>(group)].size;
       i++) {
    const PresetSourceInfo& preset =
        kPresetRuleSources[static_cast<size_t>(group)].preset_source_info[i];
    if (preset.url.empty()) {
      // Empty URL means forcibly remove.
      const auto known_preset = known_presets.find(std::string(preset.id));
      if (known_preset != known_presets.end()) {
        RemoveSource(group, known_preset->second);
        known_presets.erase(known_preset);
      }
      continue;
    }

    KnownRuleSource preset_source(*RuleSourceCore::FromUrl(GURL(preset.url)));

    auto known_source = known_sources.find(preset_source.core.id());
    // We already have a rule source with that URL
    if (known_source != known_sources.end()) {
      // It wasn't added manually
      if (!known_source->second.preset_id.empty()) {
        // Keep the |preset_id| up to date if needed. This should only ever do
        // something if there was an issue with storage.
        known_source->second.preset_id = std::string(preset.id);

        known_presets.erase(std::string(preset.id));
      }
      // If it was added manually, but we had another source with this preset's
      // ID, it probably means we've updated a preset to a new URL but that
      // the user added that same URL in the meantime. In that case, if the old
      // preset source is still present, it will be erased below as it will
      // remain part of the leftovers in |knows_presets|.
      continue;
    }
    preset_source.preset_id = std::string(preset.id);

    const auto known_preset = known_presets.find(std::string(preset.id));
    if (known_preset != known_presets.end()) {
      // If there was a source with a URL matching this preset, it would have
      // been handled above.
      DCHECK(known_preset->second != preset_source.core.id());

      bool enable = IsSourceEnabled(group, known_preset->second);
      RemoveSource(group, known_preset->second);
      known_presets.erase(known_preset);
      AddSource(group, std::move(preset_source), enable);
    } else if (store_missing_as_deleted) {
      // NOTE(julien): We weren't keeping track of deleted presets before.
      // This allows us to remedy that for people who had old setups.
      // This will break addition of new presets for those people, so we
      // shouldn't add new presets too soon after this.
      deleted_presets_[static_cast<size_t>(group)].insert(
          std::string(preset.id));
    } else if (deleted_presets_[static_cast<size_t>(group)].count(
                   std::string(preset.id)) == 0) {
      AddSource(group, std::move(preset_source), false);
    }
  }

  for (auto& known_preset : known_presets) {
    // Get rid of sources that come from a removed preset, unless they are
    // enabled. We do this because we expect that preset removal is done either
    // because a list has died out or because we were specifically asked to
    // remove support for it.
    // Clear the preset id before removal, so it doesn't end up being stored
    // in the list of deleted presets.
    known_sources.at(known_preset.second).preset_id.clear();
    if (!IsSourceEnabled(group, known_preset.second))
      RemoveSource(group, known_preset.second);
  }

  schedule_save_.Run();
}

void KnownRuleSourcesHandlerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void KnownRuleSourcesHandlerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace adblock_filter
