// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/url_and_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_pref_names.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/channel_info.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/stats.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/types.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"

namespace tab_groups {
namespace {

constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Hours(1);

std::unique_ptr<syncer::ModelTypeChangeProcessor>
CreateSavedTabGroupChangeProcessor() {
  return std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
      syncer::SAVED_TAB_GROUP,
      base::BindRepeating(&syncer::ReportUnrecoverableError,
                          chrome::GetChannel()));
}

std::unique_ptr<syncer::ModelTypeChangeProcessor>
CreateSharedTabGroupDataChangeProcessor() {
  return std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
      syncer::SHARED_TAB_GROUP_DATA,
      base::BindRepeating(&syncer::ReportUnrecoverableError,
                          chrome::GetChannel()));
}

std::unique_ptr<SyncDataTypeConfiguration>
MaybeCreateSyncConfigurationForSharedTabGroupData(
    syncer::OnceModelTypeStoreFactory store_factory) {
  if (!base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    return nullptr;
  }

  return std::make_unique<SyncDataTypeConfiguration>(
      CreateSharedTabGroupDataChangeProcessor(), std::move(store_factory));
}

}  // anonymous namespace

SavedTabGroupKeyedService::SavedTabGroupKeyedService(
    Profile* profile,
    syncer::DeviceInfoTracker* device_info_tracker)
    : profile_(profile),
      wrapper_service_(std::make_unique<TabGroupServiceWrapper>(nullptr, this)),
      listener_(wrapper_service_.get(), profile),
      sync_bridge_mediator_(
          model(),
          profile->GetPrefs(),
          std::make_unique<SyncDataTypeConfiguration>(
              CreateSavedTabGroupChangeProcessor(),
              GetStoreFactory()),
          MaybeCreateSyncConfigurationForSharedTabGroupData(GetStoreFactory())),
      metrics_logger_(
          std::make_unique<TabGroupSyncMetricsLogger>(device_info_tracker)) {
  // TODO: Don't observe depending on which service we are using in
  // `wrapper_service_`.
  model()->AddObserver(this);

  metrics_timer_.Start(
      FROM_HERE, kDelayBeforeMetricsLogged,
      base::BindRepeating(&SavedTabGroupKeyedService::RecordMetrics,
                          base::Unretained(this)));
}

SavedTabGroupKeyedService::~SavedTabGroupKeyedService() {
  model_.RemoveObserver(this);
}

// Whether the sync setting is on for saved tab groups.
bool SavedTabGroupKeyedService::AreSavedTabGroupsSynced() {
  const syncer::SyncService* const sync_service =
      SyncServiceFactory::GetForProfile(profile_);

  if (!sync_service->IsSyncFeatureEnabled()) {
    return false;
  }

  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups);
}

