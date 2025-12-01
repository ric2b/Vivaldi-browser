// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "ui/webui/vivaldi_profile_picker_handler.h"
#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chromium/chrome/browser/browser_process.h"
#include "chromium/chrome/browser/signin/signin_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_pref_names.h"

#include "base/files/file.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "base/task/thread_pool/thread_pool_instance.h"

#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/platform_util.h"
#include "chromium/ui/shell_dialogs/selected_file_info.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"

#include "content/public/browser/web_contents_delegate.h"

// This is defined in nuke_profile_directory_utils.inc
bool IsProfileDirectoryScheduledForDeletion(const base::FilePath& profile_path);

namespace {
std::optional<int> GetCallbackId(const base::Value::List& args) {
  if (args.size() == 0) {
    return std::nullopt;
  }

  return args[0].GetIfInt();
}

const char* kMissingCallbacIdMessage = "missing callback_id";

class Helper {
 public:
  Helper(VivaldiProfilePickerHandler* handler, const base::Value::List& args)
      : args_(args.Clone()), handler_wheak_(handler->GetWeakPtr()) {
    handler->InitByHelper();
    auto callback_id = GetCallbackId(args);
    if (!callback_id) {
      LOG(ERROR) << kMissingCallbacIdMessage;
      return;
    }

    callback_id_ = *callback_id;
  }

  VivaldiProfilePickerHandler* GetHandler() { return handler_wheak_.get(); }

  ~Helper() {
    if (!callback_id_ || resoponse_sent_) {
      return;
    }

    if (!GetHandler()) {
      return;
    }

    if (error_message_) {
      GetHandler()->SendErrorResponse(*callback_id_, *error_message_);
      return;
    }

    // No respose sent so far, sending OK.
    GetHandler()->SendResponse(*callback_id_);
  }

  bool IsValid() const {
    if (!handler_wheak_) {
      return false;
    }
    return callback_id_ && !error_message_;
  }

  const base::Value* GetValue(const std::string& key,
                              size_t argument_index = 1) {
    if (args_.size() <= argument_index) {
      SetErrorMessage("missing argument");
      return nullptr;
    }

    auto* profile_args = args_[argument_index].GetIfDict();
    if (!profile_args) {
      SetErrorMessage("argument is not a dict");
      return nullptr;
    }

    return profile_args->Find(key);
  }

  bool HasPath(size_t argument_index = 1) {
    auto* path = GetValue("path", argument_index);
    return !!path;
  }

  std::optional<base::FilePath> GetPath(size_t argument_index = 1) {
    if (!IsValid()) {
      return std::nullopt;
    }

    auto* path = GetValue("path", argument_index);

    if (!path) {
      SetErrorMessage("path is missing");
      return std::nullopt;
    }

    auto res = base::ValueToFilePath(*path);
    if (!res) {
      SetErrorMessage("invalid path argument");
      return std::nullopt;
    }

    return res;
  }

  std::optional<bool> GetBool(const std::string& key,
                              size_t argument_index = 1) {
    auto* val = GetValue(key, argument_index);
    if (!val) {
      return std::nullopt;
    }

    auto res = val->GetIfBool();
    if (!res) {
      SetErrorMessage("bool expected");
      return std::nullopt;
    }

    return res;
  }

  const std::string* GetString(const std::string& key,
                               size_t argument_index = 1) {
    auto* val = GetValue(key, argument_index);
    if (!val) {
      return nullptr;
    }

    auto* res = val->GetIfString();
    if (!res) {
      SetErrorMessage("string expected");
      return nullptr;
    }

    return res;
  }

  std::optional<int> GetInt(const std::string& key, size_t argument_index = 1) {
    auto* val = GetValue(key, argument_index);
    if (!val) {
      return std::nullopt;
    }

    auto res = val->GetIfInt();
    if (!res) {
      SetErrorMessage("integer expected");
      return std::nullopt;
    }

    return res;
  }

