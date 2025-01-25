// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_caching_storage.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

constexpr char kAdURL[] = "https://www.foo.com/ad1.html";
constexpr char kBiddingURL[] = "https://www.example.com/bidding_logic";
constexpr char kUpdateURL[] = "https://www.example.com/update";
constexpr char kJoiningURL[] = "https://www.test.com";
blink::InterestGroup MakeInterestGroup(url::Origin owner, std::string name) {
  blink::InterestGroup group;
  group.expiry = base::Time::Now() + base::Days(1);
  group.owner = owner;
  group.name = name;
  group.bidding_url = GURL(kBiddingURL);
  group.update_url = GURL(kUpdateURL);
  group.ads.emplace();
  group.ads->emplace_back(GURL(kAdURL), /*metadata=*/"");
  EXPECT_TRUE(group.IsValid());
  return group;
}

class InterestGroupCachingStorageTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFledgeUseInterestGroupCache},
        /*disabled_features=*/{});
  }

  std::optional<scoped_refptr<StorageInterestGroups>> GetInterestGroupsForOwner(
      InterestGroupCachingStorage* caching_storage,
      const url::Origin& owner) {
    std::optional<scoped_refptr<StorageInterestGroups>> result;
    base::RunLoop run_loop;
    caching_storage->GetInterestGroupsForOwner(
        owner,
        base::BindLambdaForTesting(
            [&result, &run_loop](scoped_refptr<StorageInterestGroups> groups) {
              result = std::move(groups);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::optional<SingleStorageInterestGroup> GetInterestGroup(
      InterestGroupCachingStorage* caching_storage,
      const blink::InterestGroupKey& group_key) {
    std::optional<SingleStorageInterestGroup> result;
    base::RunLoop run_loop;
    caching_storage->GetInterestGroup(
        group_key, base::BindLambdaForTesting(
                       [&result, &run_loop](
                           std::optional<SingleStorageInterestGroup> group) {
                         result = std::move(group);
                         run_loop.Quit();
                       }));
    run_loop.Run();
    return result;
  }

  std::vector<url::Origin> GetAllInterestGroupOwners(
      InterestGroupCachingStorage* caching_storage) {
    std::vector<url::Origin> result;
    base::RunLoop run_loop;
    caching_storage->GetAllInterestGroupOwners(base::BindLambdaForTesting(
        [&result, &run_loop](std::vector<url::Origin> res) {
          result = std::move(res);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  std::vector<InterestGroupUpdateParameter> GetInterestGroupsForUpdate(
      InterestGroupCachingStorage* caching_storage,
      const url::Origin& owner,
      int groups_limit) {
    std::vector<InterestGroupUpdateParameter> result;
    base::RunLoop run_loop;
    caching_storage->GetInterestGroupsForUpdate(
        owner, groups_limit,
        base::BindLambdaForTesting(
            [&result,
             &run_loop](std::vector<InterestGroupUpdateParameter> res) {
              result = std::move(res);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::vector<url::Origin> GetAllInterestGroupJoiningOrigins(
      InterestGroupCachingStorage* caching_storage) {
    std::vector<url::Origin> result;
    base::RunLoop run_loop;
    caching_storage->GetAllInterestGroupJoiningOrigins(
        base::BindLambdaForTesting(
            [&result, &run_loop](std::vector<url::Origin> res) {
              result = std::move(res);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::vector<std::pair<url::Origin, url::Origin>>
  GetAllInterestGroupOwnerJoinerPairs(
      InterestGroupCachingStorage* caching_storage) {
    std::vector<std::pair<url::Origin, url::Origin>> result;
    base::RunLoop run_loop;
    caching_storage->GetAllInterestGroupOwnerJoinerPairs(
        base::BindLambdaForTesting(
            [&result,
             &run_loop](std::vector<std::pair<url::Origin, url::Origin>> res) {
              result = std::move(res);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::optional<base::Time> GetLastKAnonymityReported(
      InterestGroupCachingStorage* caching_storage,
      const std::string& key) {
    std::optional<base::Time> result;
    base::RunLoop run_loop;
    caching_storage->GetLastKAnonymityReported(
        key, base::BindLambdaForTesting(
                 [&result, &run_loop](std::optional<base::Time> res) {
                   result = std::move(res);
                   run_loop.Quit();
                 }));
    run_loop.Run();
    return result;
  }

  void JoinInterestGroup(InterestGroupCachingStorage* caching_storage,
                         const blink::InterestGroup& group,
                         const GURL& main_frame_joining_url) {
    base::RunLoop run_loop;
    caching_storage->JoinInterestGroup(
        group, main_frame_joining_url,
        base::BindLambdaForTesting(
            [&run_loop](
                std::optional<InterestGroupKanonUpdateParameter> input) {
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void LeaveInterestGroup(InterestGroupCachingStorage* caching_storage,
                          const blink::InterestGroupKey& group_key,
                          const url::Origin& main_frame) {
    base::RunLoop run_loop;
    caching_storage->LeaveInterestGroup(
        group_key, main_frame,
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  void UpdateInterestGroup(InterestGroupCachingStorage* caching_storage,
                           const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update) {
    base::RunLoop run_loop;
    caching_storage->UpdateInterestGroup(
        group_key, update,
        base::BindLambdaForTesting(
            [&run_loop](
                std::optional<InterestGroupKanonUpdateParameter> input) {
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  std::unique_ptr<content::InterestGroupCachingStorage> CreateCachingStorage() {
    return std::make_unique<content::InterestGroupCachingStorage>(
        temp_directory_.GetPath(), false);
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 protected:
  base::ScopedTempDir temp_directory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InterestGroupCachingStorageTest, DBUpdatesShouldModifyCache) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  GURL joining_url(kJoiningURL);
  url::Origin joining_origin(url::Origin::Create(joining_url));
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig1 = MakeInterestGroup(owner, "name1");
  auto ig2 = MakeInterestGroup(owner, "name2");
  blink::InterestGroup ig_different_owner;
  ig_different_owner.owner = url::Origin::Create(GURL("https://www.other.com"));
  ig_different_owner.name = "other";
  ig_different_owner.expiry = base::Time::Now() + base::Days(30);

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 0u);

  // We can get an interest group after joining it.
  JoinInterestGroup(caching_storage.get(), ig1, joining_url);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);

  std::optional<scoped_refptr<StorageInterestGroups>> previously_loaded_igs =
      loaded_igs;

  // Getting an interest group a second time works.
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  // Joining a second interest group (for the same owner) works.
  JoinInterestGroup(caching_storage.get(), ig2, joining_url);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 2u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());
  previously_loaded_igs = loaded_igs;

  // Leaving an interest group updates the cached value.
  LeaveInterestGroup(caching_storage.get(),
                     blink::InterestGroupKey(owner, "name1"), owner);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());

  previously_loaded_igs = loaded_igs;

  caching_storage->ClearOriginJoinedInterestGroups(
      owner, {"name2"}, joining_origin,
      base::BindLambdaForTesting([](std::vector<std::string>) {}));
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());
  previously_loaded_igs = loaded_igs;

  InterestGroupUpdate ig_update;
  ig_update.priority = 1;
  ASSERT_NE(ig2.priority, 1);
  UpdateInterestGroup(caching_storage.get(),
                      blink::InterestGroupKey(owner, "name2"), ig_update);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_EQ(loaded_igs->get()->GetInterestGroups()[0]->interest_group.priority,
            1);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());

  previously_loaded_igs = loaded_igs;

  // ReportUpdateFailed should _not_ affect our cached values.
  caching_storage->ReportUpdateFailed(blink::InterestGroupKey(owner, "name2"),
                                      false);
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  // RecordInterestGroupBids updates the in-memory object instead of
  // invalidating the cache.
  caching_storage->RecordInterestGroupBids(
      blink::InterestGroupSet({blink::InterestGroupKey(owner, "name2")}));
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_EQ(previously_loaded_igs->get()
                ->GetInterestGroups()[0]
                ->bidding_browser_signals->bid_count,
            1);
  ASSERT_EQ(loaded_igs->get()
                ->GetInterestGroups()[0]
                ->bidding_browser_signals->bid_count,
            1);

  previously_loaded_igs = loaded_igs;

  caching_storage->RecordInterestGroupWin(
      blink::InterestGroupKey(owner, "name2"), "");
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());
  ASSERT_EQ(previously_loaded_igs->get()
                ->GetInterestGroups()[0]
                ->bidding_browser_signals->prev_wins.size(),
            0u);
  ASSERT_EQ(loaded_igs->get()
                ->GetInterestGroups()[0]
                ->bidding_browser_signals->prev_wins.size(),
            1u);

  previously_loaded_igs = loaded_igs;

  std::string k_anon_key = blink::HashedKAnonKeyForAdBid(ig1, kAdURL);
  caching_storage->UpdateKAnonymity(
      blink::InterestGroupKey(ig2.owner, ig2.name), {k_anon_key},
      base::Time::Now(),
      /*replace_existing_values*/ true);
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());
  ASSERT_EQ(previously_loaded_igs->get()
                ->GetInterestGroups()[0]
                ->hashed_kanon_keys.size(),
            0u);
  ASSERT_THAT(loaded_igs->get()->GetInterestGroups()[0]->hashed_kanon_keys,
              testing::UnorderedElementsAre(k_anon_key));

  previously_loaded_igs = loaded_igs;

  // UpdateLastKAnonymityReported does not alter any cached values but does
  // update the reported time.
  base::Time update_time = base::Time::Now();
  caching_storage->UpdateLastKAnonymityReported(k_anon_key);
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  std::optional<base::Time> loaded_time =
      GetLastKAnonymityReported(caching_storage.get(), k_anon_key);
  ASSERT_EQ(loaded_time, update_time);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  caching_storage->RemoveInterestGroupsMatchingOwnerAndJoiner(
      owner, joining_origin, base::BindLambdaForTesting([]() {}));
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 0u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());

  previously_loaded_igs = loaded_igs;

  JoinInterestGroup(caching_storage.get(), ig2, joining_url);
  JoinInterestGroup(caching_storage.get(), ig_different_owner, joining_url);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(),
                                         ig_different_owner.owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  previously_loaded_igs = loaded_igs;
  caching_storage->DeleteInterestGroupData(
      base::BindLambdaForTesting([&owner](const blink::StorageKey& candidate) {
        return candidate == blink::StorageKey::CreateFirstParty(owner);
      }),
      base::BindLambdaForTesting([]() {}));
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(),
                                         ig_different_owner.owner);
  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 0u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());

  previously_loaded_igs = loaded_igs;

  JoinInterestGroup(caching_storage.get(), ig2, joining_url);
  JoinInterestGroup(caching_storage.get(), ig_different_owner, joining_url);
  caching_storage->DeleteAllInterestGroupData(
      base::BindLambdaForTesting([]() {}));
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(),
                                         ig_different_owner.owner);
  ASSERT_EQ(loaded_igs->get()->size(), 0u);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 0u);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());

  previously_loaded_igs = loaded_igs;

  JoinInterestGroup(caching_storage.get(), ig2, joining_url);
  caching_storage->SetInterestGroupPriority(
      blink::InterestGroupKey(ig2.owner, ig2.name), 2);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());
  ASSERT_EQ(loaded_igs->get()->GetInterestGroups()[0]->interest_group.priority,
            2);

  previously_loaded_igs = loaded_igs;

  base::flat_map<std::string, auction_worklet::mojom::PrioritySignalsDoublePtr>
      update_priority_signals_overrides;
  update_priority_signals_overrides.emplace(
      "key", auction_worklet::mojom::PrioritySignalsDouble::New(0));
  caching_storage->UpdateInterestGroupPriorityOverrides(
      blink::InterestGroupKey(ig2.owner, ig2.name),
      std::move(update_priority_signals_overrides));
  task_environment().FastForwardBy(base::Minutes(1));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_NE(loaded_igs->get(), previously_loaded_igs->get());
  ASSERT_TRUE(loaded_igs->get()
                  ->GetInterestGroups()[0]
                  ->interest_group.priority_signals_overrides->contains("key"));
}

TEST_F(InterestGroupCachingStorageTest, GettersShouldNotModifyCache) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig1 = MakeInterestGroup(owner, "name1");
  auto ig2 = MakeInterestGroup(owner, "name2");

  JoinInterestGroup(caching_storage.get(), ig1, GURL("https://www.test.com"));
  JoinInterestGroup(caching_storage.get(), ig2, GURL("https://www.test.com"));

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get()->size(), 2u);
  std::optional<scoped_refptr<StorageInterestGroups>> previously_loaded_igs =
      loaded_igs;

  GetInterestGroup(caching_storage.get(),
                   blink::InterestGroupKey(ig1.owner, ig1.name));
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  GetAllInterestGroupOwners(caching_storage.get());
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  GetInterestGroupsForUpdate(caching_storage.get(), owner, 1);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  GetAllInterestGroupJoiningOrigins(caching_storage.get());
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());

  GetAllInterestGroupOwnerJoinerPairs(caching_storage.get());
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs->get(), previously_loaded_igs->get());
}

TEST_F(InterestGroupCachingStorageTest, GetInterestGroupUsesCache) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name1");

  JoinInterestGroup(caching_storage.get(), ig, GURL("https://www.test.com"));

  base::HistogramTester histogram_tester;

  // GetInterestGroupsForOwner was not called yet -- there is no cached result
  // to use.
  std::optional<SingleStorageInterestGroup> pre_cache_loaded_group =
      GetInterestGroup(caching_storage.get(),
                       blink::InterestGroupKey(ig.owner, ig.name));

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs =
      GetInterestGroupsForOwner(caching_storage.get(), owner);

  ASSERT_EQ(loaded_igs->get()->size(), 1u);
  ASSERT_NE(&(loaded_igs->get()->GetInterestGroups()[0]->interest_group),
            &(pre_cache_loaded_group.value()->interest_group));
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.GetInterestGroupCacheHit", false, 1);

  // There should be a cached value to use now.
  std::optional<SingleStorageInterestGroup> post_cache_loaded_group =
      GetInterestGroup(caching_storage.get(),
                       blink::InterestGroupKey(ig.owner, ig.name));
  ASSERT_EQ(&(loaded_igs->get()->GetInterestGroups()[0]->interest_group),
            &(post_cache_loaded_group.value()->interest_group));
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.GetInterestGroupCacheHit", true, 1);

  // After the cached value expires there should be no cached value.
  task_environment().FastForwardBy(base::Days(2));
  std::optional<SingleStorageInterestGroup> post_expiration_loaded_group =
      GetInterestGroup(caching_storage.get(),
                       blink::InterestGroupKey(ig.owner, ig.name));
  EXPECT_FALSE(post_expiration_loaded_group.has_value());
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.GetInterestGroupCacheHit", true, 2);
}

TEST_F(InterestGroupCachingStorageTest, CacheWorksWhenPointerReleased) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  GURL joining_url(kJoiningURL);
  url::Origin joining_origin(url::Origin::Create(joining_url));
  url::Origin owner1 = url::Origin::Create(GURL("https://www.example.com/"));
  url::Origin owner2 = url::Origin::Create(GURL("https://www.other.com/"));
  auto ig1 = MakeInterestGroup(owner1, "name1");
  auto ig2 = MakeInterestGroup(owner1, "name2");
  blink::InterestGroup ig_different_owner;
  ig_different_owner.owner = owner2;
  ig_different_owner.name = "other";
  ig_different_owner.expiry = base::Time::Now() + base::Days(30);

  JoinInterestGroup(caching_storage.get(), ig1, joining_url);
  JoinInterestGroup(caching_storage.get(), ig2, joining_url);
  JoinInterestGroup(caching_storage.get(), ig_different_owner, joining_url);

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs_1 =
      GetInterestGroupsForOwner(caching_storage.get(), owner1);
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs_2 =
      GetInterestGroupsForOwner(caching_storage.get(), owner2);

  // Releasing loaded_igs_1 shouldn't affect loaded_igs_2.
  loaded_igs_1.reset();
  std::optional<scoped_refptr<StorageInterestGroups>> newly_loaded_igs_1 =
      GetInterestGroupsForOwner(caching_storage.get(), owner1);
  std::optional<scoped_refptr<StorageInterestGroups>> newly_loaded_igs_2 =
      GetInterestGroupsForOwner(caching_storage.get(), owner2);

  ASSERT_EQ(loaded_igs_2->get(), newly_loaded_igs_2->get());
  ASSERT_EQ(newly_loaded_igs_1->get()->size(), 2u);
  ASSERT_EQ(newly_loaded_igs_2->get()->size(), 1u);
}

TEST_F(InterestGroupCachingStorageTest,
       CacheCollatesCallsToGetInterestGroupsByOwner) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name");
  JoinInterestGroup(caching_storage.get(), ig, GURL("https://www.test.com"));

  base::HistogramTester histogram_tester;

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs1;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs2;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs3;
  base::RunLoop run_loop;
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs1](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs1 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs2](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs2 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs3,
                  &run_loop](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs3 = std::move(groups);
                   run_loop.Quit();
                 }));
  run_loop.Run();
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", true, 2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", false, 1);
  ASSERT_EQ(loaded_igs1->get(), loaded_igs2->get());
  ASSERT_EQ(loaded_igs1->get(), loaded_igs3->get());
}

TEST_F(InterestGroupCachingStorageTest,
       CacheDoesNotCollateCallsIfInvalidatedSinceOutstandingCall) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name");
  JoinInterestGroup(caching_storage.get(), ig, GURL("https://www.test.com"));

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs1;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs2;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs3;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs4;
  base::RunLoop run_loop;
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs1](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs1 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs2](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs2 = std::move(groups);
                 }));
  caching_storage->LeaveInterestGroup(blink::InterestGroupKey(owner, "name"),
                                      owner,
                                      base::BindLambdaForTesting([]() {}));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs3](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs3 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs4,
                  &run_loop](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs4 = std::move(groups);
                   run_loop.Quit();
                 }));
  run_loop.Run();
  // The first two results should reflect the state prior to LeaveInterestGroup.
  ASSERT_EQ(loaded_igs1->get(), loaded_igs2->get());
  ASSERT_EQ(loaded_igs1->get()->size(), 1u);

  // The third result should reflect the state after LeaveInterestGroup.
  ASSERT_NE(loaded_igs1->get(), loaded_igs3->get());
  ASSERT_EQ(loaded_igs3->get()->size(), 0u);

  // The fourth result *should* be the same as the third, because those two
  // calls should be collated since the invalidation occurred prior to
  // the third load.
  ASSERT_EQ(loaded_igs3->get(), loaded_igs4->get());

  // Another call to GetInterestGroupsForOwner should get the newly cached
  // value.
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs5 =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_EQ(loaded_igs3->get(), loaded_igs5->get());
}

