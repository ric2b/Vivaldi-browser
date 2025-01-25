// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_MODEL_H_
#define ASH_PICKER_MODEL_PICKER_MODEL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"

class PrefService;

namespace ui {
class TextInputClient;
}

namespace ash {

namespace input_method {
class ImeKeyboard;
}

enum class PickerModeType;

class ASH_EXPORT PickerModel {
 public:
  enum class EditorStatus { kEnabled, kDisabled };

  // `focused_client` is the input field that was focused when Picker is opened.
  // It can be null. `ime_keyboard` is used to monitor caps lock state. This
  // cannot be null.
  explicit PickerModel(ui::TextInputClient* focused_client,
                       input_method::ImeKeyboard* ime_keyboard,
                       EditorStatus editor_status);

  std::vector<PickerCategory> GetAvailableCategories() const;

  std::vector<PickerCategory> GetRecentResultsCategories() const;

  std::u16string_view selected_text() const;

  bool is_caps_lock_enabled() const;

  PickerModeType GetMode() const;

  bool IsGifsEnabled(PrefService* prefs) const;

 private:
  bool has_focus_;
  std::u16string selected_text_;
  gfx::Range selection_range_;
  bool is_caps_lock_enabled_;
  EditorStatus editor_status_;
  ui::TextInputType text_input_type_;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_MODEL_H_
