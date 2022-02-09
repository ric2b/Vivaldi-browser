// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/bookmarks/bookmarks_private_api.h"

#include <memory>
#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/win/jumplist.h"
#include "chrome/browser/win/jumplist_factory.h"
#endif
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/top_sites.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "browser/vivaldi_default_bookmarks.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/bookmarks_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi::FindVivaldiBrowser;
using vivaldi::IsVivaldiApp;
using vivaldi::kVivaldiReservedApiError;

namespace extensions {

namespace {

// Give Chromium little pause to write bookmark file before checking for unused
// data urls to minimize disk IO spikes.
constexpr base::TimeDelta kDataUrlGCSTrashDelay =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace

namespace bookmarks_private = vivaldi::bookmarks_private;

// Helper
static void GetPartnerIds(const bookmarks::BookmarkNode* node,
                          bool include_children,
                          std::vector<base::GUID>& ids) {
  base::GUID partner_id = vivaldi_bookmark_kit::GetPartner(node);
  if (partner_id.is_valid()) {
    ids.push_back(partner_id);
  }
  if (include_children) {
    for (auto& it : node->children()) {
      GetPartnerIds(it.get(), true, ids);
    }
  }
}

static void ResetParterId(BookmarkModel* model,
                          const bookmarks::BookmarkNode* node,
                          bool include_children) {
  vivaldi_bookmark_kit::RemovePartnerId(model, node);
  if (include_children) {
    for (auto& it : node->children()) {
      ResetParterId(model, it.get(), true);
    }
  }
}

static void ClearPartnerId(BookmarkModel* model,
                           const BookmarkNode* node,
                           content::BrowserContext* browser_context,
                           bool update_model,
                           bool include_children) {
  std::vector<base::GUID> partners;
  GetPartnerIds(node, include_children, partners);
  if (!partners.empty()) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    const base::Value* list_value =
        profile->GetPrefs()->GetList(vivaldiprefs::kBookmarksDeletedPartners);
    base::Value::ListStorage updated(list_value->Clone().TakeList());
    base::Value::ConstListView list_view = list_value->GetList();
    for (const base::GUID& partner_id : partners) {
      base::Value v(partner_id.AsLowercaseString());
      auto it = std::find(list_view.begin(), list_view.end(), v);
      if (it == list_view.end()) {
        updated.push_back(std::move(v));
      }
    }
    profile->GetPrefs()->Set(vivaldiprefs::kBookmarksDeletedPartners,
                             base::Value(std::move(updated)));
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
  std::string description_;
  std::string nickname_;
};

MetaInfoChangeFilter::MetaInfoChangeFilter(const BookmarkNode* node)
    : id_(node->id()),
      speeddial_(vivaldi_bookmark_kit::GetSpeeddial(node)),
      bookmarkbar_(vivaldi_bookmark_kit::GetBookmarkbar(node)),
      description_(vivaldi_bookmark_kit::GetDescription(node)),
      nickname_(vivaldi_bookmark_kit::GetNickname(node)) {}

bool MetaInfoChangeFilter::HasChanged(const BookmarkNode* node) {
  if (id_ == node->id() &&
      speeddial_ == vivaldi_bookmark_kit::GetSpeeddial(node) &&
      bookmarkbar_ == vivaldi_bookmark_kit::GetBookmarkbar(node) &&
      description_ == vivaldi_bookmark_kit::GetDescription(node) &&
      nickname_ == vivaldi_bookmark_kit::GetNickname(node)) {
    return false;
  }
  return true;
}

VivaldiBookmarksAPI::VivaldiBookmarksAPI(content::BrowserContext* context)
    : browser_context_(context), bookmark_model_(nullptr) {
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(context);
  DCHECK(bookmark_model_);
  bookmark_model_->AddObserver(this);
}

VivaldiBookmarksAPI::~VivaldiBookmarksAPI() {}

void VivaldiBookmarksAPI::Shutdown() {
  bookmark_model_->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>>::
    DestructorAtExit g_factory_bookmark = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>*
VivaldiBookmarksAPI::GetFactoryInstance() {
  return g_factory_bookmark.Pointer();
}

void VivaldiBookmarksAPI::BookmarkModelLoaded(BookmarkModel* model,
                                              bool ids_reassigned) {}

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
  // Register deleted partner node.
  if (!vivaldi_default_bookmarks::g_bookmark_update_actve) {
    ClearPartnerId(model, node, browser_context_, false, true);
  }
}

void VivaldiBookmarksAPI::BookmarkNodeChanged(BookmarkModel* model,
                                              const BookmarkNode* node) {
  // Register modified partner node.
  if (!vivaldi_default_bookmarks::g_bookmark_update_actve) {
    ClearPartnerId(model, node, browser_context_, true, false);
  }
}

void VivaldiBookmarksAPI::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  // No need to filter on upgrade
  if (!vivaldi_default_bookmarks::g_bookmark_update_actve) {
    change_filter_.reset(new MetaInfoChangeFilter(node));
  }
}

void VivaldiBookmarksAPI::BookmarkMetaInfoChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  bookmarks_private::OnMetaInfoChanged::ChangeInfo change_info;
  change_info.speeddial =
      std::make_unique<bool>(vivaldi_bookmark_kit::GetSpeeddial(node));
  change_info.bookmarkbar =
      std::make_unique<bool>(vivaldi_bookmark_kit::GetBookmarkbar(node));
  change_info.description =
      std::make_unique<std::string>(vivaldi_bookmark_kit::GetDescription(node));
  change_info.thumbnail =
      std::make_unique<std::string>(vivaldi_bookmark_kit::GetThumbnail(node));
  change_info.nickname =
      std::make_unique<std::string>(vivaldi_bookmark_kit::GetNickname(node));

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

ExtensionFunction::ResponseValue
BookmarksPrivateEmptyTrashFunction::RunOnReady() {
  namespace Results = vivaldi::bookmarks_private::EmptyTrash::Results;

  bool success = false;

  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* trash_node = model->trash_node();
  if (trash_node) {
    bool removed = false;
    while (!trash_node->children().empty()) {
      const BookmarkNode* remove_node = trash_node->children()[0].get();
      model->Remove(remove_node);
      removed = true;
    }
    if (removed) {
      VivaldiDataSourcesAPI::ScheduleRemovalOfUnusedUrlData(
          browser_context(), kDataUrlGCSTrashDelay);
    }
    success = true;
  }
  return ArgumentList(Results::Create(success));
}

ExtensionFunction::ResponseAction
BookmarksPrivateUpdatePartnersFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  vivaldi_default_bookmarks::UpdatePartners(
      profile,
      base::BindOnce(
          &BookmarksPrivateUpdatePartnersFunction::OnUpdatePartnersResult,
          this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void BookmarksPrivateUpdatePartnersFunction::OnUpdatePartnersResult(
    bool ok,
    bool no_version,
    const std::string& locale) {
  namespace Results = vivaldi::bookmarks_private::UpdatePartners::Results;

  Respond(ArgumentList(Results::Create(ok, no_version, locale)));
}

ExtensionFunction::ResponseValue
BookmarksPrivateIsCustomThumbnailFunction::RunOnReady() {
  using vivaldi::bookmarks_private::IsCustomThumbnail::Params;
  namespace Results = vivaldi::bookmarks_private::IsCustomThumbnail::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  if (!params)
    BadMessage();

  std::string error;
  const BookmarkNode* node = GetBookmarkNodeFromId(params->bookmark_id, &error);
  if (!node)
    return Error(error);

  std::string url = vivaldi_bookmark_kit::GetThumbnail(node);
  bool is_custom_thumbnail =
      !url.empty() && !vivaldi_data_url_utils::IsBookmarkCapureUrl(url);
  return ArgumentList(Results::Create(is_custom_thumbnail));
}

}  // namespace extensions
