// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/pref_names.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"
#include "components/saved_tab_groups/tab_group_sync_coordinator.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/tab_group_sync_service_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/test/fake_model_type_controller.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;

namespace tab_groups {
namespace {

const char kTestCacheGuid[] = "test_cache_guid";

class MockTabGroupSyncServiceObserver : public TabGroupSyncService::Observer {
 public:
  MockTabGroupSyncServiceObserver() = default;
  ~MockTabGroupSyncServiceObserver() override = default;

  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
};

class MockTabGroupSyncCoordinator : public TabGroupSyncCoordinator {
 public:
  MockTabGroupSyncCoordinator() = default;
  ~MockTabGroupSyncCoordinator() override = default;

  MOCK_METHOD(void,
              HandleOpenTabGroupRequest,
              (const base::Uuid&, std::unique_ptr<TabGroupActionContext>));
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
};

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class TabGroupSyncServiceTest : public testing::Test {
 public:
  TabGroupSyncServiceTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        fake_controller_delegate_(syncer::SAVED_TAB_GROUP),
        group_1_(test::CreateTestSavedTabGroup()),
        group_2_(test::CreateTestSavedTabGroup()),
        group_3_(test::CreateTestSavedTabGroup()),
        local_group_id_1_(test::GenerateRandomTabGroupID()),
        local_tab_id_1_(test::GenerateRandomTabID()) {}

  ~TabGroupSyncServiceTest() override = default;

  void SetUp() override {
    auto model = std::make_unique<SavedTabGroupModel>();
    model_ = model.get();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavedTabGroupSpecificsToDataMigration, false);
    pref_service_.registry()->RegisterDictionaryPref(prefs::kDeletedTabGroupIds,
                                                     base::Value::Dict());

    auto metrics_logger =
        std::make_unique<TabGroupSyncMetricsLogger>(&device_info_tracker_);

    tab_group_sync_service_ = std::make_unique<TabGroupSyncServiceImpl>(
        std::move(model),
        std::make_unique<SyncDataTypeConfiguration>(
            processor_.CreateForwardingProcessor(),
            syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
                store_.get())),
        nullptr, &pref_service_, std::move(metrics_logger));
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(processor_, TrackedCacheGuid())
        .WillByDefault(testing::Return(kTestCacheGuid));
    ON_CALL(processor_, GetControllerDelegate())
        .WillByDefault(testing::Return(fake_controller_delegate_.GetWeakPtr()));

    auto coordinator = std::make_unique<MockTabGroupSyncCoordinator>();
    coordinator_ = coordinator.get();
    tab_group_sync_service_->SetCoordinator(std::move(coordinator));

    observer_ = std::make_unique<MockTabGroupSyncServiceObserver>();
    tab_group_sync_service_->AddObserver(observer_.get());
    task_environment_.RunUntilIdle();