syncer::OnceModelTypeStoreFactory SavedTabGroupKeyedService::GetStoreFactory() {
  DCHECK(ModelTypeStoreServiceFactory::GetForProfile(profile()));
  return ModelTypeStoreServiceFactory::GetForProfile(profile())
      ->GetStoreFactory();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
SavedTabGroupKeyedService::GetSavedTabGroupControllerDelegate() {
  return sync_bridge_mediator_.GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
SavedTabGroupKeyedService::GetSharedTabGroupControllerDelegate() {
  return sync_bridge_mediator_.GetSharedTabGroupControllerDelegate();
}

void SavedTabGroupKeyedService::ConnectRestoredGroupToSaveId(
    const base::Uuid& saved_guid,
    const TabGroupId local_group_id) {
  if (model()->is_loaded()) {
    const SavedTabGroup* const group = model()->Get(saved_guid);
    // If there is no saved group with guid `saved_guid`, the group must
    // have been unsaved since this session closed.
    if (!group) {
      return;
    }

    // Avoid linking SavedTabGroups that are already open.
    if (group->local_group_id().has_value()) {
      return;
    }

    ConnectLocalTabGroup(local_group_id, saved_guid);
  } else {
    restored_groups_to_connect_on_load_.emplace_back(saved_guid,
                                                     local_group_id);
  }
}

void SavedTabGroupKeyedService::SaveRestoredGroup(
    const TabGroupId& local_group_id) {
  if (model()->is_loaded()) {
    CHECK(!model()->Contains(local_group_id))
        << "This group is somehow saved already when it shouldn't be.";
    SaveGroup(local_group_id);
  } else {
    restored_groups_to_save_on_load_.emplace_back(local_group_id);
  }
}

void SavedTabGroupKeyedService::UpdateAttributions(
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id) {
  model_.UpdateLastUpdaterCacheGuidForGroup(
      sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge(), group_id,
      tab_id);
}

std::optional<std::string> SavedTabGroupKeyedService::GetLocalCacheGuid()
    const {
  return sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge();
}

void SavedTabGroupKeyedService::OnTabAddedToGroupLocally(
    const base::Uuid& group_guid) {
  LogEvent(TabGroupEvent::kTabAdded, group_guid);
}

void SavedTabGroupKeyedService::OnTabRemovedFromGroupLocally(
    const base::Uuid& group_guid,
    const base::Uuid& tab_guid) {
  LogEvent(TabGroupEvent::kTabRemoved, group_guid, tab_guid);
}

void SavedTabGroupKeyedService::OnTabNavigatedLocally(
    const base::Uuid& group_guid,
    const base::Uuid& tab_guid) {
  LogEvent(TabGroupEvent::kTabNavigated, group_guid, tab_guid);
}

void SavedTabGroupKeyedService::OnTabsReorderedLocally(
    const base::Uuid& group_guid) {
  LogEvent(TabGroupEvent::kTabGroupTabsReordered, group_guid);
}

void SavedTabGroupKeyedService::OnTabGroupVisualsChanged(
    const base::Uuid& group_guid) {
  LogEvent(TabGroupEvent::kTabGroupVisualsChanged, group_guid);
}

std::optional<TabGroupId> SavedTabGroupKeyedService::OpenSavedTabGroupInBrowser(
    Browser* browser,
    const base::Uuid saved_group_guid,
    OpeningSource opening_source) {
  const SavedTabGroup* saved_group = model_.Get(saved_group_guid);

  // In the case where this function is called after confirmation of an
  // interstitial, the saved_group could be null, so protect against this by
  // early returning.
  if (!saved_group) {
    return std::nullopt;
  }

  // Activate the first tab in a group if it is already open.
  if (saved_group->local_group_id().has_value()) {
    SavedTabGroupUtils::FocusFirstTabOrWindowInOpenGroup(
        saved_group->local_group_id().value());
    return saved_group->local_group_id().value();
  }

  // If our tab group was not found in any tabstrip model, open the group in
  // this browser's tabstrip model.
  std::map<content::WebContents*, base::Uuid> opened_web_contents_to_uuid =
      GetWebContentsToTabGuidMappingForOpening(browser, saved_group,
                                               saved_group_guid);

  // If no tabs were opened, then there's nothing to do.
  if (opened_web_contents_to_uuid.empty()) {
    return std::nullopt;
  }

  // Take the opened tabs and move them into a TabGroup in the TabStrip. Link
  // the `tab_group_id` to `saved_group_guid` to stay up-to-date.
  TabGroupId tab_group_id = AddOpenedTabsToGroup(
      browser->tab_strip_model(), opened_web_contents_to_uuid, *saved_group);

  EventDetails event_details(TabGroupEvent::kTabGroupOpened);
  event_details.local_tab_group_id = tab_group_id;
  event_details.opening_source = std::move(opening_source);
  metrics_logger_->LogEvent(event_details, saved_group, nullptr);

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Opened"));

  return tab_group_id;
}

TabGroupId SavedTabGroupKeyedService::AddOpenedTabsToGroup(
    TabStripModel* const tab_strip_model_for_creation,
    const std::map<content::WebContents*, base::Uuid>&
        opened_web_contents_to_uuid,
    const SavedTabGroup& saved_group) {
  // Figure out which tabs we actually opened in this browser that aren't
  // already in groups.
  std::vector<int> tab_indices;
  for (int i = 0; i < tab_strip_model_for_creation->count(); ++i) {
    if (base::Contains(opened_web_contents_to_uuid,
                       tab_strip_model_for_creation->GetWebContentsAt(i)) &&
        !tab_strip_model_for_creation->GetTabGroupForTab(i).has_value()) {
      tab_indices.push_back(i);
      LogEvent(TabGroupEvent::kTabAdded, saved_group.saved_guid());
    }
  }

  // Create a new group in the tabstrip.
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  tab_strip_model_for_creation->AddToGroupForRestore(tab_indices, tab_group_id);

  // Update the saved tab group to link to the local group id.
  model_.OnGroupOpenedInTabStrip(saved_group.saved_guid(), tab_group_id);

  TabGroup* const tab_group =
      tab_strip_model_for_creation->group_model()->GetTabGroup(tab_group_id);

  // Activate the first tab in the tab group.
  std::optional<int> first_tab = tab_group->GetFirstTab();
  DCHECK(first_tab.has_value());
  tab_strip_model_for_creation->ActivateTabAt(first_tab.value());

  // Set the group's visual data after the tab strip is in its final state. This
  // ensures the tab group's bounds are correctly set. crbug/1408814.
  UpdateGroupVisualData(saved_group.saved_guid(),
                        saved_group.local_group_id().value());

  listener_.ConnectToLocalTabGroup(saved_group, opened_web_contents_to_uuid);

  return tab_group_id;
}

base::Uuid SavedTabGroupKeyedService::SaveGroup(const TabGroupId& group_id,
                                                bool is_pinned) {
  Browser* browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
  CHECK(browser);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->group_model());

  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  SavedTabGroup saved_tab_group(
      tab_group->visual_data()->title(), tab_group->visual_data()->color(), {},
      std::nullopt, std::nullopt, tab_group->id(),
      sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge(),
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/
      !sync_bridge_mediator_.IsSavedBridgeSyncing());
  if (is_pinned) {
    saved_tab_group.SetPinned(true);
  }

  // Build the SavedTabGroupTabs and add them to the SavedTabGroup.
  const gfx::Range tab_range = tab_group->ListTabs();

  std::map<content::WebContents*, base::Uuid> opened_web_contents_to_uuid;
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    CHECK(web_contents);

    SavedTabGroupTab saved_tab_group_tab =
        SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
            web_contents, saved_tab_group.saved_guid());

    opened_web_contents_to_uuid.emplace(web_contents,
                                        saved_tab_group_tab.saved_tab_guid());

    saved_tab_group.AddTabLocally(std::move(saved_tab_group_tab));
  }

  const base::Uuid saved_group_guid = saved_tab_group.saved_guid();
  model_.Add(std::move(saved_tab_group));

  // Link the local group to the saved group in the listener.
  listener_.ConnectToLocalTabGroup(*model_.Get(saved_group_guid),
                                   opened_web_contents_to_uuid);

  LogEvent(TabGroupEvent::kTabGroupCreated, saved_group_guid);
  return saved_group_guid;
}

