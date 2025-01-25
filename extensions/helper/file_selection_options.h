// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_FILE_SELECTION_OPTIONS_H_
#define EXTENSIONS_HELPER_FILE_SELECTION_OPTIONS_H_

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/sessions/core/session_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/shell_dialogs/select_file_dialog.h"

// Helpers to simply the usage of ui::SelectFileDialog when implementing
// extension functions.
class FileSelectionOptions {
 public:
  FileSelectionOptions(SessionID::id_type window_id);
  ~FileSelectionOptions();

  // Consume this, run the file selection dialog and call the callback on file
  // selection.
  using RunDialogResult =
      base::OnceCallback<void(base::FilePath file_path, bool cancelled)>;
  void RunDialog(RunDialogResult callback) &&;

  void SetTitle(std::string_view str);

  void SetType(ui::SelectFileDialog::Type type);

  void SetDefaultPath(std::string_view str);

  // Add extension to file_type_info. The extension must be without the dot.
  void AddExtension(std::string_view extension);

  void SetIncludeAllFiles();

  // Add multiple extensions covering a single type like image.
  void AddExtensions(std::vector<base::FilePath::StringType> extensions);

 private:
  SessionID window_id_;
  std::u16string title_;
  ui::SelectFileDialog::Type type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
  ui::SelectFileDialog::FileTypeInfo file_type_info_;
  base::FilePath default_path_;
};

#endif  // EXTENSIONS_HELPER_VIVALDI_APP_HELPER_H_