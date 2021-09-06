// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_UTILS_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_UTILS_H_

#include <string>

namespace vivaldi_update_notifier {

// Initialize the language support for the given language. If there is no
// language pack for that language, use a close approximation or fallback to a
// supported system language or English. Return the language that is used for
// UI.
std::string InitTranslations(std::string language);

std::wstring GetTranslation(int message_id);
std::wstring GetTranslation(int message_id, const std::wstring& arg);
std::wstring GetTranslation(int message_id,
                            const std::wstring& arg1,
                            const std::wstring& arg2);

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_UTILS_H_