TEST_F(InterestGroupCachingStorageTest,
       CacheCollatesLoadsCorrectlyWithMultipleInvalidationsInARow) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name");
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs1;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs2;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs3;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs4;

  // The join and leave and join calls are three invalidations in a row.
  base::RunLoop run_loop;
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs1](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs1 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs2](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs2 = std::move(groups);
                 }));
  caching_storage->JoinInterestGroup(ig, GURL("https://www.test.com"),
                                     base::DoNothing());

  caching_storage->LeaveInterestGroup(blink::InterestGroupKey(owner, "name"),
                                      owner,
                                      base::BindLambdaForTesting([]() {}));
  caching_storage->JoinInterestGroup(ig, GURL("https://www.test.com"),
                                     base::DoNothing());

  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs3](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs3 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs4,
                  &run_loop](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs4 = std::move(groups);
                   run_loop.Quit();
                 }));
  run_loop.Run();
  // The first two results should reflect the state prior to Join/Leave/Join.
  ASSERT_EQ(loaded_igs1->get(), loaded_igs2->get());
  ASSERT_EQ(loaded_igs1->get()->size(), 0u);

  // The third result should reflect the state after Join/Leave/Join.
  ASSERT_NE(loaded_igs1->get(), loaded_igs3->get());
  ASSERT_EQ(loaded_igs3->get()->size(), 1u);

  // The fourth result *should* be the same as the third, because those two
  // calls should be collated since the invalidation occurred prior to
  // the third load.
  ASSERT_EQ(loaded_igs3->get(), loaded_igs4->get());
}