    InitializeTestGroups();
  }

  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &processor_;
  }

  void TearDown() override {
    tab_group_sync_service_->RemoveObserver(observer_.get());
    model_ = nullptr;
    coordinator_ = nullptr;
  }

  void InitializeTestGroups() {
    base::Uuid id_1 = base::Uuid::GenerateRandomV4();
    base::Uuid id_2 = base::Uuid::GenerateRandomV4();
    base::Uuid id_3 = base::Uuid::GenerateRandomV4();

    const std::u16string title_1 = u"Group One";
    const std::u16string title_2 = u"Another Group";
    const std::u16string title_3 = u"The Three Musketeers";

    const tab_groups::TabGroupColorId& color_1 =
        tab_groups::TabGroupColorId::kGrey;
    const tab_groups::TabGroupColorId& color_2 =
        tab_groups::TabGroupColorId::kRed;
    const tab_groups::TabGroupColorId& color_3 =
        tab_groups::TabGroupColorId::kGreen;

    SavedTabGroupTab group_1_tab_1 = test::CreateSavedTabGroupTab(
        "A_Link", u"Only Tab", id_1, /*position=*/0);
    group_1_tab_1.SetLocalTabID(local_tab_id_1_);
    std::vector<SavedTabGroupTab> group_1_tabs = {group_1_tab_1};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        test::CreateSavedTabGroupTab("One_Link", u"One Of Two", id_2,
                                     /*position=*/0),
        test::CreateSavedTabGroupTab("Two_Link", u"Second", id_2,
                                     /*position=*/1)};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        test::CreateSavedTabGroupTab("Athos", u"All For One", id_3,
                                     /*position=*/0),
        test::CreateSavedTabGroupTab("Porthos", u"And", id_3, /*position=*/1),
        test::CreateSavedTabGroupTab("Aramis", u"One For All", id_3,
                                     /*position=*/2)};

    group_1_ = SavedTabGroup(title_1, color_1, group_1_tabs, std::nullopt, id_1,
                             local_group_id_1_);
    group_2_ =
        SavedTabGroup(title_2, color_2, group_2_tabs, std::nullopt, id_2);
    group_3_ =
        SavedTabGroup(title_3, color_3, group_3_tabs, std::nullopt, id_3);

    group_1_.SetCreatorCacheGuid(kTestCacheGuid);
    group_2_.SetCreatorCacheGuid(kTestCacheGuid);
    group_3_.SetCreatorCacheGuid(kTestCacheGuid);

    model_->Add(group_1_);
    model_->Add(group_2_);
    model_->Add(group_3_);
  }

  void VerifyCacheGuids(const SavedTabGroup& group,
                        const SavedTabGroupTab* tab,
                        std::optional<std::string> group_creator_cache_guid,
                        std::optional<std::string> group_updater_cache_guid,
                        std::optional<std::string> tab_creator_cache_guid,
                        std::optional<std::string> tab_updater_cache_guid) {
    EXPECT_EQ(group_creator_cache_guid, group.creator_cache_guid());
    EXPECT_EQ(group_updater_cache_guid, group.last_updater_cache_guid());
    if (!tab) {
      return;
    }

    EXPECT_EQ(tab_creator_cache_guid, tab->creator_cache_guid());
    EXPECT_EQ(tab_updater_cache_guid, tab->last_updater_cache_guid());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  raw_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<MockTabGroupSyncServiceObserver> observer_;
  syncer::FakeDeviceInfoTracker device_info_tracker_;
  raw_ptr<MockTabGroupSyncCoordinator> coordinator_;
  std::unique_ptr<TabGroupSyncServiceImpl> tab_group_sync_service_;
  syncer::FakeModelTypeControllerDelegate fake_controller_delegate_;

  SavedTabGroup group_1_;
  SavedTabGroup group_2_;
  SavedTabGroup group_3_;
  LocalTabGroupID local_group_id_1_;
  LocalTabID local_tab_id_1_;
};

TEST_F(TabGroupSyncServiceTest, ServiceConstruction) {
  EXPECT_TRUE(tab_group_sync_service_->GetSavedTabGroupControllerDelegate());
}

TEST_F(TabGroupSyncServiceTest, GetAllGroups) {
  auto all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_EQ(all_groups.size(), 3u);
  EXPECT_EQ(all_groups[0].saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(all_groups[1].saved_guid(), group_2_.saved_guid());
  EXPECT_EQ(all_groups[2].saved_guid(), group_3_.saved_guid());

  SavedTabGroup group_4(test::CreateTestSavedTabGroupWithNoTabs());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group_4);

  EXPECT_EQ(model_->Count(), 4);
  all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_EQ(all_groups.size(), 3u);
}

TEST_F(TabGroupSyncServiceTest, GetGroup) {
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), group_1_.title());
  EXPECT_EQ(group->color(), group_1_.color());
  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_1_.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, GetDeletedGroupIdsUsingPrefs) {
  // Delete a group from sync. It should add the deleted ID to the pref.
  model_->RemovedFromSync(group_1_.saved_guid());
  task_environment_.RunUntilIdle();

  auto deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(1u, deleted_ids.size());
  EXPECT_TRUE(base::Contains(deleted_ids, local_group_id_1_));

  // Now close out the group from tab model and notify service.
  // The entry should be cleaned up from prefs.
  tab_group_sync_service_->RemoveLocalTabGroupMapping(local_group_id_1_);

  deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(0u, deleted_ids.size());
}