  void SendResponse(base::Value&& response) {
    if (!callback_id_) {
      return;
    }

    if (!GetHandler()) {
      return;
    }

    GetHandler()->SendResponse(*callback_id_, std::move(response));
    resoponse_sent_ = true;
  }

  void SetErrorMessage(const std::string& msg) {
    if (!error_message_) {
      error_message_ = msg;
    }
  }

 private:
  std::optional<int> callback_id_;
  // Don't use reference here since the helper can be passed in BindOnce!
  const base::Value::List args_;
  std::optional<std::string> error_message_;
  bool resoponse_sent_ = false;
  VivaldiProfilePickerHandler::WeakPtr handler_wheak_;
};

using Image = VivaldiProfilePickerHandler::Image;

std::optional<std::string> ReadDataImageBase64(
    const base::FilePath& path,
    std::optional<Image::Error>& error) {
  // 5MB should be enough for any icon
  static constexpr int64_t max_icon_file_size = 5 * 1024 * 1024;

  base::File file(path, base::File::FLAG_READ | base::File::FLAG_OPEN);
  if (!file.IsValid()) {
    error = Image::ERROR_OPEN_FILE;
    return std::nullopt;
  }
  auto len = file.GetLength();
  if (len < 0 || len > max_icon_file_size) {
    error = Image::ERROR_TOO_BIG_FILE;
    return std::nullopt;
  }

  std::vector<unsigned char> buffer;
  buffer.resize(static_cast<size_t>(len));
  int read_len = file.Read(0, reinterpret_cast<char*>(buffer.data()), len);
  if (read_len != len) {
    error = Image::ERROR_READ_FILE;
    return std::nullopt;
  }

  // Very simple check, the file contains an image.
  std::string mime;
  std::string_view sv_bytes(reinterpret_cast<const char*>(buffer.data()),
                            buffer.size());
  if (!net::SniffMimeTypeFromLocalData(sv_bytes, &mime)) {
    error = Image::ERROR_NOT_SUPPORTED_IMAGE_FORMAT;
    return std::nullopt;
  }

  if (!net::MatchesMimeType("image/*", mime)) {
    error = Image::ERROR_NOT_SUPPORTED_IMAGE_FORMAT;
    return std::nullopt;
  }

  std::string image_data;
  std::string_view base64_input(
      reinterpret_cast<const char*>(&buffer.data()[0]), buffer.size());
  image_data = base::Base64Encode(base64_input);
  image_data.insert(0, base::StringPrintf("data:image/png;base64,"));
  return image_data;
}

std::optional<std::string> ReadDataImageBase64(
    const std::string& path,
    std::optional<Image::Error>& error) {
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path);
  return ReadDataImageBase64(file_path, error);
}
}  // namespace

std::optional<std::string> VivaldiProfilePickerHandler::Image::GetErrorString()
    const {
  if (!error) {
    return std::nullopt;
  }
  switch (*error) {
    case ERROR_OPEN_FILE:
      return "open_failed";
    case ERROR_READ_FILE:
      return "read_failed";
    case ERROR_TOO_BIG_FILE:
      return "too_big";
    case ERROR_NOT_SUPPORTED_IMAGE_FORMAT:
      return "not_image";
  }
  return std::nullopt;
}

void VivaldiProfilePickerHandler::SendResponse(int callback_id) {
  base::Value::Dict dict;
  dict.Set("callbackId", callback_id);
  dict.Set("status", "ok");
  FireWebUIListener("vivaldi-ui-response", dict);
}

void VivaldiProfilePickerHandler::SendResponse(int callback_id,
                                               base::Value&& response) {
  base::Value::Dict dict;
  dict.Set("callbackId", callback_id);
  dict.Set("status", "ok");
  dict.Set("response", std::move(response));
  FireWebUIListener("vivaldi-ui-response", dict);
}

void VivaldiProfilePickerHandler::SendErrorResponse(
    int callback_id,
    const std::string& message) {
  base::Value::Dict dict;
  dict.Set("callbackId", callback_id);
  dict.Set("status", "error");
  dict.Set("response", message);
  FireWebUIListener("vivaldi-ui-response", dict);
}

