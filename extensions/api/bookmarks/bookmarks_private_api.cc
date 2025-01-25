// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/bookmarks/bookmarks_private_api.h"

#include <memory>
#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_uma.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/top_sites.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

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
#include "browser/vivaldi_browser_finder.h"

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
// Generates a default path (including a default filename) that will be
// used for pre-populating the "Export Bookmarks" file chooser dialog box.
base::FilePath GetDefaultFilepathForBookmarkExport() {
  base::Time time = base::Time::Now();

  // Concatenate a date stamp to the filename.
  std::string filename =
      l10n_util::GetStringFUTF8(IDS_EXPORT_BOOKMARKS_DEFAULT_FILENAME,
                                base::TimeFormatShortDateNumeric(time));
  base::FilePath path = base::FilePath::FromUTF8Unsafe(filename);
  base::FilePath::StringType path_str = path.value();
  base::i18n::ReplaceIllegalCharactersInPath(&path_str, '_');
  path = base::FilePath(path_str);

  base::FilePath default_path;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_path);
  return default_path.Append(path);
}

}  // namespace

namespace bookmarks_private = vivaldi::bookmarks_private;

VivaldiBookmarksAPI::VivaldiBookmarksAPI(content::BrowserContext* context)
    : browser_context_(context),
      bookmark_model_(BookmarkModelFactory::GetForBrowserContext(context)) {
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

void VivaldiBookmarksAPI::BookmarkMetaInfoChanged(const BookmarkNode* node) {
  bookmarks_private::OnMetaInfoChanged::ChangeInfo change_info;
  change_info.speeddial = vivaldi_bookmark_kit::GetSpeeddial(node);
  change_info.bookmarkbar = vivaldi_bookmark_kit::GetBookmarkbar(node);
  change_info.description = vivaldi_bookmark_kit::GetDescription(node);
  change_info.thumbnail = vivaldi_bookmark_kit::GetThumbnail(node);
  change_info.nickname = vivaldi_bookmark_kit::GetNickname(node);
  change_info.theme_color = vivaldi_bookmark_kit::GetThemeColorForCSS(node);

  ::vivaldi::BroadcastEvent(bookmarks_private::OnMetaInfoChanged::kEventName,
                            bookmarks_private::OnMetaInfoChanged::Create(
                                base::NumberToString(node->id()), change_info),
                            browser_context_);
}

void VivaldiBookmarksAPI::BookmarkNodeFaviconChanged(const BookmarkNode* node) {
  if (!node->is_favicon_loaded() && !node->is_favicon_loading()) {
    // Forces loading the favicon
    bookmark_model_->GetFavicon(node);
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

ExtensionFunction::ResponseAction BookmarksPrivateGetFolderIdsFunction::Run() {
  namespace Results = vivaldi::bookmarks_private::GetFolderIds::Results;

  vivaldi::bookmarks_private::FolderIds ids;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(
      browser_context());
  if (model) {
    ids.bookmarks = base::NumberToString(model->bookmark_bar_node()->id());
    ids.mobile = base::NumberToString(model->mobile_node()->id());
    ids.trash = base::NumberToString(model->trash_node()->id());
  }
  return RespondNow(ArgumentList(Results::Create(ids)));
}

ExtensionFunction::ResponseValue
BookmarksPrivateEmptyTrashFunction::RunOnReady() {
  namespace Results = vivaldi::bookmarks_private::EmptyTrash::Results;

  bool success = false;

  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* trash_node = model->trash_node();
  if (trash_node) {
    while (!trash_node->children().empty()) {
      const BookmarkNode* remove_node = trash_node->children()[0].get();
      model->Remove(remove_node, {}, FROM_HERE);
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

  std::optional<Params> params = Params::Create(args());
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

BookmarksPrivateIOFunction::BookmarksPrivateIOFunction() {}

BookmarksPrivateIOFunction::~BookmarksPrivateIOFunction() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void BookmarksPrivateIOFunction::ShowSelectFileDialog(
    ui::SelectFileDialog::Type type,
    int window_id,
    const base::FilePath& default_path) {
  if (!dispatcher())
    return;  // Extension was unloaded.

  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  // Fail if we can not locate owning window
  Browser* browser = ::vivaldi::FindBrowserByWindowId(window_id);
  CHECK(browser && browser->window());
  gfx::NativeWindow owning_window = browser->window()->GetNativeWindow();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Balanced in one of the three callbacks of SelectFileDialog:
  // either FileSelectionCanceled, MultiFilesSelected, or FileSelected
  AddRef();

  content::WebContents* web_contents = GetSenderWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));

  select_file_dialog_->SelectFile(
      type, std::u16string(), default_path, &file_type_info, 0,
      base::FilePath::StringType(), owning_window, nullptr);
}

void BookmarksPrivateIOFunction::FileSelectionCanceled() {
  select_file_dialog_.reset();
  Release();  // Balanced in BookmarkManagerPrivateIOFunction::SelectFile()
}

void BookmarksPrivateIOFunction::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  select_file_dialog_.reset();
  Release();  // Balanced in BookmarsIOFunction::SelectFile()
  NOTREACHED() << "Should not be able to select multiple files";
}

ExtensionFunction::ResponseValue
BookmarksPrivateExportFunction::RunOnReady() {
  using vivaldi::bookmarks_private::Export::Params;

  std::optional<Params> params = Params::Create(args());
  if (!params)
    return BadMessage();

  // "bookmarks.export" is exposed to a small number of extensions. These
  // extensions use user gesture for export, so use USER_VISIBLE priority.
  // GetDefaultFilepathForBookmarkExport() might have to touch filesystem
  // (stat or access, for example), so this requires IO.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetDefaultFilepathForBookmarkExport),
      base::BindOnce(&BookmarksPrivateIOFunction::ShowSelectFileDialog,
                     this, ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                     params->window_id));
  // TODO(crbug.com/1073255): This will respond before a file is selected, which
  // seems incorrect. Waiting and responding until after
  // ui::SelectFileDialog::Listener is fired should be right thing to do, but
  // that requires auditing bookmark page callsites.
  return NoArguments();
}

void BookmarksPrivateExportFunction::FileSelected(
    const ui::SelectedFileInfo& path,
    int index) {
  bookmark_html_writer::WriteBookmarks(GetProfile(), path.file_path, nullptr);
  select_file_dialog_.reset();
  Release();  // Balanced in BookmarkManagerPrivateIOFunction::SelectFile()
}

}  // namespace extensions
