// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_HANDLER_H_
#define UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_HANDLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class VivaldiProfilePickerHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override;

  void HandleGetProfilesInfo(const base::Value::List& args);
  void HandlePickProfile(const base::Value::List& args);
  void HandleSetShowOnStartup(const base::Value::List& args);
  void HandleDeleteProfile(const base::Value::List& args);
  void HandleModifyProfile(const base::Value::List& args);
  void HandleChooseFile(const base::Value::List& args);

  void SendErrorResponse(int callback_id, const std::string& message);
  void SendResponse(int callback_id, base::Value&& response);
  void SendResponse(int callback_id);
  void InitByHelper();

  using content::WebUIMessageHandler::web_ui;
  using WeakPtr = base::WeakPtr<VivaldiProfilePickerHandler>;

  WeakPtr GetWeakPtr();

  struct Image {
    std::optional<std::string> GetErrorString() const;
    enum Error {
      ERROR_OPEN_FILE = 1,
      ERROR_READ_FILE = 2,
      ERROR_TOO_BIG_FILE = 3,
      ERROR_NOT_SUPPORTED_IMAGE_FORMAT = 4,
    };
    std::optional<std::string> url;
    std::optional<Error> error;
  };

 private:
  using ImgMap = std::map<std::string, Image>;
  using AvatarsReadCallback = base::OnceCallback<void(ImgMap)>;

  static void ReadAvatars(ImgMap request, AvatarsReadCallback callback);
  base::WeakPtrFactory<VivaldiProfilePickerHandler> weak_factory_{this};
};

class AvatarIconPicker : public ui::SelectFileDialog::Listener {
 public:
  using FileSelectedCallback =
      base::OnceCallback<void(const ui::SelectedFileInfo& file, int index)>;

  ~AvatarIconPicker() override;
  AvatarIconPicker(VivaldiProfilePickerHandler& profile_picker,
                   FileSelectedCallback callback);

  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  FileSelectedCallback callback_;
};

#endif  // UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_HANDLER_H_
