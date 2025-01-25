// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_page_action_controller.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_cluster_manager.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace commerce {

namespace {
const char kTestUrl1[] = "https://www.example.com";
const char kTestUrl2[] = "https://www.foo.com";
const int64_t kClusterId = 12345L;
const char kSetTitle[] = "SetTitle";

// Build a basic ProductInfo object.
std::optional<ProductInfo> CreateProductInfo(uint64_t cluster_id) {
  std::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  return info;
}

std::optional<ProductGroup> CreateProductGroup() {
  return ProductGroup(base::Uuid::GenerateRandomV4(), kSetTitle,
                      {GURL(kTestUrl2)}, base::Time());
}
}  // namespace

class ProductSpecificationsPageActionControllerUnittest : public testing::Test {
 public:
  ProductSpecificationsPageActionControllerUnittest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
    shopping_service_ = std::make_unique<MockShoppingService>();
    base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
    account_checker_ = std::make_unique<MockAccountChecker>();
    shopping_service_->SetAccountChecker(account_checker_.get());
    mock_cluster_manager_ = static_cast<commerce::MockClusterManager*>(
        shopping_service_->GetClusterManager());
    mock_product_specifications_service_ =
        static_cast<commerce::MockProductSpecificationsService*>(
            shopping_service_->GetProductSpecificationsService());
    controller_ = std::make_unique<ProductSpecificationsPageActionController>(
        std::move(callback), shopping_service_.get());
  }

  ProductSpecificationsPageActionControllerUnittest(
      const ProductSpecificationsPageActionControllerUnittest&) = delete;
  ProductSpecificationsPageActionControllerUnittest operator=(
      const ProductSpecificationsPageActionControllerUnittest&) = delete;
  ~ProductSpecificationsPageActionControllerUnittest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  base::MockRepeatingCallback<void()> notify_host_callback_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  raw_ptr<commerce::MockClusterManager> mock_cluster_manager_;
  raw_ptr<commerce::MockProductSpecificationsService>
      mock_product_specifications_service_;
  std::unique_ptr<ProductSpecificationsPageActionController> controller_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ProductSpecificationsPageActionControllerUnittest, IconShow) {
  EXPECT_CALL(*mock_cluster_manager_, GetProductGroupForCandidateProduct)
      .Times(1);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(1);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  mock_cluster_manager_->SetResponseForGetProductGroupForCandidateProduct(
      CreateProductGroup());
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_TRUE(controller_->WantsExpandedUi());
  ASSERT_EQ(l10n_util::GetStringFUTF16(
                IDS_PRODUCT_SPECIFICATIONS_PAGE_ACTION_ADD,
                base::UTF8ToUTF16(static_cast<std::string>(kSetTitle))),
            controller_->GetProductSpecificationsLabel(false));
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       IconNotwShow_NoProductGroup) {
  EXPECT_CALL(*mock_cluster_manager_, GetProductGroupForCandidateProduct)
      .Times(1);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(1);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  // Mock no product group for current URL.
  mock_cluster_manager_->SetResponseForGetProductGroupForCandidateProduct(
      std::nullopt);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());
  ASSERT_EQ(l10n_util::GetStringUTF16(
                IDS_PRODUCT_SPECIFICATIONS_PAGE_ACTION_ADD_DEFAULT),
            controller_->GetProductSpecificationsLabel(false));
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       IconNotwShow_NoProductInfo) {
  EXPECT_CALL(*mock_cluster_manager_, GetProductGroupForCandidateProduct)
      .Times(0);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(1);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  // Mock no product info for current URL.
  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);

  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       IconNotwShow_FeatureIneligible) {
  // Mock Ineligible for the feature.
  account_checker_->SetSignedIn(false);

  EXPECT_CALL(*mock_cluster_manager_, GetProductGroupForCandidateProduct)
      .Times(0);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(0);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(0));

  // Before a navigation, the controller already rejects.
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest, IconExecute) {
  // Set up ClusterManager to trigger page action.
  auto product_group = CreateProductGroup();
  mock_cluster_manager_->SetResponseForGetProductGroupForCandidateProduct(
      product_group);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  // Set up the similar product specifications set.
  const base::Uuid& uuid = base::Uuid::GenerateRandomV4();
  ProductSpecificationsSet set = ProductSpecificationsSet(
      product_group->uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl2)}, "set1");
  ON_CALL(*mock_product_specifications_service_, GetSetByUuid)
      .WillByDefault(testing::Return(std::move(set)));

  // Trigger page action.
  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());

  // First click would add the product to the product specifications set.
  std::vector<GURL> expected_urls = {GURL(kTestUrl2), GURL(kTestUrl1)};
  EXPECT_CALL(*mock_product_specifications_service_,
              SetUrls(product_group->uuid, expected_urls))
      .Times(1);
  controller_->OnIconClicked();
  ASSERT_TRUE(controller_->IsInRecommendedSet());

  // Second click would remove the product from the product specifications set.
  expected_urls = {GURL(kTestUrl2)};
  EXPECT_CALL(*mock_product_specifications_service_,
              SetUrls(product_group->uuid, expected_urls))
      .Times(1);
  controller_->OnIconClicked();
  ASSERT_FALSE(controller_->IsInRecommendedSet());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       OnProductSpecificationsSetAdded) {
  // Set up ClusterManager to trigger page action.
  auto product_group = CreateProductGroup();
  mock_cluster_manager_->SetResponseForGetProductGroupForCandidateProduct(
      product_group);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  // Trigger page action.
  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());

  // No change if a set is added but doesn't include the current URL.
  ProductSpecificationsSet set1 = ProductSpecificationsSet(
      product_group->uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl2)}, "set1");
  controller_->OnProductSpecificationsSetAdded(std::move(set1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());

  // Hide the page action if a set is added with the current URL.
  EXPECT_CALL(notify_host_callback_, Run()).Times(1);
  ProductSpecificationsSet set2 =
      ProductSpecificationsSet(product_group->uuid.AsLowercaseString(), 0, 0,
                               {GURL(kTestUrl1), GURL(kTestUrl2)}, "set2");
  controller_->OnProductSpecificationsSetAdded(std::move(set2));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       OnProductSpecificationsSetRemoved) {
  // Set up ClusterManager to trigger page action.
  auto product_group = CreateProductGroup();
  mock_cluster_manager_->SetResponseForGetProductGroupForCandidateProduct(
      product_group);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  // Trigger page action.
  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());

  // No change if a irrelevant set is removed.
  ProductSpecificationsSet set1 = ProductSpecificationsSet(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
      {GURL(kTestUrl2)}, "set1");
  controller_->OnProductSpecificationsSetRemoved(std::move(set1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());

  // Hide the page action if the recommended set is removed.
  EXPECT_CALL(notify_host_callback_, Run()).Times(1);
  ProductSpecificationsSet set2 = ProductSpecificationsSet(
      product_group->uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl2)}, "set2");
  controller_->OnProductSpecificationsSetRemoved(std::move(set2));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       OnProductSpecificationsSetUpdated) {
  // Set up ClusterManager to trigger page action.
  auto product_group = CreateProductGroup();
  mock_cluster_manager_->SetResponseForGetProductGroupForCandidateProduct(
      product_group);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  // Trigger page action.
  controller_->ResetForNewNavigation(GURL(kTestUrl1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());

  EXPECT_CALL(notify_host_callback_, Run()).Times(2);
  ProductSpecificationsSet set1 = ProductSpecificationsSet(
      product_group->uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl2)}, "set");
  ProductSpecificationsSet set2 =
      ProductSpecificationsSet(product_group->uuid.AsLowercaseString(), 0, 0,
                               {GURL(kTestUrl1), GURL(kTestUrl2)}, "set");

  // Notify the controller that the current page has been added to the
  // recommended set.
  controller_->OnProductSpecificationsSetUpdate(set1, set2);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_TRUE(controller_->IsInRecommendedSet());

  // Notify the controller that the current page has been removed from the
  // recommended set.
  controller_->OnProductSpecificationsSetUpdate(set2, set1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->IsInRecommendedSet());
}
}  // namespace commerce
