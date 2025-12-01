// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

#include "content/public/browser/web_contents.h"
#include "components/tabs/tab_helpers.h"

using content::WebContents;

namespace vivaldi {

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
  base::Value::Dict dict;
  if (GetTabWorkspaceId(contents->GetVivExtData()) == workspace_id) {
    // There is nothing to change.
    return false;
  }

  dict.Set(kVivaldiWorkspace, workspace_id);

  // NOTE(konrad@vivaldi.com): The tab cannot stay in the same group (tab stack)
  // after changing workspace.
  dict.Set("group", base::Value());
  dict.Set("groupColor", base::Value());
  dict.Set("fixedGroupTitle", base::Value());

  std::string json_string;
  base::JSONWriter::Write(dict, &json_string);

  contents->SetVivExtData(json_string);
  return true;
}
}  // namespace vivaldi
