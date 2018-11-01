// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "ui/webgui/notes_ui.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

namespace vivaldi {

NotesUIHTMLSource::NotesUIHTMLSource() {}

std::string NotesUIHTMLSource::GetSource() const {
  return "notes";
}

void NotesUIHTMLSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const GotDataCallback& callback) {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";

  callback.Run(NULL);
}

std::string NotesUIHTMLSource::GetMimeType(const std::string& path) const {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";
  return "text/html";
}

NotesUIHTMLSource::~NotesUIHTMLSource() {}

NotesUI::NotesUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  NotesUIHTMLSource* html_source = new NotesUIHTMLSource();

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source);
}
}  // namespace vivaldi
