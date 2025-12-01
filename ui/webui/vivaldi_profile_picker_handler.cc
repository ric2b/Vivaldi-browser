// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "ui/webui/vivaldi_profile_picker_handler.h"
#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/pref_names.h"
#include "chromium/chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace {
std::optional<int> GetCallbackId(const base::Value::List& args) {
  if (args.size() == 0) {
    return std::nullopt;
  }

  return args[0].GetIfInt();
}

const char * kMissingCallbacIdMessage = "missing callback_id";

}  // namespace

void VivaldiProfilePickerHandler::SendResponse(int callback_id,
                                               base::Value&& response) {
  base::Value::Dict dict;
  dict.Set("callbackId", callback_id);
  dict.Set("response", std::move(response));
  FireWebUIListener("vivaldi-ui-response", dict);
}

void VivaldiProfilePickerHandler::SendErrorResponse(
    int callback_id,
    const std::string& message) {
  base::Value::Dict dict;
  dict.Set("callbackId", callback_id);
  dict.Set("error", message);
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
      base::BindRepeating(&VivaldiProfilePickerHandler::HandlePickPrifile,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setShowOnStartup",
      base::BindRepeating(&VivaldiProfilePickerHandler::HandleSetShowOnStartup,
                          base::Unretained(this)));
}

void VivaldiProfilePickerHandler::HandleSetShowOnStartup(
    const base::Value::List& args) {
  AllowJavascript();
  auto callback_id = GetCallbackId(args);
  if (!callback_id) {
    LOG(ERROR) << kMissingCallbacIdMessage;
    return;
  }

  if (args.size() != 2) {
    SendErrorResponse(*callback_id, "invalid args");
    return;
  }

  auto* arg_dict = args[1].GetIfDict();
  if (!arg_dict) {
    SendErrorResponse(*callback_id, "invalid args[1]");
    return;
  }

  auto value = arg_dict->FindBool("value");
  if (!value) {
    SendErrorResponse(*callback_id, "invalid args[1].value");
    return;
  }

 g_browser_process->local_state()->SetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup, *value);

  SendResponse(*callback_id, base::Value());
}

void VivaldiProfilePickerHandler::HandleGetProfilesInfo(
    const base::Value::List& args) {
  AllowJavascript();

  auto callback_id = GetCallbackId(args);
  if (!callback_id) {
    LOG(ERROR) << kMissingCallbacIdMessage;
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    SendErrorResponse(*callback_id, "profile manager not initialized");
    return;
  }

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  auto attributes = storage.GetAllProfilesAttributesSortedForDisplay();
  base::Value::List profiles_list;
  for (auto* attr : attributes) {
    base::Value::Dict profile;
    profile.Set("name", attr->GetName());
    auto avatar = attr->GetAvatarIcon();
    profile.Set("avatar", ConvertImageToBase64DataURL(avatar));
    profile.Set("path", base::FilePathToValue(attr->GetPath()));
    profiles_list.Append(std::move(profile));
  }

  base::Value::Dict result;

  auto* web_contents = web_ui()->GetWebContents();
  if (web_contents) {
    // chrome://profile-picker is open as a regular page. We may want to
    // handle the current profile differently.
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (profile) {
      result.Set("currentProfilePath",
                 base::FilePathToValue(profile->GetPath()));
    }
  }

  result.Set("profiles", std::move(profiles_list));
  result.Set("showOnStartup", g_browser_process->local_state()->GetBoolean(
                                  prefs::kBrowserShowProfilePickerOnStartup));
  SendResponse(*callback_id, base::Value(std::move(result)));
}

void VivaldiProfilePickerHandler::HandlePickPrifile(
    const base::Value::List& args) {
  AllowJavascript();
  auto callback_id = GetCallbackId(args);
  if (!callback_id) {
    LOG(ERROR) << kMissingCallbacIdMessage;
    return;
  }

  if (args.size() != 2) {
    SendErrorResponse(*callback_id, "invalid args");
    return;
  }

  auto* profile_args = args[1].GetIfDict();
  if (!profile_args) {
    SendErrorResponse(*callback_id, "args[1] not a dict");
    return;
  }

  bool guest = false;
  std::optional<base::FilePath> profile_path;
  const base::Value* path = profile_args->Find("path");

  if (!path) {
    guest = true;
    profile_path = ProfileManager::GetGuestProfilePath();
  } else {
    profile_path = base::ValueToFilePath(*path);
  }

  if (!profile_path) {
    SendErrorResponse(*callback_id, "invalid profile path");
    return;
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
      SendErrorResponse(*callback_id, "signing required (not supported)");
      return;
    }
  }

  if (!ProfilePicker::IsOpen()) {
    // We can use chrome://profile-picker as a regular page.
    profiles::SwitchToProfile(*profile_path, false);
    SendResponse(*callback_id, base::Value());
    return;
  }

  ProfilePicker::PickProfile(
      *profile_path,
      ProfilePicker::ProfilePickingArgs{
          .open_settings = false, .should_record_startup_metrics = false});

  SendResponse(*callback_id, base::Value());
}
