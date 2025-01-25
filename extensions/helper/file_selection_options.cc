// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/file_selection_options.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

class FileSelectionRunner : private ui::SelectFileDialog::Listener {
 private:
  friend FileSelectionOptions;
  FileSelectionRunner(FileSelectionOptions::RunDialogResult callback);
  ~FileSelectionRunner() override;
  FileSelectionRunner(const FileSelectionRunner&) = delete;
  FileSelectionRunner& operator=(const FileSelectionRunner&) = delete;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& path,
                    int index) override;
  void FileSelectionCanceled() override;

  FileSelectionOptions::RunDialogResult callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

FileSelectionRunner::FileSelectionRunner(
    FileSelectionOptions::RunDialogResult callback)
    : callback_(std::move(callback)),
      select_file_dialog_(ui::SelectFileDialog::Create(this, nullptr)) {}

FileSelectionRunner::~FileSelectionRunner() {
  select_file_dialog_->ListenerDestroyed();
}

void FileSelectionRunner::FileSelected(const ui::SelectedFileInfo& path,
                                       int index) {
  FileSelectionOptions::RunDialogResult callback = std::move(callback_);
  base::FilePath path_copy = path.file_path;
  delete this;
  std::move(callback).Run(std::move(path_copy), /*cancelled=*/false);
}

void FileSelectionRunner::FileSelectionCanceled() {
  FileSelectionOptions::RunDialogResult callback = std::move(callback_);
  delete this;
  std::move(callback).Run(base::FilePath(), /*cancelled=*/true);
}

}  // namespace

FileSelectionOptions::FileSelectionOptions(SessionID::id_type window_id)
    : window_id_(SessionID::FromSerializedValue(window_id)) {
  if (!window_id_.is_valid()) {
    LOG(ERROR) << "Invalid window id - " + std::to_string(window_id);
  }
}

FileSelectionOptions::~FileSelectionOptions() = default;

void FileSelectionOptions::RunDialog(RunDialogResult callback) && {
  Browser* browser = nullptr;
  if (window_id_.is_valid()) {
    browser = chrome::FindBrowserWithID(window_id_);
    if (!browser) {
      LOG(ERROR) << "No such window - " << window_id_.id();
    }
  }
  if (!browser) {
    // Run the callback later so the caller does not need to deal with
    // synchronous callback calls.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::FilePath(),
                                  /*cancelled=*/false));
    return;
  }

  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  FileSelectionRunner* runner = new FileSelectionRunner(std::move(callback));
  runner->select_file_dialog_->SelectFile(
      type_, title_, default_path_, &file_type_info_, /*file_type_index=*/0,
      base::FilePath::StringType(), window, nullptr);
}

void FileSelectionOptions::SetTitle(std::string_view str) {
  title_ = base::UTF8ToUTF16(str);
}

void FileSelectionOptions::SetType(ui::SelectFileDialog::Type type) {
  type_ = type;
}

void FileSelectionOptions::SetDefaultPath(std::string_view str) {
  default_path_ = base::FilePath::FromUTF8Unsafe(str);
}

void FileSelectionOptions::AddExtension(std::string_view extension) {
  DCHECK(!extension.empty() && extension[0] != '.');

  // FileTypeInfo takes a nested vector like [["htm", "html"], ["txt"]] to
  // group equivalent extensions, but we don't use this feature here.
  std::vector<base::FilePath::StringType> inner_vector;
  inner_vector.push_back(base::FilePath::FromUTF8Unsafe(extension).value());
  file_type_info_.extensions.push_back(std::move(inner_vector));
}

void FileSelectionOptions::AddExtensions(
    std::vector<base::FilePath::StringType> extensions) {
  DCHECK(!extensions.empty());
  file_type_info_.extensions.push_back(std::move(extensions));
}

void FileSelectionOptions::SetIncludeAllFiles() {
  file_type_info_.include_all_files = true;
}
