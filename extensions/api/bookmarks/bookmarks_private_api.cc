// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/bookmarks/bookmarks_private_api.h"

#include <memory>
#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/top_sites.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "browser/vivaldi_default_bookmarks.h"
#include "browser/vivaldi_default_bookmarks_updater_client_impl.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/datasource/vivaldi_image_store.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/bookmarks_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/win/jumplist.h"
#include "chrome/browser/win/jumplist_factory.h"
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi::FindVivaldiBrowser;
using vivaldi::IsVivaldiApp;
using vivaldi::kVivaldiReservedApiError;

namespace extensions {

namespace {

// Give Chromium little pause to write bookmark file before checking for unused
// data urls to minimize disk IO spikes.
constexpr base::TimeDelta kDataUrlGCSTrashDelay = base::Milliseconds(100);

}  // namespace

namespace bookmarks_private = vivaldi::bookmarks_private;

VivaldiBookmarksAPI::VivaldiBookmarksAPI(content::BrowserContext* context)
    : browser_context_(context) {
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(context);
  if (bookmark_model_) {
    bookmark_model_->AddObserver(this);
  }
}

VivaldiBookmarksAPI::~VivaldiBookmarksAPI() {}

void VivaldiBookmarksAPI::Shutdown() {
  if (bookmark_model_) {
    bookmark_model_->RemoveObserver(this);
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>>::
    DestructorAtExit g_factory_bookmark = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>*
VivaldiBookmarksAPI::GetFactoryInstance() {
  return g_factory_bookmark.Pointer();
}

void VivaldiBookmarksAPI::BookmarkMetaInfoChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  bookmarks_private::OnMetaInfoChanged::ChangeInfo change_info;
  change_info.speeddial = vivaldi_bookmark_kit::GetSpeeddial(node);
  change_info.bookmarkbar = vivaldi_bookmark_kit::GetBookmarkbar(node);
  change_info.description = vivaldi_bookmark_kit::GetDescription(node);
  change_info.thumbnail = vivaldi_bookmark_kit::GetThumbnail(node);
  change_info.nickname = vivaldi_bookmark_kit::GetNickname(node);

  ::vivaldi::BroadcastEvent(bookmarks_private::OnMetaInfoChanged::kEventName,
                            bookmarks_private::OnMetaInfoChanged::Create(
                                base::NumberToString(node->id()), change_info),
                            browser_context_);
}

void VivaldiBookmarksAPI::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  if (!node->is_favicon_loaded() && !node->is_favicon_loading()) {
    // Forces loading the favicon
    model->GetFavicon(node);
  }
  if (!node->icon_url()) {
    return;
  }
  ::vivaldi::BroadcastEvent(
      bookmarks_private::OnFaviconChanged::kEventName,
      bookmarks_private::OnFaviconChanged::Create(
          base::NumberToString(node->id()), node->icon_url()->spec()),
      browser_context_);
}

ExtensionFunction::ResponseAction
BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction::Run() {
  using vivaldi::bookmarks_private::UpdateSpeedDialsForWindowsJumplist::Params;

  std::unique_ptr<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

#if BUILDFLAG(IS_WIN)
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
      VivaldiImageStore::ScheduleRemovalOfUnusedUrlData(browser_context(),
                                                        kDataUrlGCSTrashDelay);
    }
    success = true;
  }
  return ArgumentList(Results::Create(success));
}

ExtensionFunction::ResponseAction
BookmarksPrivateUpdatePartnersFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  vivaldi_default_bookmarks::UpdatePartners(
      vivaldi_default_bookmarks::UpdaterClientImpl::Create(profile),
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

  std::unique_ptr<Params> params = Params::Create(args());
  if (!params)
    BadMessage();

  std::string error;
  const BookmarkNode* node = GetBookmarkNodeFromId(params->bookmark_id, &error);
  if (!node)
    return Error(error);

  std::string url = vivaldi_bookmark_kit::GetThumbnail(node);
  bool is_custom_thumbnail =
      !url.empty() && !vivaldi_data_url_utils::IsBookmarkCaptureUrl(url);
  return ArgumentList(Results::Create(is_custom_thumbnail));
}

}  // namespace extensions
