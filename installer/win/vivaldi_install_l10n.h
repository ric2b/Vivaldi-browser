// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef INSTALLER_WIN_VIVALDI_INSTALL_L10N_H_
#define INSTALLER_WIN_VIVALDI_INSTALL_L10N_H_

#include <string>

#include "base/containers/span.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "chrome/installer/util/l10n_string_util.h"

namespace vivaldi {

using InstallerLanguageOffsets =
    base::span<const base::win::i18n::LanguageSelector::LangToOffset>;

// Make sure that the code use proper cases etc.
void NormalizeLanguageCode(std::wstring& language_code);

void InitInstallerLanguage(InstallerLanguageOffsets language_offsets,
                           std::wstring (*get_default_language)() = nullptr);
void SetInstallerLanguage(std::wstring language_code);
const std::wstring& GetInstallerLanguage();
const base::win::i18n::LanguageSelector* GetInstallerLanguageSelector();
void WriteInstallerRegistryLanguage();

}  // namespace vivaldi

// Add missing 1 and 2-argument entry to chrome/installer/util/l10n_string_util.h

namespace vivaldi_installer {

std::wstring GetLocalizedStringF(int message_id, const std::wstring& arg);

std::wstring GetLocalizedStringF2(int message_id,
                                  const std::wstring& arg1,
                                  const std::wstring& arg2);

}

#endif  // INSTALLER_WIN_VIVALDI_INSTALL_L10N_H_
