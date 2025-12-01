// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_known_sources_handler_impl.h"

#include <map>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_rule_service.h"

namespace adblock_filter {

namespace {
constexpr char kDuckDuckGoList[] =
    "https://downloads.vivaldi.com/ddg/tds-v2-current.json";
constexpr char kEasyList[] =
    "https://downloads.vivaldi.com/easylist/easylist-current.txt";
constexpr char kAdblockPlusAntiCv[] =
    "https://downloads.vivaldi.com/lists/abp/abp-filters-anti-cv-current.txt";
constexpr char kAdblockPlusAntiAdblock[] =
    "https://downloads.vivaldi.com/lists/abp/antiadblockfilters-current.txt";
constexpr char kPartnersList[] =
    "https://downloads.vivaldi.com/lists/vivaldi/partners-current.txt";

constexpr char kRussianList[] =
    "https://easylist-downloads.adblockplus.org/advblock.txt";

struct PresetSourceInfo {
  PresetKind kind;
  std::string_view url;
  RuleSourceSettings settings = {};
  bool removable = true;
};

// NOTE: When removing preset sources:
//       If the source is removed because it's permanently unavailable, change
//       the URL to an empty string. This will force its removal from the users
//       list of source regardless of whether it's enabled, which avoids issues
//       if a malicious lists appears at the same address later on.
//       If the source is removed because we choose to stop offering it while it
//       is still valid, simply remove it from the list. This will cause it to
//       remain in the user's sources list if the user enabled it.
constexpr auto kPresetSources = base::MakeFixedFlatMap<std::string_view,
                                                       PresetSourceInfo>(
    {{"bb9915f1-2e78-4fa2-9c84-b5022e7b0b09",
      {.kind = PresetKind::kTracking,
       .url = kDuckDuckGoList,
       .removable = false}},
     {"18d85f61-147a-4aa6-aa6d-607f5ef79eef",
      {.kind = PresetKind::kTracking,
       .url = "https://downloads.vivaldi.com/easylist/easyprivacy-current.txt",
       .removable = false}},
     {"8bef4dce-11b7-4452-8055-80eae7939dfb",
      {.kind = PresetKind::kAds, .url = kEasyList, .removable = false}},
     {"3b9a77ec-f9cc-4626-ace3-bd2e2d679d4c",
      {.kind = PresetKind::kAds,
       .url = kAdblockPlusAntiCv,
       .settings = {.allow_abp_snippets = true},
       .removable = false}},
     {"bbf1b5e9-5adf-4b83-abaf-88adc681dcdf",
      {.kind = PresetKind::kAds,
       .url = kAdblockPlusAntiAdblock,
       .settings = {.allow_abp_snippets = true},
       .removable = false}},
     {KnownRuleSourcesHandler::kPartnersListUuid,
      {.kind = PresetKind::kPartners,
       .url = kPartnersList,
       .settings = {.allow_attribution_tracker_rules = true},
       .removable = false}},
     {"f7bc721e-5cd1-440c-8036-50813c063929",
      {.kind = PresetKind::kRegional,
       .url =
           "https://raw.githubusercontent.com/heradhis/indonesianadblockrules/"
           "master/subscriptions/abpindo.txt"}},
     {"092a3a7f-b452-47e2-bbd7-b61e902ad0fd",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/abpvn/abpvn/master/filter/"
              "abpvn.txt"}},
     {"e5d554e9-8249-47c1-abf8-004cd29f4172",
      {.kind = PresetKind::kRegional,
       .url = "http://stanev.org/abp/adblock_bg.txt"}},
     {"439f5af1-9c74-4606-9b9e-b46863ac611c",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/easylistchina.txt"}},
     {"923b5982-519e-4c7f-9854-3bd354b368b8",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/cjx82630/cjxlist/master/"
              "cjx-annoyance.txt"}},
     {"5c9b517d-5182-401a-aee6-ae32414ca708",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/tomasko126/"
              "easylistczechandslovak/master/filters.txt"}},
     {"acf379b6-2c46-4802-88c9-6dd46bedfb32",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/easylistdutch.txt"}},
     {"933d897d-cb29-4282-a4f9-2451d83d1885",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist.to/easylistgermany/easylistgermany.txt"}},
     {"22263ec8-d105-418a-a187-36f5c9808dcf",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/easylist/EasyListHebrew/"
              "master/EasyListHebrew.txt"}},
     {"364fff45-270d-4a62-a449-982856057678",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/easylistitaly.txt"}},
     {"4f1dbb65-d152-46c8-81db-b5f2cd6d66d5",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/EasyList-Lithuania/"
              "easylist_lithuania/master/easylistlithuania.txt"}},
     {"ef6d3c42-e166-4901-9b03-58f124fbebf3",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/easylistpolish.txt"}},
     {"b1d9732d-c0f3-4c74-8596-e1518b42b356",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/"
              "easylistportuguese.txt"}},
     {"3eae7230-473c-4ccd-a15f-f08e4bb86f71",
      {.kind = PresetKind::kRegional,
       .url =
           "https://easylist-downloads.adblockplus.org/easylistspanish.txt"}},
     {"98ed727f-d9c0-4bc6-bded-19b14b52d167",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/indianlist.txt"}},
     {"629f497d-0660-4b7d-8c82-afaf89345681",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/koreanlist.txt"}},
     {"1810bcfd-dad7-4c42-82bb-0fc33ebe7892",
      {.kind = PresetKind::kRegional,
       .url = "https://notabug.org/latvian-list/adblock-latvian/raw/master/"
              "lists/latvian-list.txt"}},
     {"01b357a7-eddb-4dce-9c3f-4e90099bbfcd",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/Liste_AR.txt"}},
     {"9be6251e-631e-4177-abec-d5dbef6be4f7",
      {.kind = PresetKind::kRegional,
       .url = "https://easylist-downloads.adblockplus.org/liste_fr.txt"}},
     {"434d57a1-51ac-480f-a5af-cc1c127f0313",
      {.kind = PresetKind::kRegional,
       .url = "https://www.zoso.ro/pages/rolist.txt"}},
     {"a3d2a41d-6659-4465-9819-ba8317185118",
      {.kind = PresetKind::kRegional, .url = kRussianList}},
     {"d0b816af-f803-4efa-9b8b-39bd1a0d5c75",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/yecarrillo/adblock-colombia/"
              "master/adblock_co.txt"}},
     {"a93efa90-ebea-4df2-a1a4-972445bc6d0f",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/DandelionSprout/adfilt/master/"
              "NorwegianExperimentalList%20alternate%20versions/"
              "NordicFiltersABP.txt"}},
     {"9bd24163-31fe-4889-b7e3-99e5bf742150",
      {.kind = PresetKind::kRegional,
       .url = "https://adblock.gardar.net/is.abp.txt"}},
     {"9cc5cd12-945e-4948-8ae4-266a21c9165c",
      {.kind = PresetKind::kRegional,
       .url = "https://www.void.gr/kargig/void-gr-filters.txt"}},
     {"2450843a-66fb-4e8c-9c65-bdc530623690",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/k2jp/abp-japanese-filters/"
              "master/abpjf.txt"}},
     {"0979cdbb-6581-4f56-a57b-f7dc16fb47f8",
      {.kind = PresetKind::kRegional,
       .url =
           "https://cdn.rawgit.com/SlashArash/adblockfa/master/adblockfa.txt"}},
     {"288bb849-ca3b-4a6c-8c26-8f0f41e88af7",
      {.kind = PresetKind::kRegional, .url = "https://adblock.ee/list.php"}},
     {"d8d2b8a5-f918-4a5f-b03c-0ee921aec48f",
      {.kind = PresetKind::kRegional, .url = "https://gurud.ee/ab.txt"}},
     {"5ec4c886-a4b7-4fd4-9654-a7a138bf74bf",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/hufilter/hufilter/refs/heads/"
              "gh-pages/hufilter.txt"}},
     {"9c486dda-1589-4886-a40c-1be6484eb43d",
      {.kind = PresetKind::kOther,
       .url = "https://pgl.yoyo.org/adservers/"
              "serverlist.php?hostformat=adblockplus&mimetype=plaintext"}},
     {"53e46eb7-be5f-41b7-994c-d3155fc2025e",
      {.kind = PresetKind::kRegional,
       .url =
           "https://raw.githubusercontent.com/gioxx/xfiles/master/filtri.txt"}},
     {"aa16a0f8-9ecf-40c1-9062-d72c153145af",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/yous/YousList/master/"
              "youslist.txt"}},
     {"c43fb9ca-bf75-4f07-ad52-1c79cd67a454",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/finnish-easylist-addition/"
              "finnish-easylist-addition/master/Finland_adb.txt"}},
     {"88f940b8-990c-4caa-abff-bcdb0bfd9276",
      {.kind = PresetKind::kRegional,
       .url =
           "https://raw.githubusercontent.com/eEIi0A5L/adblock_filter/master/"
           "mochi_filter.txt"}},
     {"366ed9e8-aa6e-4fd2-b3ff-bdc151f48fa9",
      {.kind = PresetKind::kRegional,
       .url = "https://raw.githubusercontent.com/eEIi0A5L/adblock_filter/"
              "master/tamago_filter.txt"}},
     {"c29c4544-679b-4335-94f2-b27c7d099803",
      {.kind = PresetKind::kRegional,
       .url = "https://secure.fanboy.co.nz/fanboy-turkish.txt"}},
     // Removed Jan 2025 - Was I don't care about cookies
     // https://www.i-dont-care-about-cookies.eu/abp/
     // Not maintained anymore and expired certificate.
     {"c1e5bcb8-edf6-4a71-b61b-ca96a87f30e3",
      {.kind = PresetKind::kCookieNotices, .url = ""}},
     {"78610306-e2ab-4147-9a10-fb6072e6675e",
      {.kind = PresetKind::kCookieNotices,
       .url = "https://secure.fanboy.co.nz/fanboy-cookiemonster.txt"}},
     {"269f589f-0a17-4158-a961-ee5252120dad",
      {.kind = PresetKind::kAnnoyances,
       .url = "https://secure.fanboy.co.nz/fanboy-annoyance.txt"}}});