void SavedTabGroupKeyedService::UnsaveGroup(const TabGroupId& group_id,
                                            ClosingSource closing_source) {
  // Get the guid since disconnect removes the local id.
  const SavedTabGroup* group = model_.Get(group_id);
  CHECK(group);

  EventDetails event_details(TabGroupEvent::kTabGroupRemoved);
  event_details.local_tab_group_id = group_id;
  event_details.closing_source = std::move(closing_source);
  metrics_logger_->LogEvent(event_details, group, nullptr);

  // Stop listening to the local group.
  DisconnectLocalTabGroup(group_id);

  // Unsave the group.
  model_.Remove(group->saved_guid());
}

void SavedTabGroupKeyedService::PauseTrackingLocalTabGroup(
    const TabGroupId& group_id) {
  listener_.PauseTrackingLocalTabGroup(group_id);
}

void SavedTabGroupKeyedService::ResumeTrackingLocalTabGroup(
    const base::Uuid& saved_group_guid,
    const TabGroupId& group_id) {
  listener_.ResumeTrackingLocalTabGroup(group_id);
}

void SavedTabGroupKeyedService::DisconnectLocalTabGroup(
    const TabGroupId& group_id) {
  listener_.DisconnectLocalTabGroup(group_id);

  // Stop listening to the current tab group and notify observers.
  model_.OnGroupClosedInTabStrip(group_id);
}

