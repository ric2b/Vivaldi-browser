// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/import_data/import_data_api.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_resources.h"

#include "base/base_paths.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/pref_names.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#include "extensions/browser/view_type_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern_set.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_finder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "chrome/browser/prefs/session_startup_pref.h"

#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "extensions/schema/import_data.h"
#include "extensions/tools/vivaldi_tools.h"

#include "components/strings/grit/components_strings.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"


#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

class Browser;

namespace extensions {

namespace GetProfiles = vivaldi::import_data::GetProfiles;
using content::WebContents;
using vivaldi::import_data::ImportTypes;

// ProfileSingletonFactory
bool ProfileSingletonFactory::instanceFlag = false;
ProfileSingletonFactory* ProfileSingletonFactory::single = NULL;

ProfileSingletonFactory* ProfileSingletonFactory::getInstance() {
  if (!instanceFlag) {
    single = new ProfileSingletonFactory();
    instanceFlag = true;
    return single;
  } else {
    return single;
  }
}

ProfileSingletonFactory::ProfileSingletonFactory() {
  api_importer_list_.reset(new ImporterList());
  profilesRequested = false;
}

ProfileSingletonFactory::~ProfileSingletonFactory() {
  instanceFlag = false;
  profilesRequested = false;
}

ImporterList* ProfileSingletonFactory::getImporterList() {
  return single->api_importer_list_.get();
}

void ProfileSingletonFactory::setProfileRequested(bool profileReq) {
  profilesRequested = profileReq;
}

bool ProfileSingletonFactory::getProfileRequested() {
  return profilesRequested;
}

namespace {
// A getline function that deals with cross-platform line endings. See:
// https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
// Also tells us wheter line ended with both cr and lf.
std::istream& safeGetline(std::istream& is, std::string& line, bool& crlf) {
  line.clear();

  // The characters in the stream are read one-by-one using a std::streambuf.
  // That is faster than reading them one-by-one using the std::istream.
  // Code that uses streambuf this way must be guarded by a sentry object.
  // The sentry object performs various tasks,
  // such as thread synchronization and updating the stream state.

  std::istream::sentry se(is, true);
  std::streambuf* sb = is.rdbuf();

  crlf = false;
  for (;;) {
    int c = sb->sbumpc();
    switch (c) {
      case '\n':
        return is;
      case '\r':
        if (sb->sgetc() == '\n') {
          crlf = true;
          sb->sbumpc();
        }
        return is;
      case std::streambuf::traits_type::eof():
        // Also handle the case when the last line has no line ending
        if (line.empty())
          is.setstate(std::ios::eofbit);
        return is;
      default:
        line += (char)c;
    }
  }
}

// These are really flags, but never sent as flags.
static struct ImportItemToStringMapping {
  importer::ImportItem item;
  const char* name;
} import_item_string_mapping[]{
    //  NOTE(julien): We explicitly do not support importing searches ( see
    //  VB-20905 )
    // clang-format off
    {importer::FAVORITES, "favorites"},
    {importer::PASSWORDS, "passwords"},
    {importer::HISTORY, "history"},
    {importer::COOKIES, "cookies"},
    {importer::NOTES, "notes"},
    {importer::SPEED_DIAL, "speeddial"},
    {importer::CONTACTS, "contacts"},
    {importer::EXTENSIONS, "extensions"},
    {importer::TABS, "tabs"},
    // clang-format on
};

const size_t kImportItemToStringMappingLength =
    std::size(import_item_string_mapping);

const std::string ImportItemToString(importer::ImportItem item) {
  for (size_t i = 0; i < kImportItemToStringMappingLength; i++) {
    if (item == import_item_string_mapping[i].item) {
      return import_item_string_mapping[i].name;
    }
  }
  // Missing datatype in the array?
  NOTREACHED();
  //return nullptr;
}


#if BUILDFLAG(IS_WIN)
  static std::string toSystemUTF(const std::wstring& str) {
    return base::WideToUTF8(str);
  }
#else
  static std::string toSystemUTF(const std::string& str) {
    return str;
  }
#endif  // BUILDFLAG(IS_WIN)

