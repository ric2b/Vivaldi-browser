// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/sessions/vivaldi_sessions_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_constants.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_service_commands.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/schema/vivaldi_sessions.h"
#include "ui/vivaldi_session_service.h"

using extensions::vivaldi::sessions_private::SessionOpenOptions;

namespace {
enum SessionErrorCodes {
  kNoError,
  kErrorMissingName,
  kErrorWriting,
  kErrorFileMissing,
  kErrorDeleteFailure
};

const base::FilePath::StringType kSessionPath = FILE_PATH_LITERAL("Sessions");
const int kNumberBufferSize = 16;

base::FilePath GenerateFilename(Profile* profile,
                                const std::string session_name,
                                bool unique_name) {
  // PathExists() triggers IO restriction.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string temp_session_name(session_name);
  int cnt = 2;
  char number_string[kNumberBufferSize];

  do {
    base::FilePath path(profile->GetPath());
    path = path.Append(kSessionPath)
#if defined(OS_POSIX)
               .Append(temp_session_name)
#elif defined(OS_WIN)
               .Append(base::UTF8ToWide(temp_session_name))
#endif
               .AddExtension(FILE_PATH_LITERAL(".bin"));
    if (unique_name) {
      if (base::PathExists(path)) {
        temp_session_name.assign(session_name);
        base::snprintf(number_string, kNumberBufferSize, " (%d)", cnt++);
        temp_session_name.append(number_string);
      } else {
        return path;
      }
    } else {
      return path;
    }
    // Just to avoid any endless loop, which is highly unlikely, but still.
  } while (cnt < 1000);

  NOTREACHED();
  return base::FilePath();
}

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction SessionsPrivateSaveOpenTabsFunction::Run() {
  using vivaldi::sessions_private::SaveOpenTabs::Params;
  namespace Results = vivaldi::sessions_private::SaveOpenTabs::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int error_code = SessionErrorCodes::kNoError;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ::vivaldi::VivaldiSessionService service(profile);

  int save_window_id = params->options.save_only_window_id;
  std::vector<int>* ids = params->options.ids.get();

  if (params->name.empty()) {
    error_code = SessionErrorCodes::kErrorMissingName;
  } else {
    base::FilePath path = GenerateFilename(profile, params->name, true);

    for (auto* browser : *BrowserList::GetInstance()) {
      // Make sure the browser has tabs and a window. Browser's destructor
      // removes itself from the BrowserList. When a browser is closed the
      // destructor is not necessarily run immediately. This means it's possible
      // for us to get a handle to a browser that is about to be removed. If
      // the tab count is 0 or the window is NULL, the browser is about to be
      // deleted, so we ignore it.
      if (service.ShouldTrackWindow(browser, profile) &&
          browser->tab_strip_model()->count() && browser->window()) {
        if (save_window_id == 0 ||
            (save_window_id && browser->session_id().id() == save_window_id)) {
          service.BuildCommandsForBrowser(browser, ids);
        }
      }
    }
    bool success = service.Save(path);
    if (!success) {
      error_code = SessionErrorCodes::kErrorWriting;
    }
  }

  return RespondNow(ArgumentList(Results::Create(error_code)));
}

ExtensionFunction::ResponseAction SessionsPrivateGetAllFunction::Run() {
  using vivaldi::sessions_private::SessionItem;
  namespace Results = vivaldi::sessions_private::GetAll::Results;

  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::vector<SessionItem> sessions;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath path(profile->GetPath());
  path = path.Append(kSessionPath);

  base::FileEnumerator iter(path, false, base::FileEnumerator::FILES,
                            FILE_PATH_LITERAL("*.bin"));
  for (base::FilePath name = iter.Next(); !name.empty(); name = iter.Next()) {
    sessions.emplace_back();
    SessionItem* new_item = &sessions.back();
#if defined(OS_POSIX)
    std::string filename = name.BaseName().value();
#elif defined(OS_WIN)
    std::string filename = base::WideToUTF8(name.BaseName().value());
#endif
    size_t ext = filename.find_last_of('.');
    if (ext != std::string::npos) {
      // Get rid of the extension
      filename.erase(ext, std::string::npos);
    }
    new_item->name.assign(filename);

    base::FileEnumerator::FileInfo info = iter.GetInfo();
    base::Time modified = info.GetLastModifiedTime();
    new_item->create_date_js = modified.ToJsTime();
  }
  return RespondNow(ArgumentList(Results::Create(sessions)));
}

ExtensionFunction::ResponseAction SessionsPrivateOpenFunction::Run() {
  using vivaldi::sessions_private::Open::Params;
  namespace Results = vivaldi::sessions_private::Open::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ThreadRestrictions::ScopedAllowIO allow_io;

  ::vivaldi::SessionOptions opts;
  if (params->options.get()) {
    opts.openInNewWindow_ = params->options->open_in_new_window;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());

  // TODO(igor@vivaldi.com): consider switching to lastError error reporting.
  int error_code = SessionErrorCodes::kErrorFileMissing;
  base::FilePath path = GenerateFilename(profile, params->name, false);
  do {
    if (!base::PathExists(path))
      break;
    content::WebContents* web_contents = GetSenderWebContents();
    if (!web_contents)
      break;
    Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
        web_contents->GetOutermostWebContents());
    if (!browser)
      break;

    ::vivaldi::VivaldiSessionService service(profile);
    if (!service.Load(path, browser, opts))
      break;

    error_code = SessionErrorCodes::kNoError;
  } while (false);
  return RespondNow(ArgumentList(Results::Create(error_code)));
}

ExtensionFunction::ResponseAction SessionsPrivateDeleteFunction::Run() {
  using vivaldi::sessions_private::Delete::Params;
  namespace Results = vivaldi::sessions_private::Delete::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ThreadRestrictions::ScopedAllowIO allow_io;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  int error_code = SessionErrorCodes::kNoError;
  base::FilePath path = GenerateFilename(profile, params->name, false);
  if (!base::PathExists(path)) {
    error_code = kErrorFileMissing;
  } else {
    if (!base::DeleteFile(path, false)) {
      error_code = kErrorDeleteFailure;
    }
  }
  return RespondNow(ArgumentList(Results::Create(error_code)));
}

}  // namespace extensions
