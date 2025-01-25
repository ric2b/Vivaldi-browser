// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"
#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#else
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {
namespace {

using testing::SizeIs;
using testing::UnorderedElementsAre;

sync_pb::SharedTabGroupDataSpecifics MakeSharedTabGroupSpecifics(
    const base::Uuid& guid,
    const std::string& title,
    sync_pb::SharedTabGroup::Color color) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SharedTabGroup* pb_group = specifics.mutable_tab_group();
  pb_group->set_title(title);
  pb_group->set_color(color);
  return specifics;
}

sync_pb::SharedTabGroupDataSpecifics MakeSharedTabGroupTabSpecifics(
    const base::Uuid& guid,
    const base::Uuid& group_guid,
    const std::string& title,
    const GURL& url) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SharedTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_title(title);
  pb_tab->set_shared_tab_group_guid(group_guid.AsLowercaseString());
  pb_tab->set_url(url.spec());
  return specifics;
}

class SingleClientSharedTabGroupDataSyncTest : public SyncTest {
 public:
  SingleClientSharedTabGroupDataSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_overrides_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }
  ~SingleClientSharedTabGroupDataSyncTest() override = default;

  void AddSpecificsToFakeServer(
      sync_pb::SharedTabGroupDataSpecifics shared_specifics,
      const std::string& collaboration_id) {
    // First, create the collaboration for the user.
    GetFakeServer()->AddCollaboration(collaboration_id);

    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_shared_tab_group_data() =
        std::move(shared_specifics);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::
            CreateFromSharedSpecificsForTesting(
                /*non_unique_name=*/"",
                /*client_tag=*/entity_specifics.shared_tab_group_data().guid(),
                entity_specifics, /*creation_time=*/0, /*last_modified_time=*/0,
                collaboration_id));
  }

// TabGroupSyncService is used on Android only.
#if BUILDFLAG(IS_ANDROID)
  TabGroupSyncService* GetTabGroupSyncService() const {
    return TabGroupSyncServiceFactory::GetForProfile(GetProfile(0));
  }
#else
  SavedTabGroupModel* GetSavedTabGroupModel() const {
    return SavedTabGroupServiceFactory::GetForProfile(GetProfile(0))->model();
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Returns both saved and shared tab groups.
  std::vector<SavedTabGroup> GetAllTabGroups() const {
#if BUILDFLAG(IS_ANDROID)
    return GetTabGroupSyncService()->GetAllGroups();
#else
    return GetSavedTabGroupModel()->saved_tab_groups();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void AddTabGroup(SavedTabGroup group) {
#if BUILDFLAG(IS_ANDROID)
    GetTabGroupSyncService()->AddGroup(std::move(group));
#else
    GetSavedTabGroupModel()->Add(std::move(group));
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                          std::string_view collaboration_id) {
#if BUILDFLAG(IS_ANDROID)
    GetTabGroupSyncService()->MakeTabGroupShared(local_group_id,
                                                 collaboration_id);
#else
    GetSavedTabGroupModel()->MakeTabGroupShared(local_group_id,
                                                std::string(collaboration_id));
#endif  // BUILDFLAG(IS_ANDROID)
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
};

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldInitializeDataType) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::SHARED_TAB_GROUP_DATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldDownloadGroupsAndTabsAtInitialSync) {
  const base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  const std::string collaboration_id = "collaboration";

  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(group_guid, "title",
                                  sync_pb::SharedTabGroup_Color_CYAN),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 1", GURL("http://google.com/1")),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 2", GURL("http://google.com/2")),
      collaboration_id);

  ASSERT_TRUE(SetupSync());

  ASSERT_THAT(GetAllTabGroups(),
              UnorderedElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kCyan, collaboration_id)));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldTransitionSavedToSharedTabGroup) {
  ASSERT_TRUE(SetupSync());

  SavedTabGroup group(u"title", TabGroupColorId::kBlue,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  SavedTabGroupTab tab(GURL("https://google.com/1"), u"title 1",
                       group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab);
  AddTabGroup(group);

  ASSERT_TRUE(
      ServerSavedTabGroupMatchChecker(
          UnorderedElementsAre(
              HasSpecificsSavedTabGroup(
                  "title", sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE),
              HasSpecificsSavedTab("title 1", "https://google.com/1")))
          .Wait());

  // Add the user to the collaboration before making any changes (to prevent
  // filtration of local entities on GetUpdates before Commit).
  GetFakeServer()->AddCollaboration("collaboration");

  // Transition the saved tab group to shared tab group.
  MakeTabGroupShared(group.local_group_id().value(), "collaboration");

  // Only the tab group header is removed for saved tab groups.
  EXPECT_TRUE(
      ServerSavedTabGroupMatchChecker(UnorderedElementsAre(HasSpecificsSavedTab(
                                          "title 1", "https://google.com/1")))
          .Wait());

  EXPECT_TRUE(ServerSharedTabGroupMatchChecker(
                  UnorderedElementsAre(
                      HasSpecificsSharedTabGroup("title",
                                                 sync_pb::SharedTabGroup::BLUE),
                      HasSpecificsSharedTab("title 1", "https://google.com/1")))
                  .Wait());
}

#if !BUILDFLAG(IS_ANDROID)
// Android does not support PRE_ tests.
IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       PRE_ShouldReloadDataOnBrowserRestart) {
  const base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  const std::string collaboration_id = "collaboration";

  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(group_guid, "title",
                                  sync_pb::SharedTabGroup_Color_CYAN),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 1", GURL("http://google.com/1")),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 2", GURL("http://google.com/2")),
      collaboration_id);

  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldReloadDataOnBrowserRestart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace tab_groups
