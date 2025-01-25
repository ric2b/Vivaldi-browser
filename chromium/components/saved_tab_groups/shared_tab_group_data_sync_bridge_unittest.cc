// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

// PrintTo must be defined in the same namespace as the SavedTabGroupTab class.
void PrintTo(const SavedTabGroupTab& tab, std::ostream* os) {
  *os << "(title: " << tab.title() << ", url: " << tab.url() << ")";
}
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;

MATCHER_P3(HasSharedGroupMetadata, title, color, collaboration_id, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         arg.collaboration_id() == collaboration_id;
}

MATCHER_P2(HasTabMetadata, title, url, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.url() == GURL(url);
}

MATCHER_P3(HasGroupEntityData, title, color, collaboration_id, "") {
  const sync_pb::SharedTabGroup& arg_tab_group =
      arg.specifics.shared_tab_group_data().tab_group();
  return arg_tab_group.title() == title && arg_tab_group.color() == color &&
         arg.collaboration_id == collaboration_id;
}

MATCHER_P3(HasTabEntityData, title, url, collaboration_id, "") {
  const sync_pb::SharedTab& arg_tab =
      arg.specifics.shared_tab_group_data().tab();
  return arg_tab.title() == title && arg_tab.url() == url &&
         arg.collaboration_id == collaboration_id;
}

class MockTabGroupModelObserver : public SavedTabGroupModelObserver {
 public:
  MockTabGroupModelObserver() = default;

  void ObserveModel(SavedTabGroupModel* model) { observation_.Observe(model); }
  void Reset() { observation_.Reset(); }

  MOCK_METHOD(void, SavedTabGroupRemovedFromSync, (const SavedTabGroup&));
  MOCK_METHOD(void,
              SavedTabGroupUpdatedFromSync,
              (const base::Uuid&, const std::optional<base::Uuid>&));

 private:
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

// Forwards SavedTabGroupModel's observer notifications to the bridge.
class ModelObserverForwarder : public SavedTabGroupModelObserver {
 public:
  ModelObserverForwarder(SavedTabGroupModel& model,
                         SharedTabGroupDataSyncBridge& bridge)
      : model_(model), bridge_(bridge) {
    observation_.Observe(&model);
  }

  ~ModelObserverForwarder() override = default;

  // SavedTabGroupModelObserver overrides.
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override {
    bridge_->SavedTabGroupAddedLocally(guid);
  }

  void SavedTabGroupRemovedLocally(
      const SavedTabGroup& removed_group) override {
    bridge_->SavedTabGroupRemovedLocally(removed_group);
  }

  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override {
    bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  }

 private:
  raw_ref<SavedTabGroupModel> model_;
  raw_ref<SharedTabGroupDataSyncBridge> bridge_;

  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

sync_pb::SharedTabGroupDataSpecifics MakeTabGroupSpecifics(
    const std::string& title,
    sync_pb::SharedTabGroup::Color color) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  sync_pb::SharedTabGroup* tab_group = specifics.mutable_tab_group();
  tab_group->set_title(title);
  tab_group->set_color(color);
  return specifics;
}

sync_pb::SharedTabGroupDataSpecifics MakeTabSpecifics(
    const std::string& title,
    const GURL& url,
    const base::Uuid& group_id) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  sync_pb::SharedTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_url(url.spec());
  pb_tab->set_title(title);
  pb_tab->set_shared_tab_group_guid(group_id.AsLowercaseString());
  return specifics;
}

syncer::EntityData CreateEntityData(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_data() = specifics;
  entity_data.collaboration_id = collaboration_id;
  entity_data.name = specifics.guid();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateAdd(
      storage_key, CreateEntityData(specifics, collaboration_id));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateUpdate(
      storage_key, CreateEntityData(specifics, collaboration_id));
}

std::vector<syncer::EntityData> ExtractEntityDataFromBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<syncer::EntityData> result;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    result.push_back(std::move(*data_pair.second));
  }
  return result;
}

sync_pb::EntityMetadata CreateMetadata(std::string collaboration_id) {
  sync_pb::EntityMetadata metadata;
  // Other metadata fields are not used in these tests.
  metadata.mutable_collaboration()->set_collaboration_id(
      std::move(collaboration_id));
  return metadata;
}

class SharedTabGroupDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupDataSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  // Creates the bridges and initializes the model. Returns true when succeeds.
  bool InitializeBridgeAndModel() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));

    ResetBridgeAndModel();
    saved_tab_group_model_ = std::make_unique<SavedTabGroupModel>();
    mock_model_observer_.ObserveModel(saved_tab_group_model_.get());

    bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        saved_tab_group_model_.get(),
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), &pref_service_,
        base::BindOnce(&SavedTabGroupModel::LoadStoredEntries,
                       base::Unretained(saved_tab_group_model_.get())));
    observer_forwarder_ = std::make_unique<ModelObserverForwarder>(
        *saved_tab_group_model_, *bridge_);
    task_environment_.RunUntilIdle();

    return saved_tab_group_model_->is_loaded();
  }

  // Cleans up the bridge and the model, used to simulate browser restart.
  void ResetBridgeAndModel() {
    observer_forwarder_.reset();
    mock_model_observer_.Reset();
    bridge_.reset();
    saved_tab_group_model_.reset();
  }

  size_t GetNumEntriesInStore() {
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries;
    base::RunLoop run_loop;
    store_->ReadAllData(base::BindLambdaForTesting(
        [&run_loop, &entries](
            const std::optional<syncer::ModelError>& error,
            std::unique_ptr<syncer::ModelTypeStore::RecordList> data) {
          entries = std::move(data);
          run_loop.Quit();
        }));
    run_loop.Run();
    return entries->size();
  }

  SharedTabGroupDataSyncBridge* bridge() { return bridge_.get(); }
  testing::NiceMock<syncer::MockModelTypeChangeProcessor>& mock_processor() {
    return processor_;
  }
  SavedTabGroupModel* model() { return saved_tab_group_model_.get(); }
  testing::NiceMock<MockTabGroupModelObserver>& mock_model_observer() {
    return mock_model_observer_;
  }
  syncer::ModelTypeStore& store() { return *store_; }

 private:
  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  testing::NiceMock<MockTabGroupModelObserver> mock_model_observer_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SharedTabGroupDataSyncBridge> bridge_;
  std::unique_ptr<ModelObserverForwarder> observer_forwarder_;
};

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnClientTag) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()
                   ->GetClientTag(CreateEntityData(
                       MakeTabGroupSpecifics("test title",
                                             sync_pb::SharedTabGroup::GREEN),
                       "collaboration"))
                   .empty());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCallModelReadyToSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync).WillOnce(Invoke([]() {}));

  // This already invokes RunUntilIdle, so the call above is expected to happen.
  ASSERT_TRUE(InitializeBridgeAndModel());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteGroupsAtInitialSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteTabsAtInitialSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id = "collaboration";
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"),
                       /*group_id=*/group_id),
      collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id),
      collaboration_id));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kBlue, "collaboration")));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteGroupsAtIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteTabsAtIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id = "collaboration";
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"),
                       /*group_id=*/group_id),
      collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id),
      collaboration_id));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kBlue, "collaboration")));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldUpdateExistingGroup) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id1 = "collaboration";
  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id1));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);
  change_list.clear();

  group_specifics.mutable_tab_group()->set_title("updated title");
  group_specifics.mutable_tab_group()->set_color(sync_pb::SharedTabGroup::CYAN);
  change_list.push_back(
      CreateUpdateEntityChange(group_specifics, collaboration_id1));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("updated title",
                                 tab_groups::TabGroupColorId::kCyan,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldUpdateExistingTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id = "collaboration";
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  sync_pb::SharedTabGroupDataSpecifics tab_to_update_specifics =
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"),
                       /*group_id=*/group_id);

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id));
  change_list.push_back(
      CreateAddEntityChange(tab_to_update_specifics, collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id),
      collaboration_id));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(2));
  change_list.clear();

  tab_to_update_specifics.mutable_tab()->set_title("updated title");
  change_list.push_back(
      CreateUpdateEntityChange(tab_to_update_specifics, collaboration_id));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  ASSERT_EQ(model()->Count(), 1);
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("updated title", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldDeleteExistingGroup) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group_to_delete(u"title", tab_groups::TabGroupColorId::kBlue,
                                /*urls=*/{}, /*position=*/std::nullopt);
  group_to_delete.SetCollaborationId("collaboration");
  group_to_delete.AddTabLocally(SavedTabGroupTab(
      GURL("https://website.com"), u"Website Title",
      group_to_delete.saved_guid(), /*position=*/std::nullopt));
  model()->Add(group_to_delete);
  model()->Add(SavedTabGroup(u"title 2", tab_groups::TabGroupColorId::kGrey,
                             /*urls=*/{}, /*position=*/std::nullopt)
                   .SetCollaborationId("collaboration 2"));
  ASSERT_EQ(model()->Count(), 2);

  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateDelete(
      group_to_delete.saved_guid().AsLowercaseString()));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title 2", tab_groups::TabGroupColorId::kGrey, "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldDeleteExistingTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"group title", tab_groups::TabGroupColorId::kBlue,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab_to_delete(GURL("https://google.com/1"), u"title 1",
                                 group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_to_delete);
  group.AddTabLocally(SavedTabGroupTab(GURL("https://google.com/2"), u"title 2",
                                       group.saved_guid(),
                                       /*position=*/std::nullopt));
  model()->Add(group);
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(2));

  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateDelete(
      tab_to_delete.saved_tab_guid().AsLowercaseString()));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  ASSERT_EQ(model()->Count(), 1);
  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCheckValidEntities) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  EXPECT_TRUE(bridge()->IsEntityDataValid(CreateEntityData(
      MakeTabGroupSpecifics("test title", sync_pb::SharedTabGroup::GREEN),
      "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldRemoveLocalGroupsOnDisableSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Initialize the model with some initial data. Create 2 entities to make it
  // sure that each of them is being deleted.
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::GREEN),
      "collaboration"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);
  ASSERT_EQ(GetNumEntriesInStore(), 2u);
  change_list.clear();

  // Stop sync and verify that data is removed from the model.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(model()->Count(), 0);
  EXPECT_EQ(GetNumEntriesInStore(), 0u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldNotifyObserversOnDisableSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Observers must be notified for closed groups and tabs to make it sure that
  // both will be closed.
  EXPECT_CALL(mock_model_observer(), SavedTabGroupRemovedFromSync);
  EXPECT_CALL(mock_model_observer(),
              SavedTabGroupUpdatedFromSync(Eq(group.saved_guid()),
                                           Eq(tab1.saved_tab_guid())));
  // TODO(crbug.com/319521964): uncomment the following line once fixed.
  // EXPECT_CALL(mock_model_observer(),
  // SavedTabGroupUpdatedFromSync(Eq(group.saved_guid()),
  // Eq(tab2.saved_tab_guid())));
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnGroupDataForCommit) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  std::vector<syncer::EntityData> entity_data_list = ExtractEntityDataFromBatch(
      bridge()->GetDataForCommit({group.saved_guid().AsLowercaseString()}));

  EXPECT_THAT(entity_data_list, UnorderedElementsAre(HasGroupEntityData(
                                    "title", sync_pb::SharedTabGroup_Color_GREY,
                                    "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnTabDataForCommit) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  std::vector<syncer::EntityData> entity_data_list = ExtractEntityDataFromBatch(
      bridge()->GetDataForCommit({tab2.saved_tab_guid().AsLowercaseString(),
                                  tab1.saved_tab_guid().AsLowercaseString()}));

  EXPECT_THAT(
      entity_data_list,
      UnorderedElementsAre(
          HasTabEntityData("tab 2", "http://google.com/2", "collaboration"),
          HasTabEntityData("tab 1", "http://google.com/1", "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnAllDataForDebugging) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  std::vector<syncer::EntityData> entity_data_list =
      ExtractEntityDataFromBatch(bridge()->GetAllDataForDebugging());

  EXPECT_THAT(
      entity_data_list,
      UnorderedElementsAre(
          HasTabEntityData("tab 2", "http://google.com/2", "collaboration"),
          HasTabEntityData("tab 1", "http://google.com/1", "collaboration"),
          HasGroupEntityData("title", sync_pb::SharedTabGroup_Color_GREY,
                             "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncNewGroupWithTabs) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);

  std::vector<syncer::EntityData> entity_data_list;
  EXPECT_CALL(mock_processor(), Put)
      .Times(3)
      .WillRepeatedly(WithArg<1>(Invoke(
          [&entity_data_list](std::unique_ptr<syncer::EntityData> entity_data) {
            entity_data_list.push_back(std::move(*entity_data));
          })));
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  EXPECT_THAT(
      entity_data_list,
      UnorderedElementsAre(
          HasTabEntityData("tab 2", "http://google.com/2", "collaboration"),
          HasTabEntityData("tab 1", "http://google.com/1", "collaboration"),
          HasGroupEntityData("title", sync_pb::SharedTabGroup_Color_GREY,
                             "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncUpdatedGroupMetadata) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt,
                      /*saved_guid=*/base::Uuid::GenerateRandomV4(),
                      test::GenerateRandomTabGroupID());
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(WithArg<1>(
          Invoke([&captured_entity_data](
                     std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          })));
  tab_groups::TabGroupVisualData visual_data(
      u"new title", tab_groups::TabGroupColorId::kYellow);
  model()->UpdateVisualData(group.local_group_id().value(), &visual_data);

  EXPECT_THAT(
      captured_entity_data,
      HasGroupEntityData("new title", sync_pb::SharedTabGroup_Color_YELLOW,
                         "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncNewLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);

  group.AddTabLocally(tab);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 1u);

  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"new tab", group.saved_guid(), /*position=*/1);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(WithArg<1>(
          Invoke([&captured_entity_data](
                     std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          })));
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);

  EXPECT_THAT(
      captured_entity_data,
      HasTabEntityData("new tab", "http://google.com/2", "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncRemovedLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_to_remove =
      test::CreateSavedTabGroupTab("http://google.com/2", u"tab to remove",
                                   group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab_to_remove);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  EXPECT_CALL(mock_processor(),
              Delete(tab_to_remove.saved_tab_guid().AsLowercaseString(), _, _));
  model()->RemoveTabFromGroupLocally(group.saved_guid(),
                                     tab_to_remove.saved_tab_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncUpdatedLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_to_update = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab_to_update);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(WithArg<1>(
          Invoke([&captured_entity_data](
                     std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          })));
  tab_to_update.SetURL(GURL("http://google.com/updated"));
  tab_to_update.SetTitle(u"updated tab");
  model()->UpdateTabInGroup(group.saved_guid(), tab_to_update);

  EXPECT_THAT(captured_entity_data,
              HasTabEntityData("updated tab", "http://google.com/updated",
                               "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncRemovedLocalGroup) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Only the group is removed, its tabs remain orphaned.
  EXPECT_CALL(mock_processor(),
              Delete(group.saved_guid().AsLowercaseString(), _, _));
  EXPECT_CALL(mock_processor(),
              Delete(tab1.saved_tab_guid().AsLowercaseString(), _, _))
      .Times(0);
  EXPECT_CALL(mock_processor(),
              Delete(tab2.saved_tab_guid().AsLowercaseString(), _, _))
      .Times(0);
  model()->Remove(group.saved_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReloadDataOnBrowserRestart) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  const std::string collaboration_id = "collaboration";

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(collaboration_id);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Simulate sync metadata which is normally created by the change processor.
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store().CreateWriteBatch();
  syncer::MetadataChangeList* metadata_change_list =
      write_batch->GetMetadataChangeList();
  metadata_change_list->UpdateMetadata(group.saved_guid().AsLowercaseString(),
                                       CreateMetadata(collaboration_id));
  metadata_change_list->UpdateMetadata(
      tab1.saved_tab_guid().AsLowercaseString(),
      CreateMetadata(collaboration_id));
  metadata_change_list->UpdateMetadata(
      tab2.saved_tab_guid().AsLowercaseString(),
      CreateMetadata(collaboration_id));
  store().CommitWriteBatch(std::move(write_batch), base::DoNothing());

  // Verify that the model is destroyed to simulate browser restart.
  ResetBridgeAndModel();
  ASSERT_EQ(model(), nullptr);

  // Note that sync metadata is not checked explicitly because the collaboration
  // ID is stored as a part of sync metadata.
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kGrey, "collaboration")));
  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}

}  // namespace
}  // namespace tab_groups