RuleGroup GetPresetGroup(const PresetSourceInfo& preset) {
  if (preset.kind == PresetKind::kTracking) {
    return RuleGroup::kTrackingRules;
  }

  return RuleGroup::kAdBlockingRules;
}

uint32_t GetPresetSourceId(const PresetSourceInfo& preset) {
  auto source = RuleSourceCore::FromUrl(GURL(preset.url));
  // We expect that all presets have a valid URL for this purpose.
  CHECK(source);
  return source->id();
}
}  // namespace

KnownRuleSourcesHandlerImpl::KnownRuleSourcesHandlerImpl(
    RuleService* rule_service,
    int storage_version,
    const std::string& locale,
    const std::array<std::vector<KnownRuleSource>, kRuleGroupCount>&
        known_sources,
    std::array<std::set<base::Uuid>, kRuleGroupCount> deleted_presets,
    base::RepeatingClosure schedule_save)
    : rule_service_(rule_service),
      deleted_presets_(std::move(deleted_presets)),
      schedule_save_(std::move(schedule_save)) {
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    for (const auto& source : known_sources[static_cast<size_t>(group)]) {
      known_sources_[static_cast<size_t>(group)].insert(
          {source.core.id(), source});
    }
  }

  if (storage_version < 2) {
    ResetPresetSources(RuleGroup::kAdBlockingRules);
    ResetPresetSources(RuleGroup::kTrackingRules);
  } else {
    UpdateSourcesFromPresets(RuleGroup::kAdBlockingRules, false,
                             storage_version < 4);
    UpdateSourcesFromPresets(RuleGroup::kTrackingRules, false, false);
  }

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