TEST_F(InterestGroupCachingStorageTest,
       CacheCollatesLoadsCorrectlyWithInvalidationsInterwovenWithLoads) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name");
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs1;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs2;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs3;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs4;
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs5;

  // The join and leave are interwoven with outstanding loads.
  base::RunLoop run_loop;
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs1](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs1 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs2](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs2 = std::move(groups);
                 }));
  caching_storage->JoinInterestGroup(ig, GURL("https://www.test.com"),
                                     base::DoNothing());

  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs3](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs3 = std::move(groups);
                 }));
  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs4](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs4 = std::move(groups);
                 }));
  caching_storage->LeaveInterestGroup(blink::InterestGroupKey(owner, "name"),
                                      owner,
                                      base::BindLambdaForTesting([]() {}));

  caching_storage->GetInterestGroupsForOwner(
      owner, base::BindLambdaForTesting(
                 [&loaded_igs5,
                  &run_loop](scoped_refptr<StorageInterestGroups> groups) {
                   loaded_igs5 = std::move(groups);
                   run_loop.Quit();
                 }));
  run_loop.Run();
  // The first two results should reflect the state prior to JoinInterestGroup.
  ASSERT_EQ(loaded_igs1->get(), loaded_igs2->get());
  ASSERT_EQ(loaded_igs1->get()->size(), 0u);

  // The third & fourth results should reflect the state after
  // JoinInterestGroup.
  ASSERT_NE(loaded_igs1->get(), loaded_igs3->get());
  ASSERT_EQ(loaded_igs3->get(), loaded_igs4->get());
  ASSERT_EQ(loaded_igs3->get()->size(), 1u);

  // The fifth result should reflect the state after LeaveInterestGroup.
  ASSERT_NE(loaded_igs4->get(), loaded_igs5->get());
  ASSERT_EQ(loaded_igs5->get()->size(), 0u);
}