TEST_F(TabGroupSyncServiceTest,
       GetDeletedGroupIdsUsingPrefsWhileRemovedFromLocal) {
  // Delete a group from local. It should not add the entry to the prefs.
  model_->Remove(group_1_.saved_guid());
  task_environment_.RunUntilIdle();

  auto deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(0u, deleted_ids.size());
}

TEST_F(TabGroupSyncServiceTest, AddGroup) {
  base::HistogramTester histogram_tester;
  // Add a new group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

  tab_group_sync_service_->AddGroup(group_4);

  // Verify model internals.
  EXPECT_TRUE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->GetIndexOf(group_4.saved_guid()), 3);
  EXPECT_EQ(model_->Count(), 4);

  // Query the group via service and verify members.
  auto group = tab_group_sync_service_->GetGroup(group_4.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(group->saved_guid(), group_4.saved_guid());
  EXPECT_EQ(group->title(), group_4.title());
  EXPECT_EQ(group->color(), group_4.color());
  EXPECT_FALSE(group->created_before_syncing_tab_groups());
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, std::nullopt, std::nullopt,
                   std::nullopt);

  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_4.saved_tabs());
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.Created.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, AddGroupWhenSignedOut) {
  // Add a new group while signed out.
  ON_CALL(processor_, IsTrackingMetadata())
      .WillByDefault(testing::Return(false));

  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

  tab_group_sync_service_->AddGroup(group_4);

  // Query the group via service and verify members.
  auto group = tab_group_sync_service_->GetGroup(group_4.saved_guid());
  EXPECT_EQ(group->saved_guid(), group_4.saved_guid());
  EXPECT_TRUE(group->created_before_syncing_tab_groups());
}

TEST_F(TabGroupSyncServiceTest, RemoveGroupByLocalId) {
  base::HistogramTester histogram_tester;
  // Add a group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group_4);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group_4.saved_guid()).has_value());

  // Remove the group and verify.
  tab_group_sync_service_->RemoveGroup(tab_group_id);
  EXPECT_EQ(tab_group_sync_service_->GetGroup(group_4.saved_guid()),
            std::nullopt);

  // Verify model internals.
  EXPECT_FALSE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->Count(), 3);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.Removed.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, RemoveGroupBySyncId) {
  // Remove the group and verify.
  tab_group_sync_service_->RemoveGroup(group_1_.saved_guid());
  EXPECT_EQ(tab_group_sync_service_->GetGroup(group_1_.saved_guid()),
            std::nullopt);

  // Verify model internals.
  EXPECT_FALSE(model_->Contains(group_1_.saved_guid()));
  EXPECT_EQ(model_->Count(), 2);
}

TEST_F(TabGroupSyncServiceTest, UpdateVisualData) {
  base::HistogramTester histogram_tester;
  tab_groups::TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  tab_group_sync_service_->UpdateVisualData(local_group_id_1_, &visual_data);

  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());

  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), visual_data.title());
  EXPECT_EQ(group->color(), visual_data.color());
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt, std::nullopt);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.VisualsChanged.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, OpenTabGroup) {
  EXPECT_CALL(*coordinator_,
              HandleOpenTabGroupRequest(group_2_.saved_guid(), testing::_))
      .Times(1);
  tab_group_sync_service_->OpenTabGroup(
      group_2_.saved_guid(), std::make_unique<TabGroupActionContext>());
}

TEST_F(TabGroupSyncServiceTest, UpdateLocalTabGroupMapping) {
  LocalTabGroupID local_id_2 = test::GenerateRandomTabGroupID();
  tab_group_sync_service_->UpdateLocalTabGroupMapping(group_1_.saved_guid(),
                                                      local_id_2);

  auto retrieved_group = tab_group_sync_service_->GetGroup(local_id_2);
  EXPECT_TRUE(retrieved_group.has_value());

  EXPECT_EQ(retrieved_group->local_group_id().value(), local_id_2);
  EXPECT_EQ(retrieved_group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(retrieved_group->title(), group_1_.title());
  EXPECT_EQ(retrieved_group->color(), group_1_.color());

  test::CompareSavedTabGroupTabs(retrieved_group->saved_tabs(),
                                 group_1_.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, RemoveLocalTabGroupMapping) {
  auto retrieved_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(retrieved_group.has_value());
  tab_group_sync_service_->RemoveLocalTabGroupMapping(local_group_id_1_);

  retrieved_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_FALSE(retrieved_group.has_value());
  // TODO
}

TEST_F(TabGroupSyncServiceTest, AddTab) {
  base::HistogramTester histogram_tester;
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto local_tab_id_2 = test::GenerateRandomTabID();
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, std::nullopt, std::nullopt,
                   std::nullopt);

  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin", 1u);

  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt, std::nullopt);
}

