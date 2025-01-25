// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace chromeos::editor_menu {
enum class PresetQueryCategory;
}

namespace ash {

struct ASH_PUBLIC_EXPORT PickerTextResult {
  enum class Source {
    kUnknown,  // This should only be used for tests.
    kDate,
    kMath,
    kCaseTransform,
    kOmnibox,
  };

  std::u16string primary_text;
  std::u16string secondary_text;
  ui::ImageModel icon;
  Source source;

  explicit PickerTextResult(
      std::u16string_view text,
      PickerTextResult::Source source = PickerTextResult::Source::kUnknown);
  explicit PickerTextResult(std::u16string_view primary_text,
                            std::u16string_view secondary_text,
                            ui::ImageModel icon,
                            Source source);
  PickerTextResult(const PickerTextResult&);
  PickerTextResult& operator=(const PickerTextResult&);
  ~PickerTextResult();

  bool operator==(const PickerTextResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerSearchRequestResult {
  std::u16string primary_text;
  std::u16string secondary_text;
  ui::ImageModel icon;

  explicit PickerSearchRequestResult(std::u16string_view primary_text,
                                     std::u16string_view secondary_text,
                                     ui::ImageModel icon);
  PickerSearchRequestResult(const PickerSearchRequestResult&);
  PickerSearchRequestResult& operator=(const PickerSearchRequestResult&);
  ~PickerSearchRequestResult();

  bool operator==(const PickerSearchRequestResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerEmojiResult {
  enum class Type { kEmoji, kSymbol, kEmoticon };

  Type type;
  std::u16string text;
  std::u16string name;

  static PickerEmojiResult Emoji(std::u16string_view text,
                                 std::u16string name = u"");
  static PickerEmojiResult Symbol(std::u16string_view text,
                                  std::u16string name = u"");
  static PickerEmojiResult Emoticon(std::u16string_view text,
                                    std::u16string name = u"");

  explicit PickerEmojiResult(Type type,
                             std::u16string_view text,
                             std::u16string name);
  PickerEmojiResult(const PickerEmojiResult&);
  PickerEmojiResult& operator=(const PickerEmojiResult&);
  ~PickerEmojiResult();

  bool operator==(const PickerEmojiResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerClipboardResult {
  enum class DisplayFormat {
    kFile,
    kText,
    kImage,
    kHtml,
    kUrl,
  };

  // Unique ID that specifies which item in the clipboard this refers to.
  base::UnguessableToken item_id;
  DisplayFormat display_format;
  // If this is 1, `display_text` should be the filename of the file.
  size_t file_count;
  std::u16string display_text;
  std::optional<ui::ImageModel> display_image;
  bool is_recent;

  explicit PickerClipboardResult(base::UnguessableToken item_id,
                                 DisplayFormat display_format,
                                 size_t file_count,
                                 std::u16string display_text,
                                 std::optional<ui::ImageModel> display_image,
                                 bool is_recent);
  PickerClipboardResult(const PickerClipboardResult&);
  PickerClipboardResult& operator=(const PickerClipboardResult&);
  ~PickerClipboardResult();

  bool operator==(const PickerClipboardResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerBrowsingHistoryResult {
  GURL url;
  std::u16string title;
  ui::ImageModel icon;
  bool best_match;

  explicit PickerBrowsingHistoryResult(GURL url,
                                       std::u16string title,
                                       ui::ImageModel icon,
                                       bool best_match = false);
  PickerBrowsingHistoryResult(const PickerBrowsingHistoryResult&);
  PickerBrowsingHistoryResult& operator=(const PickerBrowsingHistoryResult&);
  ~PickerBrowsingHistoryResult();

  bool operator==(const PickerBrowsingHistoryResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerLocalFileResult {
  std::u16string title;
  base::FilePath file_path;
  bool best_match;

  explicit PickerLocalFileResult(std::u16string title,
                                 base::FilePath file_path,
                                 bool best_match = false);
  PickerLocalFileResult(const PickerLocalFileResult&);
  PickerLocalFileResult& operator=(const PickerLocalFileResult&);
  ~PickerLocalFileResult();

  bool operator==(const PickerLocalFileResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerDriveFileResult {
  std::optional<std::string> id;
  std::u16string title;
  GURL url;
  base::FilePath file_path;
  bool best_match;

  explicit PickerDriveFileResult(std::optional<std::string> id,
                                 std::u16string title,
                                 GURL url,
                                 base::FilePath file_path,
                                 bool best_match = false);
  PickerDriveFileResult(const PickerDriveFileResult&);
  PickerDriveFileResult& operator=(const PickerDriveFileResult&);
  ~PickerDriveFileResult();

  bool operator==(const PickerDriveFileResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerCategoryResult {
  PickerCategory category;

  explicit PickerCategoryResult(PickerCategory category);
  PickerCategoryResult(const PickerCategoryResult&);
  PickerCategoryResult& operator=(const PickerCategoryResult&);
  ~PickerCategoryResult();

  bool operator==(const PickerCategoryResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerEditorResult {
  enum class Mode { kWrite, kRewrite };

  Mode mode;
  std::u16string display_name;
  std::optional<chromeos::editor_menu::PresetQueryCategory> category;
  std::optional<std::string> preset_query_id;

  PickerEditorResult(
      Mode mode,
      std::u16string display_name,
      std::optional<chromeos::editor_menu::PresetQueryCategory> category,
      std::optional<std::string> preset_query_id);
  PickerEditorResult(const PickerEditorResult&);
  PickerEditorResult& operator=(const PickerEditorResult&);
  ~PickerEditorResult();

  bool operator==(const PickerEditorResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerNewWindowResult {
  enum Type {
    kDoc,
    kSheet,
    kSlide,
    kChrome,
  };

  Type type;

  explicit PickerNewWindowResult(Type type);
  PickerNewWindowResult(const PickerNewWindowResult&);
  PickerNewWindowResult& operator=(const PickerNewWindowResult&);
  ~PickerNewWindowResult();

  bool operator==(const PickerNewWindowResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerCapsLockResult {
  enum class Shortcut {
    kAltLauncher,
    kAltSearch,
    kFnRightAlt,
  };

  bool enabled;
  Shortcut shortcut;

  explicit PickerCapsLockResult(bool enabled, Shortcut shortcut);
  PickerCapsLockResult(const PickerCapsLockResult&);
  PickerCapsLockResult& operator=(const PickerCapsLockResult&);
  ~PickerCapsLockResult();

  bool operator==(const PickerCapsLockResult&) const;
};

struct ASH_PUBLIC_EXPORT PickerCaseTransformResult {
  enum Type {
    kUpperCase,
    kLowerCase,
    kTitleCase,
  };

  Type type;

  explicit PickerCaseTransformResult(Type type);
  PickerCaseTransformResult(const PickerCaseTransformResult&);
  PickerCaseTransformResult& operator=(const PickerCaseTransformResult&);
  ~PickerCaseTransformResult();

  bool operator==(const PickerCaseTransformResult&) const;
};

using PickerSearchResult = std::variant<PickerTextResult,
                                        PickerSearchRequestResult,
                                        PickerEmojiResult,
                                        PickerClipboardResult,
                                        PickerBrowsingHistoryResult,
                                        PickerLocalFileResult,
                                        PickerDriveFileResult,
                                        PickerCategoryResult,
                                        PickerEditorResult,
                                        PickerNewWindowResult,
                                        PickerCapsLockResult,
                                        PickerCaseTransformResult>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
