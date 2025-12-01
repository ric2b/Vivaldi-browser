// Copyright (c) 2018-2021 Vivaldi Technologies AS. All rights reserved.

#include "app/vivaldi_apptools.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_child_frame.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/content/vivaldi_tab_check.h"

namespace content {

void WebContentsImpl::SetVivExtData(const std::string& viv_ext_data) {
  std::optional<base::Value> old_json_data =
      base::JSONReader::Read(viv_ext_data_, base::JSON_PARSE_RFC);
  std::optional<base::Value> new_json_data =
      base::JSONReader::Read(viv_ext_data, base::JSON_PARSE_RFC);

  base::Value::Dict merged_dict;
  if (old_json_data && old_json_data->is_dict()) {
    merged_dict = std::move(old_json_data->GetDict());
  }

  bool changed = false;
  if (new_json_data && new_json_data->is_dict()) {
    const base::Value::Dict& new_dict = new_json_data->GetDict();

    for (const auto [key, new_val] : new_dict) {
      const base::Value* old_val = merged_dict.Find(key);

      if (new_val.is_none()) {
        changed |= merged_dict.Remove(key);
      } else if (!old_val || *old_val != new_val) {
        merged_dict.Set(key, new_val.Clone());
        changed = true;
      }
    }

    if (changed) {
      std::string result_json;
      base::JSONWriter::Write(merged_dict, &result_json);
      viv_ext_data_ = std::move(result_json);
      observers_.NotifyObservers(&WebContentsObserver::VivExtDataSet, this);
    }
  }
  // NOTE(konrad@vivaldi.com): VB-119566 Strangely enough, we have to notify
  // SessionService about all VivExtData calls to prevent panels from appearing
  // as regular tabs.
  vivaldi::GetExtDataUpdatedCallbackList().Notify(this);
}

void WebContentsImpl::SetIgnoreLinkRouting(const bool ignore_link_routing) {
  ignore_link_routing_ = ignore_link_routing;
}

const std::string& WebContentsImpl::GetVivExtData() const {
  return viv_ext_data_;
}

bool WebContentsImpl::GetIgnoreLinkRouting() const {
  return ignore_link_routing_;
}


void WebContentsImpl::SetResumePending(bool resume) {
  is_resume_pending_ = resume;
}

// Loop through all web contents and check if it cointains the point.
// Returns true if the point is only contained by the UI content.
bool WebContentsImpl::IsVivaldiUI(const gfx::Point& point) {
  bool uiContainsPoint = false;

  if (this->GetVisibility() == Visibility::VISIBLE &&
      this->GetViewBounds().Contains(point)) {
    if (vivaldi::IsVivaldiUrl(this->GetVisibleURL().spec())) {
      uiContainsPoint = true;
    } else {
      return false;
    }
  }

  std::vector<WebContentsImpl*> relevant_contents(1, this);
  for (size_t i = 0; i != relevant_contents.size(); ++i) {
    for (auto* inner : relevant_contents[i]->GetInnerWebContents()) {

      if (inner->GetVisibility() == Visibility::VISIBLE &&
          inner->GetViewBounds().Contains(point)) {
        if (vivaldi::IsVivaldiUrl(inner->GetVisibleURL().spec())) {
          uiContainsPoint = true;
        }
        if (!vivaldi::IsVivaldiUrl(inner->GetVisibleURL().spec())) {
          return false;
        }
        relevant_contents.push_back(static_cast<WebContentsImpl*>(inner));
      }
    }
  }

  return uiContainsPoint;
}

}  // namespace content