std::string ConvertImageToBase64DataURL(const gfx::Image& image) {
  const SkBitmap* bitmap = image.ToSkBitmap();
  if (!bitmap || bitmap->drawsNothing()) {
    return "";
  }

  auto png_data = gfx::PNGCodec::EncodeBGRASkBitmap(*bitmap, false);

  if (!png_data)
    return "";

  std::string base64_encoded;
  std::span<uint8_t> span(*png_data);
  auto b64 = base::Base64Encode(span);

  return base::StringPrintf("data:image/png;base64,%s", b64.c_str());
}

void VivaldiProfilePickerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfilesInfo",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleGetProfilesInfo,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "pickProfile",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandlePickProfile,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setShowOnStartup",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleSetShowOnStartup,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "deleteProfile",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleDeleteProfile,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "modifyProfile",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleModifyProfile,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "chooseFile",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleChooseFile,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "closeProfilePicker",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleCloseProfilePicker,
                          base::Unretained(this)));
}

bool VivaldiProfilePickerHandler::UseCSD() {
#if BUILDFLAG(IS_MAC)
        // Mac always uses the native decorations, don't show ours!
        return false;
#else
        // The chrome://profile-picker can be open also as tab. In this case,
        // we want the artifical window-bar to be hidden.
        if (!ProfilePicker::IsOpen())
          return false;

        auto* web_contents = web_ui()->GetWebContents();
        if (web_contents) {
          auto * delegate = web_contents->GetDelegate();
          if (delegate) {
            // Vivaldi Frame draws the decorations by itself.
            return delegate->UsesVivaldiFrame();
          }
        }
        // Should not be reached.
        return false;
#endif
}

void VivaldiProfilePickerHandler::HandleSetShowOnStartup(
    const base::Value::List& args) {
  Helper helper(this, args);
  if (!helper.IsValid()) {
    return;
  }

  auto value = helper.GetBool("value");
  if (!value) {
    return;
  }

  g_browser_process->local_state()->SetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup, *value);

  helper.SendResponse(base::Value());
}

VivaldiProfilePickerHandler::WeakPtr VivaldiProfilePickerHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

AvatarIconPicker::~AvatarIconPicker() {
  select_file_dialog_->ListenerDestroyed();
}

AvatarIconPicker::AvatarIconPicker(
    VivaldiProfilePickerHandler& profile_picker,
    AvatarIconPicker::FileSelectedCallback callback)
    : callback_(std::move(callback)) {
  auto* web_contents = profile_picker.web_ui()->GetWebContents();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  ui::SelectFileDialog::FileTypeInfo file_type_info;

  auto owning_window =
      platform_util::GetTopLevel(web_contents->GetNativeView());

  base::FilePath default_path;
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(), default_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
}

void AvatarIconPicker::FileSelected(const ui::SelectedFileInfo& file,
                                    int index) {
  std::move(callback_).Run(file, index);
  delete this;
}

void AvatarIconPicker::FileSelectionCanceled() {
  delete this;
}

void VivaldiProfilePickerHandler::HandleChooseFile(
    const base::Value::List& args) {
  auto helper = std::make_unique<Helper>(this, args);
  if (!helper->IsValid()) {
    return;
  }

  auto callback = base::BindOnce(
      [](std::unique_ptr<Helper> helper, const ui::SelectedFileInfo& file,
         int index) {
        if (!helper->IsValid()) {
          return;
        }

        // Try to read the file as an image.
        ImgMap try_read;
        const std::string filename = file.local_path.AsUTF8Unsafe();
        try_read[filename] = Image();

        auto finish_callback = base::BindOnce(
            [](std::unique_ptr<Helper> helper, std::string filename,
               ImgMap img_map) {
              if (!helper->IsValid()) {
                return;
              }
              base::Value::Dict response;
              auto error_string = img_map[filename].GetErrorString();
              if (error_string) {
                response.Set("error", *error_string);
              }
              response.Set("file", filename);
              helper->SendResponse(base::Value(std::move(response)));
            },
            std::move(helper), filename);

        // Can't IO on this thread.
        base::ThreadPool::PostTask(
            FROM_HERE,
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
            base::BindOnce(&VivaldiProfilePickerHandler::ReadAvatars, try_read,
                           std::move(finish_callback)));
      },
      std::move(helper));
  new AvatarIconPicker(*this, std::move(callback));
}