void SavedTabGroupKeyedService::ConnectLocalTabGroup(
    const TabGroupId& local_group_id,
    const base::Uuid& saved_guid) {
  Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);
  CHECK(browser);

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);

  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(local_group_id);
  CHECK(tab_group);

  const SavedTabGroup* const saved_group = model_.Get(saved_guid);
  CHECK(saved_group);

  const size_t tabs_in_group = tab_group->tab_count();
  const size_t tabs_in_saved_group = saved_group->saved_tabs().size();
  const bool saved_group_has_less_tabs = tabs_in_saved_group < tabs_in_group;
  const bool saved_group_has_more_tabs = tabs_in_saved_group > tabs_in_group;

  stats::RecordTabCountMismatchOnConnect(tabs_in_saved_group, tabs_in_group);
  if (saved_group_has_more_tabs) {
    AddMissingTabsToOutOfSyncLocalTabGroup(browser, tab_group, saved_group);
  } else if (saved_group_has_less_tabs) {
    RemoveExtraTabsFromOutOfSyncLocalTabGroup(tab_strip_model, tab_group,
                                              saved_group);
  }

  const gfx::Range& tab_range = tab_group->ListTabs();
  CHECK(tab_range.length() == tabs_in_saved_group);

  UpdateWebContentsToMatchSavedTabGroupTabs(tab_strip_model, saved_group,
                                            tab_range);

  model_.OnGroupOpenedInTabStrip(saved_guid, local_group_id);
  UpdateGroupVisualData(saved_guid, local_group_id);

  listener_.ConnectToLocalTabGroup(
      *model_.Get(saved_guid), GetWebContentsToTabGuidMappingForSavedGroup(
                                   tab_strip_model, saved_group, tab_range));
}

void SavedTabGroupKeyedService::SavedTabGroupModelLoaded() {
  // One time migration from Saved Tab Group V1 to V2
  // TODO(b/333742126): Remove migration code in M135.
  PrefService* pref_service = profile()->GetPrefs();
  if (IsTabGroupsSaveUIUpdateEnabled() &&
      !saved_tab_groups::prefs::IsTabGroupSavesUIUpdateMigrated(pref_service)) {
    model_.MigrateTabGroupSavesUIUpdate();
    saved_tab_groups::prefs::SetTabGroupSavesUIUpdateMigrated(pref_service);
  }

  for (const auto& [saved_guid, local_group_id] :
       restored_groups_to_connect_on_load_) {
    if (model()->is_loaded() && !model()->Contains(saved_guid)) {
      continue;
    }

    ConnectLocalTabGroup(local_group_id, saved_guid);
  }

  for (const auto& local_group_id : restored_groups_to_save_on_load_) {
    SaveGroup(local_group_id);
  }

  // Clear restored groups to connect and save now that we have processed them.
  restored_groups_to_connect_on_load_.clear();
  restored_groups_to_save_on_load_.clear();
}

void SavedTabGroupKeyedService::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  // Do nothing if `removed_group` is not open in the tabstrip.
  if (!removed_group.local_group_id().has_value()) {
    return;
  }

  // Update the local group's contents to match the saved group's.
  listener_.RemoveLocalGroupFromSync(removed_group.local_group_id().value());
}

