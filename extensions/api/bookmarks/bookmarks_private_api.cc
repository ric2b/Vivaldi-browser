// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/bookmarks/bookmarks_private_api.h"

#include <memory>
#include <set>
#include <string>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/win/jumplist_factory.h"
#include "chrome/browser/win/jumplist.h"
#endif
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/history/core/browser/top_sites.h"
#include "extensions/schema/bookmarks_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using vivaldi::IsVivaldiApp;
using vivaldi::kVivaldiReservedApiError;
using vivaldi::FindVivaldiBrowser;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

namespace bookmarks_private = vivaldi::bookmarks_private;

// Helper
static void GetPartnerIds(const bookmarks::BookmarkNode* node,
                          std::vector<std::string>* ids,
                          bool include_children) {
  std::string partner;
  if (node->GetMetaInfo("Partner", &partner) && partner.length() > 0) {
    ids->push_back(partner);
  }
  if (include_children) {
    for (auto& it: node->children()) {
      GetPartnerIds(it.get(), ids, true);
    }
  }
}

static void ResetParterId(BookmarkModel* model,
                          const bookmarks::BookmarkNode* node,
                          bool include_children) {
  std::string partner;
  if (node->GetMetaInfo("Partner", &partner) && partner.length() > 0) {
    model->SetNodeMetaInfo(node, "Partner", "");
  }
  if (include_children) {
    for (auto& it: node->children()) {
      ResetParterId(model, it.get(), true);
    }
  }
}

static void ClearPartnerId(BookmarkModel* model, const BookmarkNode* node,
                           content::BrowserContext* browser_context,
                           bool update_model,
                           bool include_children) {
  std::vector<std::string> partners;
  GetPartnerIds(node, &partners, include_children);
  if (!partners.empty()) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    const base::ListValue* list = profile->GetPrefs()->GetList(
        vivaldiprefs::kBookmarksDeletedPartners);
    base::ListValue updated(list->GetList());
    for (auto partner = partners.begin(); partner != partners.end();
         partner++) {
      auto it = std::find(list->begin(), list->end(), base::Value(*partner));
      if (it == list->end()) {
        updated.GetList().push_back(base::Value(*partner));
      }
    }
    profile->GetPrefs()->Set(vivaldiprefs::kBookmarksDeletedPartners, updated);
    if (update_model) {
      ResetParterId(model, node, include_children);
    }
  }
}

// To determine meta data changes for which we should clear the partner id
class MetaInfoChangeFilter {
 public:
  MetaInfoChangeFilter(const BookmarkNode* node);
  bool HasChanged(const BookmarkNode* node);
 private:
  int64_t id_;
  bool speeddial_;
  bool bookmarkbar_;
  base::string16 description_;
  base::string16 nickname_;
};

MetaInfoChangeFilter::MetaInfoChangeFilter(const BookmarkNode* node)
  :id_(node->id()),
   speeddial_(node->GetSpeeddial()),
   bookmarkbar_(node->GetBookmarkbar()),
   description_(node->GetDescription()),
   nickname_(node->GetNickName()) {
}

bool MetaInfoChangeFilter::HasChanged(const BookmarkNode* node) {
  if (id_ == node->id() &&
      speeddial_ == node->GetSpeeddial() &&
      bookmarkbar_ == node->GetBookmarkbar() &&
      description_ == node->GetDescription() &&
      nickname_ == node->GetNickName()) {
    return false;
  }
  return true;
}

VivaldiBookmarksAPI::VivaldiBookmarksAPI(content::BrowserContext* context)
    : browser_context_(context),
      bookmark_model_(nullptr),
      partner_upgrade_active_(false) {
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(context);
  DCHECK(bookmark_model_);
  bookmark_model_->AddObserver(this);
}

VivaldiBookmarksAPI::~VivaldiBookmarksAPI() {}

void VivaldiBookmarksAPI::Shutdown() {
  bookmark_model_->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI> >::
    DestructorAtExit g_factory_bookmark = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>*
VivaldiBookmarksAPI::GetFactoryInstance() {
  return g_factory_bookmark.Pointer();
}

void VivaldiBookmarksAPI::SetPartnerUpgradeActive(bool active) {
  partner_upgrade_active_ = active;
}

// static
std::string VivaldiBookmarksAPI::GetThumbnailUrl(
    const bookmarks::BookmarkNode* node) {
  std::string thumbnail_url;
  if (!node->GetMetaInfo("Thumbnail", &thumbnail_url))
    return std::string();
  return thumbnail_url;
}

void VivaldiBookmarksAPI::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {}

void VivaldiBookmarksAPI::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  // We're removing the bookmark (emptying the trash most likely),
  // remove the thumbnail.
  VivaldiDataSourcesAPI::OnUrlChange(browser_context_, GetThumbnailUrl(node),
                                     std::string());
  // Register deleted partner node.
  if (!partner_upgrade_active_) {
    ClearPartnerId(model, node, browser_context_, false, true);
  }
}

