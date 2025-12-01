// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "content/public/browser/web_contents_delegate.h"
#include "components/content/vivaldi_postponed_calls.h"

namespace content {

void WebContentsDelegate::SetDownloadInformation(
    const content::DownloadInformation& info) {
  download_info_ = info;
}

void WebContentsDelegate::CanDownload(const GURL& url,
                                      const std::string& request_method,
                                      base::OnceCallback<void(bool)> callback) {
  if (IsWebApp()) {
    std::move(callback).Run(true);
    return;
  }
// NOTE(jarle@vivaldi.com): Ref. VAB-11056.
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(true);
#else
  using Args = vivaldi::VivaldiPostponedCalls::Args;
  using Reason = vivaldi::VivaldiPostponedCalls::Reason;

  auto* postponed_calls = GetVivaldiPostponedCalls();
  CHECK(postponed_calls);

  postponed_calls->Add(base::BindOnce(
      [](GURL url, std::string request_method,
         base::OnceCallback<void(bool)> callback, const Args& args) {
        if (args.reason == Reason::GUEST_ATTACHED) {
          CHECK(args.delegate);
          if (args.delegate->IsVivaldiGuestView()) {
            args.delegate->VivaldiCanDownload(url, request_method,
                                              std::move(callback));
            return;
          }
        }
        std::move(callback).Run(false);
      },
      url, request_method, std::move(callback)));
#endif // IS_ANDROID
}

::vivaldi::VivaldiPostponedCalls*
WebContentsDelegate::GetVivaldiPostponedCalls() {
  if (!vivaldi_postponed_calls_) {
    vivaldi_postponed_calls_ =
        std::make_unique<::vivaldi::VivaldiPostponedCalls>();
  }
  return vivaldi_postponed_calls_.get();
}

void WebContentsDelegate::RunVivaldiPostponedCalls() {
  auto calls = vivaldi_postponed_calls_.get();
  if (calls) {
    calls->GuestAttached(this);
  }
}

void WebContentsDelegate::VivaldiCanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

DownloadInformation::DownloadInformation(
    int64_t size,
    const std::string& mimetype,
    const std::u16string& suggested_filename)
    : size(size), mime_type(mimetype), suggested_filename(suggested_filename) {}

DownloadInformation::DownloadInformation(const DownloadInformation& old)
    : size(old.size),
      mime_type(old.mime_type),
      suggested_filename(old.suggested_filename) {}

DownloadInformation::DownloadInformation() : size(-1) {}

DownloadInformation::~DownloadInformation() {}

}  // namespace content