void SavedTabGroupKeyedService::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* const saved_group = model_.Get(group_guid);
  CHECK(saved_group);

  // Do nothing if the saved group is not open in the tabstrip.
  if (!saved_group->local_group_id().has_value()) {
    return;
  }

  // Update the local group's contents to match the saved group's.
  listener_.UpdateLocalGroupFromSync(saved_group->local_group_id().value());
}

void SavedTabGroupKeyedService::AddMissingTabsToOutOfSyncLocalTabGroup(
    Browser* browser,
    const TabGroup* const tab_group,
    const SavedTabGroup* const saved_group) {
  size_t num_tabs_in_saved_group = saved_group->saved_tabs().size();
  size_t num_tabs_in_local_group = tab_group->tab_count();

  for (size_t relative_index_in_group = num_tabs_in_local_group;
       relative_index_in_group < num_tabs_in_saved_group;
       ++relative_index_in_group) {
    const GURL url_to_add =
        saved_group->saved_tabs()[relative_index_in_group].url();

    // Open the tab in the tabstrip and add it to the end of the group.
    auto* navigation_handle = SavedTabGroupUtils::OpenTabInBrowser(
        url_to_add, browser, profile_,
        WindowOpenDisposition::NEW_BACKGROUND_TAB);
    const content::WebContents* const new_tab =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;

    const int tab_index =
        browser->tab_strip_model()->GetIndexOfWebContents(new_tab);
    browser->tab_strip_model()->AddToExistingGroup({tab_index},
                                                   tab_group->id());
  }

  CHECK(size_t(tab_group->tab_count()) == num_tabs_in_saved_group);
}

void SavedTabGroupKeyedService::RemoveExtraTabsFromOutOfSyncLocalTabGroup(
    TabStripModel* tab_strip_model,
    TabGroup* const tab_group,
    const SavedTabGroup* const saved_group) {
  size_t num_tabs_in_saved_group = saved_group->saved_tabs().size();

  // Remove tabs from the end of the tab group to even out the number of tabs in
  // the local and saved group.
  while (tab_group->tab_count() > int(num_tabs_in_saved_group)) {
    const int last_tab = tab_group->GetLastTab().value();
    tab_strip_model->CloseWebContentsAt(
        last_tab, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }

  CHECK(size_t(tab_group->tab_count()) == num_tabs_in_saved_group);
}

void SavedTabGroupKeyedService::UpdateWebContentsToMatchSavedTabGroupTabs(
    const TabStripModel* const tab_strip_model,
    const SavedTabGroup* const saved_group,
    const gfx::Range& tab_range) {
  for (size_t index_in_tabstrip = tab_range.start();
       index_in_tabstrip < tab_range.end(); ++index_in_tabstrip) {
    content::WebContents* const web_contents =
        tab_strip_model->GetWebContentsAt(index_in_tabstrip);
    CHECK(web_contents);

    const int saved_tab_index = index_in_tabstrip - tab_range.start();
    const SavedTabGroupTab& saved_tab =
        saved_group->saved_tabs()[saved_tab_index];

    if (saved_tab.url() != web_contents->GetLastCommittedURL()) {
      web_contents->GetController().LoadURLWithParams(
          content::NavigationController::LoadURLParams(saved_tab.url()));
    }
  }
}

std::map<content::WebContents*, base::Uuid>
SavedTabGroupKeyedService::GetWebContentsToTabGuidMappingForSavedGroup(
    const TabStripModel* const tab_strip_model,
    const SavedTabGroup* const saved_group,
    const gfx::Range& tab_range) {
  std::map<content::WebContents*, base::Uuid> web_contents_map;

  for (size_t index_in_tabstrip = tab_range.start();
       index_in_tabstrip < tab_range.end(); ++index_in_tabstrip) {
    content::WebContents* const web_contents =
        tab_strip_model->GetWebContentsAt(index_in_tabstrip);
    CHECK(web_contents);

    const int saved_tab_index = index_in_tabstrip - tab_range.start();
    const SavedTabGroupTab& saved_tab =
        saved_group->saved_tabs()[saved_tab_index];

    web_contents_map.emplace(web_contents, saved_tab.saved_tab_guid());
  }

  return web_contents_map;
}

std::map<content::WebContents*, base::Uuid>
SavedTabGroupKeyedService::GetWebContentsToTabGuidMappingForOpening(
    Browser* browser,
    const SavedTabGroup* const saved_group,
    const base::Uuid& saved_group_guid) {
  std::map<content::WebContents*, base::Uuid> web_contents;
  for (const SavedTabGroupTab& saved_tab : saved_group->saved_tabs()) {
    if (!saved_tab.url().is_valid()) {
      continue;
    }

    auto* navigation_handle = SavedTabGroupUtils::OpenTabInBrowser(
        saved_tab.url(), browser, profile_,
        WindowOpenDisposition::NEW_BACKGROUND_TAB);
    content::WebContents* created_contents =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;

    if (!created_contents) {
      continue;
    }

    web_contents.emplace(created_contents, saved_tab.saved_tab_guid());
  }
  return web_contents;
}

const TabStripModel* SavedTabGroupKeyedService::GetTabStripModelWithTabGroupId(
    const TabGroupId& local_group_id) {
  const Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);
  CHECK(browser);
  return browser->tab_strip_model();
}