void VivaldiBookmarksAPI::BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) {
  // Register modified partner node.
  if (!partner_upgrade_active_) {
    ClearPartnerId(model, node, browser_context_, true, false);
  }
}

void VivaldiBookmarksAPI::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  saved_thumbnail_url_ = GetThumbnailUrl(node);
  // No need to filter on upgrade
  if (!partner_upgrade_active_) {
    change_filter_.reset(new MetaInfoChangeFilter(node));
  }
}

void VivaldiBookmarksAPI::BookmarkMetaInfoChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  std::string thumbnail_url = GetThumbnailUrl(node);
  if (thumbnail_url != saved_thumbnail_url_) {
    VivaldiDataSourcesAPI::OnUrlChange(
        browser_context_, saved_thumbnail_url_, thumbnail_url);
  }
  saved_thumbnail_url_.clear();

  bookmarks_private::OnMetaInfoChanged::ChangeInfo change_info;
  change_info.speeddial.reset(new bool(node->GetSpeeddial()));
  change_info.bookmarkbar.reset(new bool(node->GetBookmarkbar()));
  change_info.description.reset(
      new std::string(base::UTF16ToUTF8(node->GetDescription())));
  change_info.thumbnail.reset(new std::string(std::move(thumbnail_url)));
  change_info.nickname.reset(
      new std::string(base::UTF16ToUTF8(node->GetNickName())));
  // We can add visited time here if we want. Currently not used in UI.

  if (change_filter_ && change_filter_->HasChanged(node)) {
    ClearPartnerId(model, node, browser_context_, true, false);
  }
  change_filter_.reset();


  ::vivaldi::BroadcastEvent(bookmarks_private::OnMetaInfoChanged::kEventName,
                            bookmarks_private::OnMetaInfoChanged::Create(
                                base::NumberToString(node->id()), change_info),
                            browser_context_);
}

void VivaldiBookmarksAPI::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  ::vivaldi::BroadcastEvent(bookmarks_private::OnFaviconChanged::kEventName,
                            bookmarks_private::OnFaviconChanged::Create(
                                base::NumberToString(node->id())),
                            browser_context_);
}

ExtensionFunction::ResponseAction
BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::Run() {
  using vivaldi::bookmarks_private::UpdateSpeedDialsForWindowsJumplist::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

#if defined(OS_WIN)
  Browser* browser = FindVivaldiBrowser();
  if (browser && browser->is_vivaldi()) {
    JumpList* jump_list = JumpListFactory::GetForProfile(browser->profile());
    if (jump_list)
      jump_list->NotifyVivaldiSpeedDialsChanged(params->speed_dials);
  }
#endif
  return RespondNow(NoArguments());
}

bool BookmarksPrivateEmptyTrashFunction::RunOnReady() {
  namespace Results = vivaldi::bookmarks_private::EmptyTrash::Results;

  bool success = false;

  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* trash_node = model->trash_node();
  if (trash_node) {
    VivaldiDataSourcesAPI::SetBulkChangesMode(browser_context(), true);
    while (!trash_node->children().empty()) {
      const BookmarkNode* remove_node = trash_node->children()[0].get();
      model->Remove(remove_node);
    }
    VivaldiDataSourcesAPI::SetBulkChangesMode(browser_context(), false);
    success = true;
  }
  results_ = Results::Create(success);
  return true;
}

bool BookmarksPrivateUpgradePartnerFunction::RunOnReady() {
  ::vivaldi::SetPartnerUpgrade auto_set(browser_context(), true);
  return BookmarksUpdateFunction::RunOnReady();
}

bool BookmarksPrivateIsCustomThumbnailFunction::RunOnReady() {
  using vivaldi::bookmarks_private::IsCustomThumbnail::Params;
  namespace Results = vivaldi::bookmarks_private::IsCustomThumbnail::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const BookmarkNode* node = GetBookmarkNodeFromId(params->bookmark_id);
  if (!node)
    return false;

  std::string url = VivaldiBookmarksAPI::GetThumbnailUrl(node);
  bool is_custom_thumbnail =
      !url.empty() && !VivaldiDataSourcesAPI::IsBookmarkCapureUrl(url);
  results_ = Results::Create(is_custom_thumbnail);
  return true;
}

}  // namespace extensions