TEST_F(TabGroupSyncServiceTest, AddUpdateRemoveTabWithUnknownGroupId) {
  base::HistogramTester histogram_tester;
  auto unknown_group_id = test::GenerateRandomTabGroupID();
  auto local_tab_id = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(unknown_group_id, local_tab_id,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(unknown_group_id);
  EXPECT_FALSE(group.has_value());

  tab_group_sync_service_->UpdateTab(unknown_group_id, local_tab_id,
                                     u"random tab title",
                                     GURL("www.google.com"), std::nullopt);

  group = tab_group_sync_service_->GetGroup(unknown_group_id);
  EXPECT_FALSE(group.has_value());

  tab_group_sync_service_->RemoveTab(unknown_group_id, local_tab_id);

  // No histograms should be recorded.
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin", 0u);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin", 0u);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin", 0u);
}

TEST_F(TabGroupSyncServiceTest, RemoveTab) {
  base::HistogramTester histogram_tester;
  // Add a new tab.
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());

  // Remove tab.
  tab_group_sync_service_->RemoveTab(local_group_id_1_, local_tab_id_2);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(1u, group->saved_tabs().size());
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt, std::nullopt);

  // Remove the last tab. The group should be removed from the model.
  tab_group_sync_service_->RemoveTab(local_group_id_1_, local_tab_id_1_);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_FALSE(group.has_value());
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin", 2u);
}

TEST_F(TabGroupSyncServiceTest, UpdateTab) {
  base::HistogramTester histogram_tester;
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto* tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(group.has_value());
  EXPECT_TRUE(tab);
  VerifyCacheGuids(*group, tab, kTestCacheGuid, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt);

  // Update tab.
  std::u16string new_title = u"tab title 2";
  GURL new_url = GURL("www.example.com");
  tab_group_sync_service_->UpdateTab(local_group_id_1_, local_tab_id_2,
                                     new_title, new_url, 2);

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());

  // Verify updated tab.
  tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(tab);
  EXPECT_EQ(new_title, tab->title());
  EXPECT_EQ(new_url, tab->url());
  VerifyCacheGuids(*group, tab, kTestCacheGuid, kTestCacheGuid, kTestCacheGuid,
                   kTestCacheGuid);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, MoveTab) {
  base::HistogramTester histogram_tester;
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto* tab = group->GetTab(local_tab_id_2);
  EXPECT_EQ(1u, tab->position());

  // Move tab from position 1 to position 0.
  tab_group_sync_service_->MoveTab(local_group_id_1_, local_tab_id_2, 0);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  tab = group->GetTab(local_tab_id_2);
  EXPECT_EQ(0u, tab->position());

  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin", 1u);

  // Call API with a invalid tab ID.
  tab_group_sync_service_->MoveTab(local_group_id_1_,
                                   test::GenerateRandomTabID(), 0);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, OnTabSelected) {
  base::HistogramTester histogram_tester;
  // Add a new tab.
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  // Select tab.
  tab_group_sync_service_->OnTabSelected(local_group_id_1_, local_tab_id_2);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabSelected.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, RecordTabGroupEvent) {
  base::HistogramTester histogram_tester;
  EventDetails event_details(TabGroupEvent::kTabGroupOpened);
  event_details.local_tab_group_id = local_group_id_1_;
  event_details.opening_source = OpeningSource::kAutoOpenedFromSync;
  tab_group_sync_service_->RecordTabGroupEvent(event_details);
  histogram_tester.ExpectTotalCount("TabGroups.Sync.TabGroup.Opened.Reason",
                                    1u);
}

