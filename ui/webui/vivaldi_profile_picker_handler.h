// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_HANDLER_H_
#define UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

class VivaldiProfilePickerHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override;

  void HandleGetProfilesInfo(const base::Value::List& args);
  void HandlePickPrifile(const base::Value::List& args);
  void HandleSetShowOnStartup(const base::Value::List& args);

 private:
  void SendResponse(int callback_id, base::Value&& response);
  void SendErrorResponse(int callback_id, const std::string& message);
};

#endif // UI_PROFILE_PICKER_VIVALDI_PROFILE_PICKER_HANDLER_H_