void VivaldiProfilePickerHandler::ReadAvatars(ImgMap request,
                                              AvatarsReadCallback callback) {
  ImgMap resp;
  for (auto it : request) {
    std::optional<std::string> error;
    auto& item = resp[it.first];
    item.url = ReadDataImageBase64(it.first, item.error);
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(resp)));
}

void VivaldiProfilePickerHandler::HandleGetProfilesInfo(
    const base::Value::List& args) {
  auto helper = std::make_unique<Helper>(this, args);
  if (!helper->IsValid()) {
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  ImgMap custom_icons;
  auto attributes = storage.GetAllProfilesAttributesSortedForDisplay();
  for (auto* attr : attributes) {
    auto custom_icon = vivaldi::GetImagePathFromProfilePath(
        vivaldiprefs::kVivaldiProfileImagePath, attr->GetPath().AsUTF8Unsafe());
    if (!custom_icon.empty()) {
      custom_icons[custom_icon] = Image();
    }
  }

  auto callback = base::BindOnce(
      [](std::unique_ptr<Helper> helper, ImgMap img_map) {
        if (!helper->IsValid()) {
          return;
        }

        auto* self = helper->GetHandler();

        ProfileManager* profile_manager = g_browser_process->profile_manager();

        ProfileAttributesStorage& storage =
            profile_manager->GetProfileAttributesStorage();

        base::Value::List profiles_list;
        Profile* this_profile = nullptr;
        auto* web_contents = self->web_ui()->GetWebContents();
        if (web_contents) {
          // chrome://profile-picker is open as a regular page. We may want to
          // handle the current profile differently.
          this_profile =
              Profile::FromBrowserContext(web_contents->GetBrowserContext());
        }

        auto attributes = storage.GetAllProfilesAttributesSortedForDisplay();
        for (auto* attr : attributes) {
          // When the profile is deleted in the profile picker, it may take
          // a few seconds to be actually deleted as the IO operations are
          // delegated to the IO thread. We want to reload the profile picker
          // page without showing the to-be-deleted profiles.
          if (IsProfileDirectoryScheduledForDeletion(attr->GetPath())) {
            continue;
          }

          bool can_delete = true;

          if (this_profile && this_profile->GetPath() == attr->GetPath()) {
            can_delete = false;
          }

          if (attributes.size() <= 1)
            can_delete = false;

          base::Value::Dict profile;

          auto custom_icon = vivaldi::GetImagePathFromProfilePath(
              vivaldiprefs::kVivaldiProfileImagePath,
              attr->GetPath().AsUTF8Unsafe());

          if (!custom_icon.empty()) {
            auto it = img_map.find(custom_icon);
            if (it != img_map.end()) {
              auto& image = it->second;
              if (image.url) {
                profile.Set("customAvatar", *image.url);
              }
              if (image.error) {
                profile.Set("customAvatarError", *image.error);
              }
            }
          }

          profile.Set("name", attr->GetName());
          auto avatar = attr->GetAvatarIcon();
          profile.Set("avatarIconIndex", int(attr->GetAvatarIconIndex()));
          profile.Set("path", base::FilePathToValue(attr->GetPath()));
          profile.Set("canDelete", can_delete);
          profiles_list.Append(std::move(profile));
        }

        base::Value::Dict result;

        if (this_profile) {
          result.Set("currentProfilePath",
                     base::FilePathToValue(this_profile->GetPath()));
        }

        result.Set("enableCSD", self->UseCSD());
        result.Set("profiles", std::move(profiles_list));
        result.Set("showOnStartup",
                   g_browser_process->local_state()->GetBoolean(
                       prefs::kBrowserShowProfilePickerOnStartup));
        helper->SendResponse(base::Value(std::move(result)));
      },
      std::move(helper));
  // ...callback

  // Read the custom icons on background thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&VivaldiProfilePickerHandler::ReadAvatars, custom_icons,
                     std::move(callback)));
}