TEST_F(TabGroupSyncServiceTest, UpdateLocalTabId) {
  auto tab_guid = group_1_.saved_tabs()[0].saved_tab_guid();
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->UpdateLocalTabId(local_group_id_1_, tab_guid,
                                            local_tab_id_2);
  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(1u, group->saved_tabs().size());

  // Verify updated tab.
  auto* updated_tab = group->GetTab(tab_guid);
  EXPECT_TRUE(updated_tab);
  EXPECT_EQ(local_tab_id_2, updated_tab->local_tab_id().value());
}

TEST_F(TabGroupSyncServiceTest, AddObserverBeforeInitialize) {
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  task_environment_.RunUntilIdle();
}

TEST_F(TabGroupSyncServiceTest, AddObserverAfterInitialize) {
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  task_environment_.RunUntilIdle();

  auto observer2 = std::make_unique<MockTabGroupSyncServiceObserver>();
  EXPECT_CALL(*observer2, OnInitialized()).Times(1);
  tab_group_sync_service_->AddObserver(observer2.get());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedFromRemoteSource) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->AddedFromSync(group_4);
  task_environment_.RunUntilIdle();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedFromLocalSource) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->Add(group_4);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedFromRemoteSource) {
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->UpdatedVisualDataFromSync(group_1_.saved_guid(), &visual_data);
  task_environment_.RunUntilIdle();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedFromLocalSource) {
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->UpdateVisualData(group_1_.local_group_id().value(), &visual_data);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedOnTabGroupIdMappingChange) {
  // Close a group.
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->OnGroupClosedInTabStrip(local_group_id_1_);

  // Open a group.
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_2_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->OnGroupOpenedInTabStrip(group_2_.saved_guid(),
                                  test::GenerateRandomTabGroupID());
}

TEST_F(TabGroupSyncServiceTest, TabIDMappingIsCleardOnGroupClose) {
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group->local_group_id().has_value());
  EXPECT_TRUE(group->saved_tabs()[0].local_tab_id().has_value());

  // Close a group.
  model_->OnGroupClosedInTabStrip(local_group_id_1_);

  // Verify that tab IDs are unmapped.
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_FALSE(group->local_group_id().has_value());
  EXPECT_FALSE(group->saved_tabs()[0].local_tab_id().has_value());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedNoTabs) {
  // Create a group with no tabs. Observers won't be notified.
  SavedTabGroup group_4 = test::CreateTestSavedTabGroupWithNoTabs();
  base::Uuid group_id = group_4.saved_guid();
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->AddedFromSync(group_4);
  task_environment_.RunUntilIdle();

  // Update visuals. Observers still won't be notified.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  model_->UpdatedVisualDataFromSync(group_id, &visual_data);
  task_environment_.RunUntilIdle();

  // Add a tab to the group. Observers will be notified as an Add event.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(1);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  SavedTabGroupTab tab =
      test::CreateSavedTabGroupTab("A_Link", u"Tab", group_id);
  model_->AddTabToGroupFromSync(group_id, tab);
  task_environment_.RunUntilIdle();

  // Update visuals. Observers will be notified as an Update event.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->UpdatedVisualDataFromSync(group_id, &visual_data);
  task_environment_.RunUntilIdle();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupRemovedFromRemoteSource) {
  // Removig group having local ID.
  EXPECT_CALL(*observer_,
              OnTabGroupRemoved(
                  testing::TypedEq<const LocalTabGroupID&>(local_group_id_1_),
                  Eq(TriggerSource::REMOTE)))
      .Times(1);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->RemovedFromSync(group_1_.saved_guid());
  task_environment_.RunUntilIdle();

  // Remove a group with no local ID.
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_2_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->RemovedFromSync(group_2_.saved_guid());
  task_environment_.RunUntilIdle();

  // Try removing a group that doesn't exist.
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->RemovedFromSync(group_1_.saved_guid());
  task_environment_.RunUntilIdle();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupRemovedFromLocalSource) {
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->Remove(group_1_.local_group_id().value());
}

}  // namespace tab_groups