TEST_F(InterestGroupCachingStorageTest, NoCachingWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFledgeUseInterestGroupCache});
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name");

  JoinInterestGroup(caching_storage.get(), ig, GURL("https://www.test.com"));

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs_again =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_NE(loaded_igs->get(), loaded_igs_again->get());

  std::optional<SingleStorageInterestGroup> single_loaded_group =
      GetInterestGroup(caching_storage.get(),
                       blink::InterestGroupKey(ig.owner, ig.name));
  ASSERT_TRUE(single_loaded_group.has_value());
  ASSERT_NE(&(loaded_igs->get()->GetInterestGroups()[0]->interest_group),
            &(single_loaded_group.value()->interest_group));

  ASSERT_TRUE(histogram_tester
                  .GetAllSamples("Ads.InterestGroup.Auction.LoadGroupsCacheHit")
                  .empty());
  ASSERT_TRUE(histogram_tester
                  .GetAllSamples("Ads.InterestGroup.GetInterestGroupCacheHit")
                  .empty());
}

TEST_F(InterestGroupCachingStorageTest, LoadGroupsCacheHitHistogram) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();
  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  auto ig = MakeInterestGroup(owner, "name");

  JoinInterestGroup(caching_storage.get(), ig, GURL("https://www.test.com"));

  base::HistogramTester histogram_tester;
  GetInterestGroupsForOwner(caching_storage.get(), owner);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", false, 1);

  task_environment_.FastForwardBy(base::Seconds(2));
  GetInterestGroupsForOwner(caching_storage.get(), owner);
  // Cache hit because the cache holds a reference to the object for
  // kMinimumCacheHoldTime.
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", true, 1);

  task_environment_.FastForwardBy(
      InterestGroupCachingStorage::kMinimumCacheHoldTime - base::Seconds(1));
  GetInterestGroupsForOwner(caching_storage.get(), owner);
  // More than kMinimumCacheHoldTime has passed since the database load but less
  // than kMinimumCacheHoldTime has passed since the last access of the groups.
  // The reference should still be in memory.
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", true, 2);

  task_environment_.FastForwardBy(
      InterestGroupCachingStorage::kMinimumCacheHoldTime);
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  // Not a cache hit because the previous result of GetInterestGroupsForOwner is
  // not in memory.
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", false, 2);

  task_environment_.FastForwardBy(
      InterestGroupCachingStorage::kMinimumCacheHoldTime);
  GetInterestGroupsForOwner(caching_storage.get(), owner);
  // Cache hit because we have a reference to the object in `loaded_igs`.
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", true, 3);

  task_environment_.FastForwardBy(
      InterestGroupCachingStorage::kMinimumCacheHoldTime);
  GetInterestGroupsForOwner(caching_storage.get(), owner);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", true, 4);

  caching_storage->RecordInterestGroupWin(
      blink::InterestGroupKey(owner, "name"), "");
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  // Not a cache hit because RecordInterestGroupWin wipes out the cached value.
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", false, 3);

  task_environment_.FastForwardBy(
      InterestGroupCachingStorage::kMinimumCacheHoldTime);
  loaded_igs = GetInterestGroupsForOwner(caching_storage.get(), owner);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.LoadGroupsCacheHit", true, 5);
}

TEST_F(InterestGroupCachingStorageTest, DontLoadCachedInterestGroupsIfExpired) {
  std::unique_ptr<content::InterestGroupCachingStorage> caching_storage =
      CreateCachingStorage();

  url::Origin owner = url::Origin::Create(GURL("https://www.example.com/"));
  // Make a group with expiration 1 day from now.
  auto ig = MakeInterestGroup(owner, "name");

  JoinInterestGroup(caching_storage.get(), ig, GURL("https://www.test.com"));

  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs =
      GetInterestGroupsForOwner(caching_storage.get(), owner);

  task_environment().FastForwardBy(base::Days(2));
  std::optional<scoped_refptr<StorageInterestGroups>> loaded_igs_again =
      GetInterestGroupsForOwner(caching_storage.get(), owner);
  ASSERT_NE(loaded_igs->get(), loaded_igs_again->get());
}

}  // namespace content