  bool GetDefaultUserDataDirectory(base::FilePath* result) {
#if BUILDFLAG(IS_WIN)
    const auto key = base::DIR_COMMON_APP_DATA;
#elif BUILDFLAG(IS_MAC)
    const auto key = base::DIR_APP_DATA;
#else
    const auto key = base::DIR_HOME;
#endif
    bool success = false;
    if (result && base::PathService::Get(key, result)) {
      success = true;
    }
    return success;
  }

}  // namespace

ImportDataAPI::ImportDataAPI(content::BrowserContext* context)
    : browser_context_(context),
      import_succeeded_count_(0) {}

ImportDataAPI::~ImportDataAPI() {}

void ImportDataAPI::StartImport(const importer::SourceProfile& source_profile,
                                uint16_t imported_items) {
  if (!imported_items)
    return;

  import_succeeded_count_ = 0;

  DCHECK(!importer_host_);

  // If another import is already ongoing, let it finish silently.
  if (importer_host_)
    importer_host_->set_observer(nullptr);

  base::Value importing(true);

  importer_host_ = new ExternalProcessImporterHost();
  importer_host_->set_observer(this);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  importer_host_->StartImportSettings(
      source_profile, profile, imported_items,
      base::MakeRefCounted<ProfileWriter>(profile).get());
}

void ImportDataAPI::ImportStarted() {
  ::vivaldi::BroadcastEvent(vivaldi::import_data::OnImportStarted::kEventName,
                            vivaldi::import_data::OnImportStarted::Create(),
                            browser_context_);
}

void ImportDataAPI::ImportItemStarted(importer::ImportItem item) {
  import_succeeded_count_++;
  const std::string item_name = ImportItemToString(item);

  ::vivaldi::BroadcastEvent(
      vivaldi::import_data::OnImportItemStarted::kEventName,
      vivaldi::import_data::OnImportItemStarted::Create(item_name),
      browser_context_);
}

void ImportDataAPI::ImportItemEnded(importer::ImportItem item) {
  import_succeeded_count_--;
  const std::string item_name = ImportItemToString(item);

  ::vivaldi::BroadcastEvent(
      vivaldi::import_data::OnImportItemEnded::kEventName,
      vivaldi::import_data::OnImportItemEnded::Create(item_name),
      browser_context_);
}
void ImportDataAPI::ImportItemFailed(importer::ImportItem item,
                                     const std::string& error) {
  // Ensure we get an error at the end.
  import_succeeded_count_++;
  const std::string item_name = ImportItemToString(item);

  ::vivaldi::BroadcastEvent(
      vivaldi::import_data::OnImportItemFailed::kEventName,
      vivaldi::import_data::OnImportItemFailed::Create(item_name, error),
      browser_context_);
}

void ImportDataAPI::ImportEnded() {
  importer_host_->set_observer(nullptr);
  importer_host_ = nullptr;

  ::vivaldi::BroadcastEvent(
      vivaldi::import_data::OnImportEnded::kEventName,
      vivaldi::import_data::OnImportEnded::Create(import_succeeded_count_),
      browser_context_);
}

bool ImportDataAPI::OpenThunderbirdMailbox(std::string path,
                                           std::streampos seek_pos,
                                           std::streampos& fsize) {
  // NOTE: If mailbox is already open, we leave fsize unthouched.
  if (!thunderbird_mailbox_path_.empty()) {
    if (thunderbird_mailbox_path_ == path) {
      // This mailbox already open. Only seek.
      thunderbird_mailbox_.seekg(seek_pos);
      return true;
    } else {
      // New path. close the old one.
      thunderbird_mailbox_.close();
      if (path.empty()) {
        return true;
      }
    }
  }
  thunderbird_mailbox_path_ = path;

#if BUILDFLAG(IS_WIN)
  std::wstring wpath;
  base::UTF8ToWide(path.c_str(), path.size(), &wpath);
  thunderbird_mailbox_.open(wpath.c_str(), std::ios::binary | std::ios::ate);
#else
  thunderbird_mailbox_.open(path, std::ios::binary | std::ios::ate);
#endif  // BUILDFLAG(IS_WIN)

  fsize = thunderbird_mailbox_.tellg();

  thunderbird_mailbox_.seekg(seek_pos);
  if (thunderbird_mailbox_.fail()) {
    return false;
  }
  return true;
}

void ImportDataAPI::CloseThunderbirdMailbox() {
  thunderbird_mailbox_path_ = "";
  thunderbird_mailbox_.close();
}

bool ImportDataAPI::ReadThunderbirdMessage(std::string& content,
                                           std::streampos& seek_pos) {
  if (thunderbird_mailbox_path_.empty()) {
    return false;
  }
  std::string line;
  bool first_pass = true;
  bool crlf_ending;

  while (safeGetline(thunderbird_mailbox_, line, crlf_ending)) {
    // Message must start with 'From '.
    if (first_pass && line.rfind("From ", 0) != 0) {
      break;
    }

    // Saves a little bit of time.
    if (line.length() > 0 && line[0] == 'F') {
      // Test whether line begin with "From " and is a potential header.
      if (line.rfind("From ", 0) == 0 && !first_pass) {
        // Keep this line in case of false positive.
        std::string from_buffer = line;

        // Keep track of header length, i.e. backwards seek for next read.
        int seek = line.length() + (crlf_ending ? 2 : 1);

        if (safeGetline(thunderbird_mailbox_, line, crlf_ending)) {
          // Found start of next message. Put the header back and bail.
          if (line.rfind("X-", 0) == 0) {
            seek += line.length() + (crlf_ending ? 2 : 1);
            thunderbird_mailbox_.seekg(-seek, std::ios::cur);
            break;
            // False positive. Innocent 'From ' spotted in message.
          } else {
            content += from_buffer;
            content += '\n';
            seek = line.length() + (crlf_ending ? 2 : 1);
            thunderbird_mailbox_.seekg(-seek, std::ios::cur);
            continue;
          }
        }
      }
    }
    first_pass = false;
    content += line;
    content += '\n';
  }

  seek_pos = thunderbird_mailbox_.tellg();
  return true;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ImportDataAPI>>::
    DestructorAtExit g_factory_import = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ImportDataAPI>*
ImportDataAPI::GetFactoryInstance() {
  return g_factory_import.Pointer();
}

ImportTypes MapImportType(const importer::ImporterType& importer_type) {
  ImportTypes type;
  switch (importer_type) {
#if BUILDFLAG(IS_WIN)
    case importer::TYPE_IE:
      type = vivaldi::import_data::ImportTypes::kInternetExplorer;
      break;
#endif
    case importer::TYPE_FIREFOX:
      type = vivaldi::import_data::ImportTypes::kFirefox;
      break;
#if BUILDFLAG(IS_MAC)
    case importer::TYPE_SAFARI:
      type = vivaldi::import_data::ImportTypes::kSafari;
      break;
#endif
    case importer::TYPE_BOOKMARKS_FILE:
      type = vivaldi::import_data::ImportTypes::kBookmarksFile;
      break;

    case importer::TYPE_OPERA:
      type = vivaldi::import_data::ImportTypes::kOpera;
      break;

    case importer::TYPE_CHROME:
      type = vivaldi::import_data::ImportTypes::kChrome;
      break;

    case importer::TYPE_CHROMIUM:
      type = vivaldi::import_data::ImportTypes::kChromium;
      break;

    case importer::TYPE_VIVALDI:
      type = vivaldi::import_data::ImportTypes::kVivaldi;
      break;

    case importer::TYPE_YANDEX:
      type = vivaldi::import_data::ImportTypes::kYandex;
      break;

    case importer::TYPE_OPERA_OPIUM_BETA:
      type = vivaldi::import_data::ImportTypes::kOperaOpiumBeta;
      break;

    case importer::TYPE_OPERA_OPIUM_DEV:
      type = vivaldi::import_data::ImportTypes::kOperaOpiumDev;
      break;
    case importer::TYPE_BRAVE:
      type = vivaldi::import_data::ImportTypes::kBrave;
      break;
#if BUILDFLAG(IS_WIN)
    case importer::TYPE_EDGE:
      type = vivaldi::import_data::ImportTypes::kEdge;
      break;
#endif
    case importer::TYPE_EDGE_CHROMIUM:
      type = vivaldi::import_data::ImportTypes::kEdgeChromium;
      break;

    case importer::TYPE_THUNDERBIRD:
      type = vivaldi::import_data::ImportTypes::kThunderbird;
      break;

    case importer::TYPE_OPERA_OPIUM:
      type = vivaldi::import_data::ImportTypes::kOperaOpium;
      break;

    case importer::TYPE_ARC:
      type = vivaldi::import_data::ImportTypes::kArc;
      break;

    case importer::TYPE_OPERA_GX:
      type = vivaldi::import_data::ImportTypes::kOperaGx;
      break;

    case importer::TYPE_UNKNOWN:
      type = vivaldi::import_data::ImportTypes::kOperaOpium;
      break;
  }
  return type;
}
std::string MapSuggestedProfilePath(
    const vivaldi::import_data::ImportTypes& type) {
  using ImportTypes = vivaldi::import_data::ImportTypes;
  base::FilePath path;

  // Currently only Safari and bookmark files are treated specially.
  // The rest of ImportTypes is usually detected,
  // or should be in the user data dir (e.g ~/Library/Application Support)
  switch (type) {
    #if BUILDFLAG(IS_MAC)
    case ImportTypes::kSafari: {
      return base::apple::GetUserLibraryPath().Append("Safari").AsUTF8Unsafe();
    }
#endif
    case ImportTypes::kBookmarksFile:
      return chrome::GetUserDocumentsDirectory(&path) ? path.AsUTF8Unsafe()
                                                      : "";
    default:
      break;
  }
  return GetDefaultUserDataDirectory(&path) ? path.AsUTF8Unsafe() : "";
}

ImportDataGetProfilesFunction::ImportDataGetProfilesFunction() {}

ImportDataGetProfilesFunction::~ImportDataGetProfilesFunction() {}

void ImportDataGetProfilesFunction::Finished() {
  namespace Results = vivaldi::import_data::GetProfiles::Results;

  std::vector<vivaldi::import_data::ProfileItem> nodes;
  const auto appendProfileToImporter =
      [&nodes](const auto& importer_index,
               const importer::SourceProfile& source_profile,
               std::optional<int> profile_index = std::nullopt) {
        vivaldi::import_data::UserProfileItem profile;
        profile.profile_name = profile.profile_display_name =
            base::UTF16ToUTF8(source_profile.profile);
        profile.profile_path = toSystemUTF(source_profile.source_path.value());
        profile.index = profile_index;

        if (!profile.profile_name.empty() && !profile.profile_path->empty()) {
          nodes.at(importer_index)
              .user_profiles.emplace_back(std::move(profile));
        }
      };

  std::map<std::string, int> importersNamesLUT;
  for (size_t i = 0; i < api_importer_list_->count(); ++i) {
    const importer::SourceProfile& source_profile =
        api_importer_list_->GetSourceProfileAt(i);

    // NOTE(konrad@vivaldi.com): VB-76639 To distinguish firefox profiles
    const auto importerName = base::UTF16ToUTF8(source_profile.importer_name);
    if (auto it = importersNamesLUT.find(importerName);
        it != importersNamesLUT.end()) {
      appendProfileToImporter(it->second, source_profile, i);
      continue;
    }
    nodes.emplace_back();
    vivaldi::import_data::ProfileItem* profile = &nodes.back();

    uint16_t browser_services = source_profile.services_supported;

    profile->name = importerName;
    profile->index = i;
    importersNamesLUT.emplace(profile->name, profile->index);

    profile->history = ((browser_services & importer::HISTORY) != 0);
    profile->favorites = ((browser_services & importer::FAVORITES) != 0);
    profile->passwords = ((browser_services & importer::PASSWORDS) != 0);
    profile->supports_master_password =
        ((browser_services & importer::MASTER_PASSWORD) != 0);
    profile->notes = ((browser_services & importer::NOTES) != 0);
    profile->speeddial = ((browser_services & importer::SPEED_DIAL) != 0);
    profile->email = ((browser_services & importer::EMAIL) != 0);
    profile->contacts = ((browser_services & importer::CONTACTS) != 0);
    profile->extensions = ((browser_services & importer::EXTENSIONS) != 0);
    profile->tabs = ((browser_services & importer::TABS) != 0);

    profile->import_type = MapImportType(source_profile.importer_type);

    profile->mail_path = toSystemUTF(source_profile.mail_path.value());
    profile->has_default_install = !source_profile.source_path.empty();

    if (profile->has_default_install) {
      profile->detected_profile_path = toSystemUTF(source_profile.source_path.value());
    } else {
      // To be able to detect Safari, first we need to
      // obtain a permission from the user.
      profile->requires_access_permission =
          profile->import_type == ImportTypes::kSafari;

      profile->requires_interactive_import = true;
      profile->suggested_profile_path =
          MapSuggestedProfilePath(profile->import_type);
    }
    if (source_profile.importer_type == importer::TYPE_BOOKMARKS_FILE) {
      profile->dialog_type = "file";
    } else {
      profile->dialog_type = "folder";
    }

    std::vector<vivaldi::import_data::UserProfileItem> profileItems;

    for (size_t j = 0; j < source_profile.user_profile_names.size(); ++j) {
      profileItems.emplace_back();
      vivaldi::import_data::UserProfileItem* profItem = &profileItems.back();

      profItem->profile_display_name = base::UTF16ToUTF8(
          source_profile.user_profile_names.at(j).profileDisplayName);
      profItem->profile_name =
          source_profile.user_profile_names.at(j).profileName;
      profItem->index = profile->index;
    }

    profile->user_profiles = std::move(profileItems);

    // NOTE(konrad@vivaldi.com): VB-76639 To distinguish firefox profiles
    appendProfileToImporter(profile->index, source_profile);
  }

  Respond(ArgumentList(Results::Create(nodes)));
}

ExtensionFunction::ResponseAction ImportDataGetProfilesFunction::Run() {
  ProfileSingletonFactory* singl = ProfileSingletonFactory::getInstance();
  api_importer_list_ = singl->getInstance()->getImporterList();

  // if (!singl->getProfileRequested() &&
  // !api_importer_list->count()){
  singl->setProfileRequested(true);
  api_importer_list_->DetectSourceProfiles(
      g_browser_process->GetApplicationLocale(), true,
      base::BindOnce(&ImportDataGetProfilesFunction::Finished, this));
  return RespondLater();
  // }
}

ImportDataStartImportFunction::ImportDataStartImportFunction() {}

ImportDataStartImportFunction::~ImportDataStartImportFunction() {
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

// ExtensionFunction:
ExtensionFunction::ResponseAction ImportDataStartImportFunction::Run() {
  using vivaldi::import_data::StartImport::Params;
  namespace Results = vivaldi::import_data::StartImport::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int browser_index = params->profile_index;

  ImporterList* api_importer_list =
      ProfileSingletonFactory::getInstance()->getInstance()->getImporterList();

  importer::SourceProfile source_profile =
      api_importer_list->GetSourceProfileAt(browser_index);
  int supported_items = source_profile.services_supported;
  int selected_items = importer::NONE;
  importer_type_ = source_profile.importer_type;

  if (params->types_to_import.history) {
    selected_items |= importer::HISTORY;
  }
  if (params->types_to_import.favorites) {
    selected_items |= importer::FAVORITES;
  }
  if (params->types_to_import.passwords) {
    selected_items |= importer::PASSWORDS;
  }
  if (params->types_to_import.notes) {
    selected_items |= importer::NOTES;
  }
  if (params->types_to_import.speeddial) {
    selected_items |= importer::SPEED_DIAL;
  }
  if (params->types_to_import.extensions) {
    selected_items |= importer::EXTENSIONS;
  }
  if (params->types_to_import.tabs) {
    selected_items |= importer::TABS;
  }

  imported_items_ = (selected_items & supported_items);

  source_profile.selected_profile_name = params->profile_name;

  if (params->master_password.has_value()) {
    source_profile.master_password = params->master_password.value();
  }

  std::u16string dialog_title;
  if (importer_type_ == importer::TYPE_BOOKMARKS_FILE ||
      ((importer_type_ == importer::TYPE_OPERA ||
        importer_type_ == importer::TYPE_EDGE_CHROMIUM ||
        importer_type_ == importer::TYPE_BRAVE ||
        importer_type_ == importer::TYPE_VIVALDI) &&
       !params->ask_user_for_file_location)) {
    base::FilePath import_path =
        base::FilePath::FromUTF8Unsafe(params->import_path->c_str());
    source_profile.source_path = import_path;
    source_profile.importer_type = importer_type_;

    StartImport(source_profile);
    return RespondNow(NoArguments());
  } else {
    if (imported_items_) {
      StartImport(source_profile);
    } else {
      LOG(WARNING) << "There were no settings to import from '"
                   << source_profile.importer_name << "'.";
    }
    return RespondNow(NoArguments());
  }
}

void ImportDataStartImportFunction::StartImport(
    const importer::SourceProfile& source_profile) {
  ImportDataAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->StartImport(source_profile, imported_items_);
}

ExtensionFunction::ResponseAction
ImportDataOpenThunderbirdMailboxFunction::Run() {
  namespace Results = vivaldi::import_data::OpenThunderbirdMailbox::Results;
  using vivaldi::import_data::OpenThunderbirdMailbox::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::streampos fsize;
  std::string seekpos_str = *params->seek_position;

  int64_t seekpos;
  base::StringToInt64(seekpos_str, &seekpos);

  bool success = ImportDataAPI::GetFactoryInstance()
                     ->Get(browser_context())
                     ->OpenThunderbirdMailbox(params->path, seekpos, fsize);

  if (!success) {
    return RespondNow(Error(
        base::StringPrintf("Couldn't open file %s", params->path.c_str())));
  }
  std::string fsize_serialized = base::NumberToString(fsize);
  return RespondNow(ArgumentList(Results::Create(fsize_serialized)));
}

ExtensionFunction::ResponseAction
ImportDataCloseThunderbirdMailboxFunction::Run() {
  ImportDataAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->CloseThunderbirdMailbox();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ImportDataReadMessageFromThunderbirdMailboxFunction::Run() {
  namespace Results =
      vivaldi::import_data::ReadMessageFromThunderbirdMailbox::Results;
  std::string content;
  std::streampos seek_pos;
  bool success = ImportDataAPI::GetFactoryInstance()
                     ->Get(browser_context())
                     ->ReadThunderbirdMessage(content, seek_pos);
  if (!success) {
    std::string path = ImportDataAPI::GetFactoryInstance()
                           ->Get(browser_context())
                           ->GetThunderbirdPath();
    if (path.empty()) {
      return RespondNow(Error(base::StringPrintf("Mailbox not open.")));
    } else {
      return RespondNow(
          Error(base::StringPrintf("Couldn't read file %s", path.c_str())));
    }
  }
  std::string seek_pos_serialized = base::NumberToString(seek_pos);
  return RespondNow(
      ArgumentList(Results::Create(content, seek_pos_serialized)));
}

}  // namespace extensions
