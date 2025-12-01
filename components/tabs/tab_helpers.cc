// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved


#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"

#include "content/public/browser/web_contents.h"
#include "components/tabs/tab_helpers.h"

using content::WebContents;

namespace vivaldi {

namespace {

  std::optional<base::Value> GetDictValueFromVivExtData(
    std::string& viv_extdata) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> value =
      base::JSONReader::Read(viv_extdata, options);
  if (value && value->is_dict()) {
    return value;
  }
  return std::nullopt;
}

bool ValueToJSONString(const base::Value& value, std::string& json_string) {
  JSONStringValueSerializer serializer(&json_string);
  return serializer.Serialize(value);
}

} // namespace

bool IsTabMuted(const WebContents* web_contents) {
  std::string viv_extdata = web_contents->GetVivExtData();
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
      base::JSONReader::Read(viv_extdata, options);
  std::optional<bool> mute = std::nullopt;
  if (json && json->is_dict()) {
    mute = json->GetDict().FindBool(kVivaldiTabMuted);
  }
  return mute ? *mute : false;
}

bool IsTabInAWorkspace(const WebContents* web_contents) {
  return IsTabInAWorkspace(web_contents->GetVivExtData());
}

bool IsTabInAWorkspace(const std::string& viv_extdata) {
  return GetTabWorkspaceId(viv_extdata).has_value();
}

std::optional<double> GetTabWorkspaceId(const std::string& viv_extdata) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
      base::JSONReader::Read(viv_extdata, options);
  std::optional<double> value;
  if (json && json->is_dict()) {
    value = json->GetDict().FindDouble(kVivaldiWorkspace);
  }
  return value;
}

bool SetTabWorkspaceId(content::WebContents* contents, double workspace_id) {
  auto viv_ext_data = contents->GetVivExtData();
  std::optional<base::Value> json = GetDictValueFromVivExtData(viv_ext_data);
  if (!json) {
    return false;
  }
  std::optional<double> value = json->GetDict().FindDouble(kVivaldiWorkspace);
  if (value.has_value() && value.value() == workspace_id) {
    return false;
  }
  json->GetDict().Set(kVivaldiWorkspace, workspace_id);
  json->GetDict().Remove("group");
  std::string json_string;
  if (!ValueToJSONString(*json, json_string)) {
    return false;
  }
  contents->SetVivExtData(json_string);
  return true;
}
}  // namespace vivaldi