const std::set<base::Uuid>& KnownRuleSourcesHandlerImpl::GetDeletedPresets(
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
  if (known_source->second.preset_id.is_valid())
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

bool KnownRuleSourcesHandlerImpl::IsPresetEnabled(base::Uuid preset_id) {
  const auto preset = kPresetSources.find(preset_id.AsLowercaseString());
  if (preset == kPresetSources.end()) {
    return false;
  }

  return IsSourceEnabled(GetPresetGroup(preset->second),
                         GetPresetSourceId(preset->second));
}

std::optional<base::Uuid> KnownRuleSourcesHandlerImpl::GetPresetIdForSourceId(
    RuleGroup group,
    uint32_t source_id) {
  auto source_and_preset_id =
      source_id_to_preset_maps_[static_cast<size_t>(group)].find(source_id);
  if (source_and_preset_id ==
      source_id_to_preset_maps_[static_cast<size_t>(group)].end()) {
    return std::nullopt;
  }

  return source_and_preset_id->second;
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
  source_id_to_preset_maps_[static_cast<size_t>(group)].clear();
  // Doesn't make sense to do both at the same time.
  DCHECK(!add_deleted_presets || !store_missing_as_deleted);
  KnownRuleSources& known_sources = GetSourceMap(group);

  if (add_deleted_presets)
    deleted_presets_[static_cast<size_t>(group)].clear();

  std::map<base::Uuid, uint32_t> known_presets;

  for (const auto& known_source : known_sources) {
    if (known_source.second.preset_id.is_valid())
      known_presets[known_source.second.preset_id] = known_source.first;
  }

  for (const auto& [preset_id_string, preset] : kPresetSources) {
    base::Uuid preset_id = base::Uuid::ParseLowercase(preset_id_string);
    if (GetPresetGroup(preset) != group) {
      continue;
    }
    if (preset.url.empty()) {
      // Empty URL means forcibly remove.
      const auto known_preset = known_presets.find(preset_id);
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
      // Preset kinds are not stored. We restore their value here.
      known_source->second.preset_kind = preset.kind;
      // It wasn't added manually
      if (known_source->second.preset_id.is_valid()) {
        source_id_to_preset_maps_[static_cast<size_t>(group)]
                                 [preset_source.core.id()] = preset_id;
        // Keep the |preset_id| up to date if needed. This should only ever do
        // something if there was an issue with storage.
        known_source->second.preset_id = preset_id;

        known_presets.erase(preset_id);
      }
      // If it was added manually, but we had another source with this preset's
      // ID, it probably means we've updated a preset to a new URL but that
      // the user added that same URL in the meantime. In that case, if the old
      // preset source is still present, it will be erased below as it will
      // remain part of the leftovers in |knows_presets|.
      continue;
    }
    preset_source.preset_id = preset_id;
    preset_source.preset_kind = preset.kind;
    preset_source.core.set_settings(preset.settings);
    preset_source.removable = preset.removable;

    const auto known_preset = known_presets.find(preset_id);
    if (known_preset != known_presets.end()) {
      // If there was a source with a URL matching this preset, it would have
      // been handled above.
      CHECK(known_preset->second != preset_source.core.id());

      bool enable = IsSourceEnabled(group, known_preset->second);
      RemoveSource(group, known_preset->second);
      known_presets.erase(known_preset);
      source_id_to_preset_maps_[static_cast<size_t>(group)]
                               [preset_source.core.id()] = preset_id;
      AddSource(group, std::move(preset_source), enable);
    } else if (store_missing_as_deleted) {
      // NOTE(julien): We weren't keeping track of deleted presets before.
      // This allows us to remedy that for people who had old setups.
      // This will break addition of new presets for those people, so we
      // shouldn't add new presets too soon after this.
      deleted_presets_[static_cast<size_t>(group)].insert(preset_id);
    } else if (deleted_presets_[static_cast<size_t>(group)].count(preset_id) ==
                   0 ||
               !preset.removable) {
      source_id_to_preset_maps_[static_cast<size_t>(group)]
                               [preset_source.core.id()] = preset_id;
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
    known_sources.at(known_preset.second).preset_id = base::Uuid();
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
