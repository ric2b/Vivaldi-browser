// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_ui.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/commerce_product_specifications_resources.h"
#include "chrome/grit/commerce_product_specifications_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace commerce {

ProductSpecificationsUI::ProductSpecificationsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  if (!shopping_service ||
      !IsProductSpecificationsEnabled(shopping_service->GetAccountChecker())) {
    return;
  }
  // Add ThemeSource to serve the chrome logo.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
  // Add SanitizedImageSource to embed images in WebUI.
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  // Set up the chrome://compare source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUICompareHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kCommerceProductSpecificationsResources,
                      kCommerceProductSpecificationsResourcesSize),
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_HTML);

  // Set up chrome://compare/disclosure
  source->AddResourcePath(
      "disclosure/",
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_PRODUCT_SPECIFICATIONS_DISCLOSURE_HTML);
  source->AddResourcePath(
      "disclosure",
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_PRODUCT_SPECIFICATIONS_DISCLOSURE_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"acceptDisclosure", IDS_PRODUCT_SPECIFICATIONS_DISCLOSURE_ACCEPT},
      {"addToNewGroup", IDS_PRODUCT_SPECIFICATIONS_ADD_TO_NEW_GROUP},
      {"delete", IDS_PRODUCT_SPECIFICATIONS_DELETE},
      {"disclosureAboutItem", IDS_PRODUCT_SPECIFICATIONS_DISCLOSURE_ABOUT_ITEM},
      {"disclosureAccountItem",
       IDS_PRODUCT_SPECIFICATIONS_DISCLOSURE_ACCOUNT_ITEM},
      {"disclosureDataItem", IDS_PRODUCT_SPECIFICATIONS_DISCLOSURE_DATA_ITEM},
      {"disclosureItemsHeader",
       IDS_PRODUCT_SPECIFICATIONS_DISCLOSURE_ITEMS_HEADER},
      {"disclosureTitle", IDS_PRODUCT_SPECIFICATIONS_DISCLOSURE_TITLE},
      {"emptyMenu", IDS_PRODUCT_SPECIFICATIONS_EMPTY_SELECTION_MENU},
      {"emptyProductSelector",
       IDS_PRODUCT_SPECIFICATIONS_EMPTY_PRODUCT_SELECTOR},
      {"emptyStateDescription",
       IDS_PRODUCT_SPECIFICATIONS_EMPTY_STATE_TITLE_DESCRIPTION},
      {"emptyStateTitle", IDS_PRODUCT_SPECIFICATIONS_EMPTY_STATE_TITLE},
      {"experimentalFeatureDisclaimer", IDS_PRODUCT_SPECIFICATIONS_DISCLAIMER},
      {"learnMore", IDS_LEARN_MORE},
      {"learnMoreA11yLabel", IDS_PRODUCT_SPECIFICATIONS_LEARN_MORE_A11Y_LABEL},
      {"priceRowTitle", IDS_PRODUCT_SPECIFICATIONS_PRICE_ROW_TITLE},
      {"recentlyViewedTabs",
       IDS_PRODUCT_SPECIFICATIONS_RECENTLY_VIEWED_TABS_SECTION},
      {"removeColumn", IDS_PRODUCT_SPECIFICATIONS_REMOVE_COLUMN},
      {"renameGroup", IDS_PRODUCT_SPECIFICATIONS_RENAME_GROUP},
      {"seeAll", IDS_PRODUCT_SPECIFICATIONS_SEE_ALL},
      {"suggestedTabs", IDS_PRODUCT_SPECIFICATIONS_SUGGESTIONS_SECTION},
      {"thumbsDown", IDS_THUMBS_DOWN},
      {"thumbsUp", IDS_THUMBS_UP},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddString("message", "Some example content...");
  source->AddString("pageTitle", "Product Specifications");
  source->AddString("summaryTitle", "Summary");
}

void ProductSpecificationsUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ProductSpecificationsUI::BindInterface(
    mojo::PendingReceiver<
        shopping_service::mojom::ShoppingServiceHandlerFactory> receiver) {
  shopping_service_factory_receiver_.reset();
  shopping_service_factory_receiver_.Bind(std::move(receiver));
}

void ProductSpecificationsUI::CreateShoppingServiceHandler(
    mojo::PendingRemote<shopping_service::mojom::Page> page,
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  shopping_service_handler_ =
      std::make_unique<commerce::ShoppingServiceHandler>(
          std::move(page), std::move(receiver), bookmark_model,
          shopping_service, profile->GetPrefs(), tracker,
          std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr,
                                                                profile),
          optimization_guide_keyed_service
              ? optimization_guide_keyed_service
                    ->GetModelQualityLogsUploaderService()
              : nullptr);
}

ProductSpecificationsUI::~ProductSpecificationsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ProductSpecificationsUI)

ProductSpecificationsUIConfig::ProductSpecificationsUIConfig()
    : WebUIConfig(content::kChromeUIScheme, kChromeUICompareHost) {}

ProductSpecificationsUIConfig::~ProductSpecificationsUIConfig() = default;

std::unique_ptr<content::WebUIController>
ProductSpecificationsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                     const GURL& url) {
  return std::make_unique<ProductSpecificationsUI>(web_ui);
}

}  // namespace commerce
