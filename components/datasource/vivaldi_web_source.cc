// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_web_source.h"

#include <stddef.h>
#include <string>

#include "app/vivaldi_constants.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/strcat.h"
#include "chrome/common/url_constants.h"
#include "net/url_request/url_request.h"

namespace {
const char kStartpageType[] = "startpage";
const char kHtmlHeader[] = "<!DOCTYPE html>\n<html>\n<head>\n";
const char kHtmlTitle[] = "<title>Start Page</title>\n";
const char kHtmlStyleStart[] = "<style type=\"text/css\">\n";
const char kHtmlStyleEnd[] = "</style>\n";
const char kHtmlBody[] = "</head>\n<body>\n";
const char kHtmlFooter[] = "</body>\n</html>\n";
}

VivaldiWebSource::VivaldiWebSource(Profile* profile)
    : weak_ptr_factory_(this) {
}

VivaldiWebSource::~VivaldiWebSource() {
}

std::string VivaldiWebSource::GetSource() const {
  return vivaldi::kVivaldiWebUIHost;
}

const char kBackgroundColorCss[] = "background-color";

void VivaldiWebSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string type;
  std::string data;
  std::string color;

  ExtractRequestTypeAndData(path, type, data);

  if (type == kStartpageType) {
    // Takes urls of this format:
    // chrome://vivaldi-webui/startpage?section=bookmarks&background-color=#AABBCC
    std::vector<std::string> out;
    out.push_back(kHtmlHeader);
    out.push_back(kHtmlTitle);
    size_t pos = data.find(kBackgroundColorCss);
    if (pos != std::string::npos) {
      color = data.substr(sizeof(kBackgroundColorCss) + pos);
    }
    if (color.size()) {
      out.push_back(kHtmlStyleStart);
      std::string css = "body { background-color: " + color + ";}";
      out.push_back(css);
      out.push_back(kHtmlStyleEnd);
    }
    out.push_back(kHtmlBody);
    out.push_back(kHtmlFooter);

    std::string out_html = base::StrCat(out);
    callback.Run(base::RefCountedString::TakeString(&out_html));
  } else {
    NOTREACHED();
  }
}

// In a url such as chrome://vivaldi-data/desktop-image/0 type is
// "desktop-image" and data is "0".
void VivaldiWebSource::ExtractRequestTypeAndData(const std::string& path,
                                                  std::string& type,
                                                  std::string& data) {
  std::string page_url_str(path);

  size_t pos = path.find('/');
  if (pos != std::string::npos) {
    type = path.substr(0, pos);
    data = path.substr(pos+1);
  } else {
    pos = path.find('?');
    if (pos != std::string::npos) {
      type = path.substr(0, pos);
      data = path.substr(pos + 1);
    } else {
      type = path;
    }
  }
}

std::string VivaldiWebSource::GetMimeType(const std::string&) const {
  return "text/html";
}

bool VivaldiWebSource::AllowCaching() const {
  return false;
}

bool VivaldiWebSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) const {
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
}
