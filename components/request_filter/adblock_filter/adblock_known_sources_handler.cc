// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_known_sources_handler.h"

#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"

namespace adblock_filter {

namespace {

const std::vector<std::string> kPermanentKnownTrackingSources = {
    "https://downloads.vivaldi.com/ddg/tds-v2-current.json",
    "https://downloads.vivaldi.com/easylist/easyprivacy-current.txt"};

const std::vector<std::string> kPermanentKnownAdBlockSources = {
    "https://downloads.vivaldi.com/lists/vivaldi/partners-current.txt",
    "https://downloads.vivaldi.com/easylist/easylist-current.txt"};

struct PresetSourceInfo {
  std::string url;
  std::string id;
};

const std::vector<PresetSourceInfo> kPresetAdBlockSources = {
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
    {"http://margevicius.lt/easylistlithuania.txt",
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
    {"https://easylist-downloads.adblockplus.org/advblock.txt",
     "a3d2a41d-6659-4465-9819-ba8317185118"},
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
    {"http://adblock.dajbych.net/adblock.txt",
     "73449266-40be-4c68-b5e8-ad68c8544e21"},
    {"https://adblock.ee/list.php", "288bb849-ca3b-4a6c-8c26-8f0f41e88af7"},
    {"https://gurud.ee/ab.txt", "d8d2b8a5-f918-4a5f-b03c-0ee921aec48f"},
    {"http://abp.mozilla-hispano.org/nauscopio/filtros.txt",
     "8e4f4bf9-5cba-40fc-b0f0-91d395c23dc7"},
    {"https://raw.githubusercontent.com/hufilter/hufilter/master/hufilter.txt",
     "5ec4c886-a4b7-4fd4-9654-a7a138bf74bf"},
    {"https://pgl.yoyo.org/adservers/"
     "serverlist.php?hostformat=adblockplus&mimetype=plaintext",
     "9c486dda-1589-4886-a40c-1be6484eb43d"},
    {"https://www.squidblacklist.org/downloads/sbl-adblock.acl",
     "acf18485-785d-4a3e-9a58-321e6ae7f392"},
    {"https://raw.githubusercontent.com/gioxx/xfiles/master/filtri.txt",
     "53e46eb7-be5f-41b7-994c-d3155fc2025e"},
    {"https://raw.githubusercontent.com/yous/YousList/master/youslist.txt",
     "aa16a0f8-9ecf-40c1-9062-d72c153145af"},
    {"https://raw.githubusercontent.com/finnish-easylist-addition/"
     "finnish-easylist-addition/master/Finland_adb.txt",
     "c43fb9ca-bf75-4f07-ad52-1c79cd67a454"}};

const std::array<const std::vector<PresetSourceInfo>*, kRuleGroupCount>
    kPresetRuleSources = {nullptr, &kPresetAdBlockSources};

}  // namespace

KnownRuleSource::KnownRuleSource(const GURL& source_url, RuleGroup group)
    : RuleSourceBase(source_url, group) {}
KnownRuleSource::KnownRuleSource(const base::FilePath& source_file,
                                 RuleGroup group)
    : RuleSourceBase(source_file, group) {}
KnownRuleSource::~KnownRuleSource() = default;
KnownRuleSource::KnownRuleSource(const KnownRuleSource&) = default;

KnownRuleSourcesHandler::Observer::~Observer() = default;

KnownRuleSourcesHandler::KnownRuleSourcesHandler(
    RuleServiceImpl* rule_service,
    int storage_version,
    const std::array<std::vector<KnownRuleSource>, kRuleGroupCount>&
        known_sources,
    std::array<std::set<std::string>, kRuleGroupCount> deleted_presets,
    base::RepeatingClosure schedule_save)
    : rule_service_(rule_service),
      deleted_presets_(std::move(deleted_presets)),
      schedule_save_(std::move(schedule_save)) {
  for (const auto& url : kPermanentKnownTrackingSources) {
    KnownRuleSource source(GURL(url), RuleGroup::kTrackingRules);
    source.removable = false;
    known_sources_[static_cast<size_t>(RuleGroup::kTrackingRules)].insert(
        {source.id, source});
  }

  for (const auto& url : kPermanentKnownAdBlockSources) {
    KnownRuleSource source(GURL(url), RuleGroup::kAdBlockingRules);
    source.removable = false;
    known_sources_[static_cast<size_t>(RuleGroup::kAdBlockingRules)].insert(
        {source.id, source});
  }

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    for (const auto& source : known_sources[static_cast<size_t>(group)]) {
      known_sources_[static_cast<size_t>(group)].insert({source.id, source});
    }
  }

