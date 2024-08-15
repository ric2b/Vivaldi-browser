// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "installer/win/vivaldi_install_l10n.h"

#include <Windows.h>

#include <algorithm>

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/win/i18n.h"
#include "chrome/installer/util/google_update_constants.h"

#include "installer/util/vivaldi_install_util.h"

namespace vivaldi {

namespace {

struct LanguageState {
  InstallerLanguageOffsets language_offsets;

  // We need to change the selector, but it does not support assignments, so go
  // via Optional and explicit emplace.
  std::optional<base::win::i18n::LanguageSelector> selector;
  std::wstring language_code;
};

LanguageState& GetState() {
  static base::NoDestructor<LanguageState> instance;
  return *instance;
}

std::wstring ReadInstallerRegistryLanguage() {
  std::wstring language_code = ReadRegistryString(
      google_update::kRegLangField,
      OpenRegistryKeyToRead(HKEY_CURRENT_USER,
                            vivaldi::constants::kVivaldiKey));
  return language_code;
}

}  // namespace

void NormalizeLanguageCode(std::wstring& code) {
  // The language variant part should be separated by the dash, not underscore.
  size_t underscore = code.find(L'_');
  if (underscore != std::wstring::npos) {
    code[underscore] = L'-';
  }
  // The main language ISO code should use the lower case.
  size_t dash = code.find(L'-');
  size_t language_end = (dash != std::wstring::npos) ? dash : code.length();
  for (size_t i = 0; i < language_end; ++i) {
    code[i] = base::ToLowerASCII(code[i]);
  }
  // The variant part should be be either two upper case letters if this is a c
  // country variant, or use the Name case for longer non-country names.
  if (dash != std::wstring::npos && dash + 3 <= code.length()) {
    code[dash + 1] = base::ToUpperASCII(code[dash + 1]);
    if (dash + 3 == code.length()) {
      // The variant is country.
      code[dash + 2] = base::ToUpperASCII(code[dash + 2]);
    } else {
      for (size_t i = dash + 2; i < code.length(); ++i) {
        code[i] = base::ToLowerASCII(code[i]);
      }
    }
  }
  // Fix the wrong name for Norsk Bokmål that Google Grit and related tools use.
  if (code == L"no") {
    code = L"nb";
  }
}

void WriteInstallerRegistryLanguage() {
  std::wstring language_code = GetInstallerLanguage();
  base::win::RegKey key = OpenRegistryKeyToWrite(
      HKEY_CURRENT_USER, vivaldi::constants::kVivaldiKey);
  WriteRegistryString(google_update::kRegLangField, language_code, key);
}

void InitInstallerLanguage(InstallerLanguageOffsets language_offsets,
                           std::wstring (*get_default_language)()) {
  LanguageState& state = GetState();
  DCHECK(!language_offsets.empty());
  DCHECK(state.language_offsets.empty());
  DCHECK(!state.selector);
  state.language_offsets = language_offsets;
  std::wstring language_code =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          vivaldi::constants::kVivaldiLanguage);
  if (language_code.empty() && get_default_language) {
    language_code = get_default_language();
  }
  if (language_code.empty()) {
    language_code = ReadInstallerRegistryLanguage();
  }
  SetInstallerLanguage(std::move(language_code));
}

void SetInstallerLanguage(std::wstring language_code) {
  LanguageState& state = GetState();
  std::vector<std::wstring> candidates;
  if (!language_code.empty()) {
    // An explicit language from a command line or registry overrides any system
    // preferences.
    NormalizeLanguageCode(language_code);
    candidates.push_back(language_code);
  } else {
    base::win::i18n::GetThreadPreferredUILanguageList(&candidates);
  }
  state.selector.emplace(candidates, state.language_offsets);
  if (language_code.empty()) {
    // The selector returns an internal all lower case form. So normalize it.
    language_code = state.selector->selected_translation();
    DCHECK(!language_code.empty());
    NormalizeLanguageCode(language_code);
  }
  state.language_code = std::move(language_code);

  DLOG(INFO) << "language_code=" << state.language_code
             << " selected_translation="
             << state.selector->selected_translation();
}

const std::wstring& GetInstallerLanguage() {
  DCHECK(GetState().selector);
  return GetState().language_code;
}

const base::win::i18n::LanguageSelector* GetInstallerLanguageSelector() {
  LanguageState& state = GetState();
  return state.selector ? &*state.selector : nullptr;
}

}  // namespace vivaldi

namespace vivaldi_installer {

std::wstring GetLocalizedStringF(int message_id,
                                  const std::wstring& arg) {
  std::wstring text = ::installer::GetLocalizedString(message_id);
  return base::ReplaceStringPlaceholders(text, {arg}, nullptr);
}

std::wstring GetLocalizedStringF2(int message_id,
                                  const std::wstring& arg1,
                                  const std::wstring& arg2) {
  std::wstring text = ::installer::GetLocalizedString(message_id);
  return base::ReplaceStringPlaceholders(text, {arg1, arg2}, nullptr);
}

}  // namespace installer