void VivaldiProfilePickerHandler::HandlePickProfile(
    const base::Value::List& args) {
  Helper helper(this, args);
  if (!helper.IsValid()) {
    return;
  }

  bool guest = false;
  std::optional<base::FilePath> profile_path;
  if (helper.HasPath()) {
    profile_path = helper.GetPath();
    if (!profile_path) {
      return;
    }
  } else {
    profile_path = ProfileManager::GetGuestProfilePath();
    if (!profile_path) {
      helper.SetErrorMessage("no guest profile");
      return;
    }
    guest = true;
  }

  if (!guest) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(*profile_path);
    if (!entry) {
      NOTREACHED();
    }

    if (entry->IsSigninRequired()) {
      helper.SetErrorMessage("signing required (not supported)");
      return;
    }
  }

  if (!ProfilePicker::IsOpen()) {
    // We can use chrome://profile-picker as a regular page.
    profiles::SwitchToProfile(*profile_path, false);
    helper.SendResponse(base::Value());
    return;
  }

  ProfilePicker::PickProfile(
      *profile_path,
      ProfilePicker::ProfilePickingArgs{.open_settings = false,
                                        .should_record_startup_metrics = false},
      {});

  helper.SendResponse(base::Value());
}

void VivaldiProfilePickerHandler::InitByHelper() {
  AllowJavascript();
}

void VivaldiProfilePickerHandler::HandleDeleteProfile(
    const base::Value::List& args) {
  Helper helper(this, args);
  if (!helper.IsValid()) {
    return;
  }

  std::optional<base::FilePath> profile_path = helper.GetPath();
  if (!profile_path) {
    return;
  }

  Profile* this_profile = Profile::FromWebUI(web_ui());

  if (this_profile && this_profile->GetPath() == *profile_path) {
    // We don't delete the current profile.
    helper.SetErrorMessage("Not allowed");
    return;
  }

  webui::DeleteProfileAtPath(*profile_path,
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
  helper.SendResponse(base::Value());
  return;
}

void VivaldiProfilePickerHandler::HandleModifyProfile(
    const base::Value::List& args) {
  Helper helper(this, args);
  if (!helper.IsValid()) {
    return;
  }

  std::optional<base::FilePath> profile_path = helper.GetPath();
  if (!profile_path) {
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(*profile_path);
  if (!entry) {
    helper.SetErrorMessage("Failet to get profile entry");
    return;
  }

  auto* new_name = helper.GetString("name");
  if (new_name) {
    std::u16string utf16 = base::UTF8ToUTF16(*new_name);
    entry->SetLocalProfileName(utf16, false);
  }

  // avatarIconPath avatarIconIndex
  std::string avatar_icon_path;
  auto* avatar_icon_path_ptr = helper.GetString("avatarIconPath");
  if (avatar_icon_path_ptr) {
    avatar_icon_path = *avatar_icon_path_ptr;
  }
  auto icon_index = helper.GetInt("avatarIconIndex");

  if (icon_index || avatar_icon_path_ptr) {
    if (icon_index) {
      entry->SetAvatarIconIndex(*icon_index);
    }
    vivaldi::SetImagePathForProfilePath(vivaldiprefs::kVivaldiProfileImagePath,
                                        avatar_icon_path,
                                        profile_path->AsUTF8Unsafe());
  }
}

void VivaldiProfilePickerHandler::HandleCloseProfilePicker(
    const base::Value::List& args) {
  Helper helper(this, args);
  if (!helper.IsValid()) {
    return;
  }

  ProfilePicker::Hide();
  helper.SendResponse(base::Value());
}