void SavedTabGroupKeyedService::UpdateGroupVisualData(
    const base::Uuid saved_group_guid,
    const TabGroupId group_id) {
  TabGroup* const tab_group = SavedTabGroupUtils::GetTabGroupWithId(group_id);
  CHECK(tab_group);
  const SavedTabGroup* const saved_group = model_.Get(saved_group_guid);
  CHECK(saved_group);

  // Update the group to use the saved title and color.
  TabGroupVisualData visual_data(saved_group->title(), saved_group->color(),
                                 /*is_collapsed=*/false);
  tab_group->SetVisualData(visual_data, /*is_customized=*/true);
}

void SavedTabGroupKeyedService::RecordMetrics() {
  stats::RecordSavedTabGroupMetrics(model());
  RecordTabGroupMetrics();
  metrics_timer_.Reset();
}

void SavedTabGroupKeyedService::RecordTabGroupMetrics() {
  int total_unsaved_groups = 0;

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (profile_ != browser->profile()) {
      continue;
    }

    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    if (!tab_strip_model->SupportsTabGroups()) {
      continue;
    }

    const TabGroupModel* group_model = tab_strip_model->group_model();
    CHECK(group_model);

    std::vector<TabGroupId> group_ids = group_model->ListTabGroups();

    for (TabGroupId group_id : group_ids) {
      if (model()->Contains(group_id)) {
        continue;
      }

      const TabGroup* const group = group_model->GetTabGroup(group_id);
      base::UmaHistogramCounts10000("TabGroups.UnsavedTabGroupTabCount",
                                    group->tab_count());
      ++total_unsaved_groups;
    }
  }

  // Record total number of non-saved tab groups in all browsers.
  base::UmaHistogramCounts10000("TabGroups.UnsavedTabGroupCount",
                                total_unsaved_groups);
}

void SavedTabGroupKeyedService::LogEvent(
    TabGroupEvent event,
    const base::Uuid& group_saved_id,
    const std::optional<base::Uuid>& tab_saved_id) {
  if (!metrics_logger_) {
    LOG(WARNING) << __func__ << " Metrics logger doesn't exist";
    return;
  }

  const auto* group = model_.Get(group_saved_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab =
      tab_saved_id.has_value() ? group->GetTab(tab_saved_id.value()) : nullptr;

  EventDetails event_details(event);
  event_details.local_tab_group_id = group->local_group_id();
  if (tab) {
    event_details.local_tab_id = tab->local_tab_id();
  }
  metrics_logger_->LogEvent(event_details, group, tab);
}

}  // namespace tab_groups