  if (storage_version < 2)
    ResetPresetSources(RuleGroup::kAdBlockingRules);
  else
    UpdateSourcesFromPresets(RuleGroup::kAdBlockingRules, false,
                             storage_version < 4);

  if (rule_service->delegate() && storage_version < 5 &&
      (rule_service->delegate()->GetLocaleForDefaultLists() == "ru" ||
       rule_service->delegate()->GetLocaleForDefaultLists() == "be" ||
       rule_service->delegate()->GetLocaleForDefaultLists() == "uk")) {
    EnableSource(
        RuleGroup::kAdBlockingRules,
        RuleSource(
            GURL("https://easylist-downloads.adblockplus.org/advblock.txt"),
            RuleGroup::kAdBlockingRules)
            .id);
  }
}

KnownRuleSourcesHandler::~KnownRuleSourcesHandler() = default;

KnownRuleSources& KnownRuleSourcesHandler::GetSourceMap(RuleGroup group) {
  return known_sources_[static_cast<size_t>(group)];
}

const KnownRuleSources& KnownRuleSourcesHandler::GetSourceMap(
    RuleGroup group) const {
  return known_sources_[static_cast<size_t>(group)];
}

const KnownRuleSources& KnownRuleSourcesHandler::GetSources(
    RuleGroup group) const {
  return GetSourceMap(group);
}

const std::set<std::string>& KnownRuleSourcesHandler::GetDeletedPresets(
    RuleGroup group) const {
  return deleted_presets_[static_cast<size_t>(group)];
}

base::Optional<uint32_t> KnownRuleSourcesHandler::AddSourceFromUrl(
    RuleGroup group,
    const GURL& url) {
  if (!url.is_valid())
    return base::nullopt;

  return AddSource(KnownRuleSource(url, group), true);
}

base::Optional<uint32_t> KnownRuleSourcesHandler::AddSourceFromFile(
    RuleGroup group,
    const base::FilePath& file) {
  if (file.empty() || !file.IsAbsolute() || file.ReferencesParent() ||
      file.EndsWithSeparator())
    return base::nullopt;

  return AddSource(KnownRuleSource(file, group), true);
}

base::Optional<uint32_t> KnownRuleSourcesHandler::AddSource(
    const KnownRuleSource& known_source,
    bool enable) {
  auto& known_sources = GetSourceMap(known_source.group);

  // since the id is just a hash of the file path, if a source with the same id
  // exist, we have a source with the exact same path already.
  if (known_sources.find(known_source.id) != known_sources.end())
    return base::nullopt;

  known_sources.insert({known_source.id, known_source});
  schedule_save_.Run();

  for (Observer& observer : observers_)
    observer.OnKnownSourceAdded(known_source);

  if (enable)
    EnableSource(known_source.group, known_source.id);

  return known_source.id;
}

base::Optional<KnownRuleSource> KnownRuleSourcesHandler::GetSource(
    RuleGroup group,
    uint32_t source_id) {
  auto& known_sources = GetSourceMap(group);
  const auto& known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return base::nullopt;
  return known_source->second;
}

bool KnownRuleSourcesHandler::RemoveSource(RuleGroup group,
                                           uint32_t source_id) {
  auto& known_sources = GetSourceMap(group);
  const auto& known_source = known_sources.find(source_id);
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

bool KnownRuleSourcesHandler::EnableSource(RuleGroup group,
                                           uint32_t source_id) {
  auto& known_sources = GetSourceMap(group);
  const auto& known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return false;

  if (IsSourceEnabled(group, source_id))
    return true;

  base::Optional<uint32_t> result;
  if (known_source->second.is_from_url) {
    result =
        rule_service_->AddRulesFromURL(group, known_source->second.source_url);
  } else {
    result = rule_service_->AddRulesFromFile(group,
                                             known_source->second.source_file);
  }

  DCHECK(result && result.value() == source_id);

  for (Observer& observer : observers_)
    observer.OnKnownSourceEnabled(group, source_id);

  return result.has_value();
}

void KnownRuleSourcesHandler::DisableSource(RuleGroup group,
                                            uint32_t source_id) {
  auto& known_sources = GetSourceMap(group);
  const auto& known_source = known_sources.find(source_id);
  if (known_source == known_sources.end())
    return;

  rule_service_->DeleteRuleSource(group, source_id);

  for (Observer& observer : observers_)
    observer.OnKnownSourceDisabled(group, source_id);
}

bool KnownRuleSourcesHandler::IsSourceEnabled(RuleGroup group,
                                              uint32_t source_id) {
  return rule_service_->GetRuleSource(group, source_id).has_value();
}

void KnownRuleSourcesHandler::ResetPresetSources(RuleGroup group) {
  UpdateSourcesFromPresets(group, true, false);
}

void KnownRuleSourcesHandler::UpdateSourcesFromPresets(
    RuleGroup group,
    bool add_deleted_presets,
    bool store_missing_as_deleted) {
  // Doesn't make sense to do both at the same time.
  DCHECK(!add_deleted_presets || !store_missing_as_deleted);
  auto& known_sources = GetSourceMap(group);

  if (add_deleted_presets)
    deleted_presets_[static_cast<size_t>(group)].clear();

  std::map<std::string, uint32_t> known_presets;

  if (!kPresetRuleSources[static_cast<size_t>(group)])
    return;

  for (const auto& known_source : known_sources) {
    if (!known_source.second.preset_id.empty())
      known_presets[known_source.second.preset_id] = known_source.first;
  }

  for (const auto& preset : *kPresetRuleSources[static_cast<size_t>(group)]) {
    KnownRuleSource preset_source(GURL(preset.url), group);

    auto known_source = known_sources.find(preset_source.id);
    // We already have a rule source with that URL
    if (known_source != known_sources.end()) {
      // It wasn't added manually
      if (!known_source->second.preset_id.empty()) {
        // Keep the |preset_id| up to date if needed. This should only ever do
        // something if there was an issue with storage.
        known_source->second.preset_id = preset.id;

        known_presets.erase(preset.id);
      }
      // If it was added manually, but we had another source with this preset's
      // ID, it probably means we've updated a preset to a new URL but that
      // the user added that same URL in the meantime. In that case, if the old
      // preset source is still present, it will be erased below as it will
      // remain part of the leftovers in |knows_presets|.
      continue;
    }
    preset_source.preset_id = preset.id;

    const auto& known_preset = known_presets.find(preset.id);
    if (known_preset != known_presets.end()) {
      // If there was a source with a URL matching this preset, it would have
      // been handled above.
      DCHECK(known_preset->second != preset_source.id);

      bool enable = IsSourceEnabled(group, known_preset->second);
      RemoveSource(group, known_preset->second);
      known_presets.erase(known_preset);
      AddSource(preset_source, enable);
    } else if (store_missing_as_deleted) {
      // NOTE(julien): We weren't keeping track of deleted presets before
      // this allows us to remedy that for people who had old setups.
      // This will break addition of new presets for those people, so we
      // shouldn't add new presets too soon after this.
      deleted_presets_[static_cast<size_t>(group)].insert(preset.id);
    } else if (deleted_presets_[static_cast<size_t>(group)].count(preset.id) ==
               0) {
      AddSource(preset_source, false);
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

void KnownRuleSourcesHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void KnownRuleSourcesHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace adblock_filter