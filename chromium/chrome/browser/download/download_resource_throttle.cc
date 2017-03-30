// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_resource_throttle.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/render_view_host.h"

using content::DownloadControllerAndroid;
#endif

using content::BrowserThread;

namespace {

void OnCanDownloadDecided(base::WeakPtr<DownloadResourceThrottle> throttle,
                          const content::DownloadItemAction& action) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceThrottle::ContinueDownload, throttle, action));
}

void CanDownload(
    scoped_ptr<DownloadResourceThrottle::DownloadRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  info->limiter->CanDownload(info->web_contents_getter, info->url,
                             info->request_method,
                             info->download_info,
                             info->continue_callback);
}

#if defined(OS_ANDROID)
void OnAcquireFileAccessPermissionDone(
    scoped_ptr<DownloadResourceThrottle::DownloadRequestInfo> info,
    bool granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (granted)
    CanDownload(std::move(info));
  else
    info->continue_callback.Run(false);
}
#endif

void CanDownloadOnUIThread(
    scoped_ptr<DownloadResourceThrottle::DownloadRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  content::WebContents* contents = info->web_contents_getter.Run();
  if (contents) {
    content::DownloadControllerAndroid::Get()->AcquireFileAccessPermission(
        contents, base::Bind(&OnAcquireFileAccessPermissionDone,
                             base::Passed(std::move(info))));
  } else {
    OnAcquireFileAccessPermissionDone(std::move(info), false);
  }
#else
  CanDownload(std::move(info));
#endif
}

}  // namespace

DownloadResourceThrottle::DownloadRequestInfo::DownloadRequestInfo(
    scoped_refptr<DownloadRequestLimiter> limiter,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    const content::DownloadInformation &download_info,
    const DownloadRequestLimiter::Callback& continue_callback)
    : limiter(limiter),
      web_contents_getter(web_contents_getter),
      url(url),
      request_method(request_method),
      download_info(download_info),
      continue_callback(continue_callback) {}

DownloadResourceThrottle::DownloadRequestInfo::~DownloadRequestInfo() {}

DownloadResourceThrottle::DownloadResourceThrottle(
    scoped_refptr<DownloadRequestLimiter> limiter,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    const content::DownloadInformation& info)
    : querying_limiter_(true),
      request_allowed_(false),
      request_deferred_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &CanDownloadOnUIThread,
          base::Passed(scoped_ptr<DownloadRequestInfo>(new DownloadRequestInfo(
              limiter, web_contents_getter, url, request_method, info,
              base::Bind(&OnCanDownloadDecided, AsWeakPtr()))))));
}

DownloadResourceThrottle::~DownloadResourceThrottle() {
}

void DownloadResourceThrottle::WillStartRequest(bool* defer) {
  WillDownload(defer);
}

void DownloadResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  WillDownload(defer);
}

void DownloadResourceThrottle::WillProcessResponse(bool* defer) {
  WillDownload(defer);
}

const char* DownloadResourceThrottle::GetNameForLogging() const {
  return "DownloadResourceThrottle";
}

void DownloadResourceThrottle::WillDownload(bool* defer) {
  DCHECK(!request_deferred_);

  // Defer the download until we have the DownloadRequestLimiter result.
  if (querying_limiter_) {
    request_deferred_ = true;
    *defer = true;
    return;
  }

  if (!request_allowed_)
    controller()->Cancel();
}

void DownloadResourceThrottle::ContinueDownload(const content::DownloadItemAction& action) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  querying_limiter_ = false;
  request_allowed_ = action.allow;

  if (action.allow) {
    // Presumes all downloads initiated by navigation use this throttle and
    // nothing else does.
    RecordDownloadSource(DOWNLOAD_INITIATED_BY_NAVIGATION);
  } else {
    RecordDownloadCount(CHROME_DOWNLOAD_COUNT_BLOCKED_BY_THROTTLING);
  }

  if (request_deferred_) {
    request_deferred_ = false;
    if (action.allow) {
      controller()->Resume(action.open_when_done, action.ask_for_target);
    } else {
      controller()->Cancel();
    }
  }
}
