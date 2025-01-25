// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_web_source.h"

#include <stddef.h>
#include <string>

#include "app/vivaldi_constants.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "net/url_request/url_request.h"

namespace {
const char kStartpageType[] = "startpage";
const char kHtmlHeader[] = "<!DOCTYPE html>\n<html>\n<head>\n";
const char kHtmlStyleStart[] = "<style type=\"text/css\">\n";
const char kHtmlStyleEnd[] = "</style>\n";
const char kHtmlBody[] = "</head>\n<body>\n";
const char kHtmlFooter[] = "</body>\n</html>\n";
}  // namespace

VivaldiWebSource::VivaldiWebSource(Profile* profile)
    : weak_ptr_factory_(this) {}

VivaldiWebSource::~VivaldiWebSource() {}

std::string VivaldiWebSource::GetSource() {
  return vivaldi::kVivaldiWebUIHost;
}

const char kBackgroundColorCss[] = "background-color";

void VivaldiWebSource::StartDataRequest(
    const GURL& path,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string type;
  std::string data;
  std::string color;

  ExtractRequestTypeAndData(path, type, data);

  std::vector<std::string> out;
  out.push_back(kHtmlHeader);

  if (type == kStartpageType) {
    // Takes urls of this format:
    // chrome://vivaldi-webui/startpage?section=bookmarks&background-color=#AABBCC
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
  }
  // else just a blank page (should "never" happen)

  out.push_back(kHtmlBody);
  out.push_back(kHtmlFooter);

  std::string out_html = base::StrCat(out);
  scoped_refptr<base::RefCountedMemory> out_html_data(
      new base::RefCountedString(out_html));
  std::move(callback).Run(out_html_data);
}

// In a url such as chrome://vivaldi-data/desktop-image/0 type is
// "desktop-image" and data is "0".
void VivaldiWebSource::ExtractRequestTypeAndData(const GURL& url,
                                                 std::string& type,
                                                 std::string& data) {
  if (url.has_path()) {
    type = url.path();
    if (type[0] == '/') {
      type = type.erase(0, 1);
    }
    data = url.spec();
  } else {
    type = url.GetContent();
  }
}

std::string VivaldiWebSource::GetMimeType(const GURL&) {
  return "text/html";
}

bool VivaldiWebSource::AllowCaching() {
  return false;
}
