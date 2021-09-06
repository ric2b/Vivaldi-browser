// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/update_notifier_utils.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/resource/data_pack.h"

#include <Windows.h>

namespace vivaldi_update_notifier {

namespace {

const wchar_t kPakfileResourceType[] = L"PAKFILE";
const char kFallbackLanguage[] = "en-US";
const char kFallbackTranslationResource[] = "en_us";

struct TranslationState {
  ui::DataPack main_pack{ui::SCALE_FACTOR_100P};
  ui::DataPack fallback_pack{ui::SCALE_FACTOR_100P};
};

TranslationState& GetTranslationState() {
  static base::NoDestructor<TranslationState> instance;
  return *instance;
}

std::string ConvertLanguageToResourceName(std::string language) {
  for (char& c : language) {
    c = base::ToLowerASCII(c);
  }
  if (language.length() >= 4) {
    if (language[2] == '-') {
      language[2] = '_';
    }
    if (language[3] == '-') {
      language[3] = '_';
    }
  }
  return language;
}

base::StringPiece FindPakResource(base::StringPiece language_as_resource_name) {
  std::wstring name = base::UTF8ToWide(language_as_resource_name);
  // If the locale is language_region, try the language form in case we have
  //  a resource for that.
  for (int step = 0; step <= 1; ++step) {
    if (step == 1) {
      if (name.length() < 4)
        break;
      if (name[2] == '_') {
        name.resize(2);
      } else if (name[3] == '_') {
        name.resize(3);
      } else {
        break;
      }
    }
    HRSRC hResource =
        ::FindResource(nullptr, name.c_str(), kPakfileResourceType);
    if (!hResource)
      continue;
    HGLOBAL hData = ::LoadResource(nullptr, hResource);
    if (!hData)
      continue;
    const void* data = ::LockResource(hData);
    if (!data)
      continue;
    size_t data_size = ::SizeofResource(nullptr, hResource);
    if (!data_size)
      continue;
    return base::StringPiece(reinterpret_cast<const char*>(data), data_size);
  }
  return base::StringPiece();
}

// This is based on ResourceBundle::GetLocalizedStringImpl(). We cannot use that
// directly as ResourceBundle cannot be initialized from a Windows resource.
std::wstring FindTranslation(int string_id) {
  TranslationState& state = GetTranslationState();
  base::StringPiece data;
  ui::ResourceHandle::TextEncodingType encoding =
      state.main_pack.GetTextEncodingType();
  if (!state.main_pack.GetStringPiece(string_id, &data) || data.empty()) {
    if (!state.fallback_pack.GetStringPiece(string_id, &data) || data.empty()) {
      NOTREACHED();
    }
    encoding = state.fallback_pack.GetTextEncodingType();
  }

  std::wstring text;
  if (encoding == ui::ResourceHandle::UTF16) {
    text = std::wstring(reinterpret_cast<const wchar_t*>(data.data()),
                        data.length() / 2);
  } else if (encoding == ui::ResourceHandle::UTF8) {
    text = base::UTF8ToWide(data);
  } else {
    // Data pack encodes strings as either UTF8 or UTF16.
    NOTREACHED();
  }
  if (text.empty()) {
    text = L"[[ Unknown text, Please report this! ]]";
  }
  return text;
}

std::wstring GetTranslation(int message_id,
                            const std::vector<std::wstring>& replacements) {
  std::wstring text = FindTranslation(message_id);
  text = base::ReplaceStringPlaceholders(text, replacements, nullptr);
  return text;
}

}  // namespace

std::string InitTranslations(std::string language) {
  base::StringPiece fallback_data =
      FindPakResource(kFallbackTranslationResource);
  CHECK(!fallback_data.empty());

  base::StringPiece data;
  std::string resource_name = ConvertLanguageToResourceName(language);
  if (!resource_name.empty() && resource_name != kFallbackTranslationResource) {
    data = FindPakResource(resource_name);
  }
  if (data.empty()) {
    l10n_util::OverrideLocaleWithUILanguageList();
    language = l10n_util::GetApplicationLocale(std::string());
    resource_name = ConvertLanguageToResourceName(language);
    if (!resource_name.empty() &&
        resource_name != kFallbackTranslationResource) {
      data = FindPakResource(resource_name);
    }
  }
  bool using_fallback = false;
  if (data.empty()) {
    data = fallback_data;
    using_fallback = true;
    language = kFallbackLanguage;
  }

  TranslationState& state = GetTranslationState();
  bool success = state.main_pack.LoadFromBuffer(data);
  CHECK(success) << "resource_name=" << resource_name;
  if (!using_fallback) {
    success = state.fallback_pack.LoadFromBuffer(fallback_data);
    CHECK(success);
  }
  return language;
}

std::wstring GetTranslation(int message_id) {
  return FindTranslation(message_id);
}

std::wstring GetTranslation(int message_id, const std::wstring& arg) {
  std::vector<std::wstring> replacements = {arg};
  return GetTranslation(message_id, replacements);
}

std::wstring GetTranslation(int message_id,
                            const std::wstring& arg1,
                            const std::wstring& arg2) {
  std::vector<std::wstring> replacements = {arg1, arg2};
  return GetTranslation(message_id, replacements);
}

}  // namespace vivaldi_update_notifier