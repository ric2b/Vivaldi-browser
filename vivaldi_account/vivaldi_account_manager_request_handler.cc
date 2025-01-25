// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager_request_handler.h"

#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace vivaldi {

namespace {

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    5000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    // TODO(crbug.com/246686): We should retry RequestAccessToken on connection
    // state change after backoff.
    1000 * 60 * 5,  // 5 minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
    const GURL& url,
    const std::string& body,
    const net::HttpRequestHeaders& headers) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_account_manager_fetcher",
                                          R"(
        semantics {
          sender: "Vivaldi Account Manager Fetcher"
          description:
            "This request is used by the Vivaldi account manager to fetch an "
            "OAuth 2.0 tokens and user information for a vivaldi.net account."
          trigger:
            "This request is triggered when logging in the browser to "
            "vivaldi.net as well as on startup or when the previous access "
            "token has expired"
          data:
            "Vivaldi account credentials."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if user signs "
            "out of their account, this request would not be made."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers = headers;
  if (!body.empty())
    resource_request->method = "POST";

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (!body.empty())
    url_loader->AttachStringForUpload(body,
                                      "application/x-www-form-urlencoded");

  // We want to receive the body even on error, as it contains the reason for
  // failure.
  url_loader->SetAllowHttpErrorResults(true);

  url_loader->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  return url_loader;
}

}  // anonymous namespace

VivaldiAccountManagerRequestHandler::VivaldiAccountManagerRequestHandler(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& request_url,
    const std::string& body,
    const net::HttpRequestHeaders& headers,
    RequestDoneCallback callback)
    : url_loader_factory_(std::move(url_loader_factory)),
      request_url_(request_url),
      headers_(headers),
      body_(body),
      callback_(callback),
      request_backoff_(&kBackoffPolicy) {
  HandleRequest();
}

VivaldiAccountManagerRequestHandler::~VivaldiAccountManagerRequestHandler() {}

void VivaldiAccountManagerRequestHandler::HandleRequest() {
  request_start_time_ = base::Time::Now();

  url_loader_ = CreateURLLoader(request_url_, body_, headers_);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&VivaldiAccountManagerRequestHandler::OnURLLoadComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void VivaldiAccountManagerRequestHandler::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  callback_.Run(std::move(url_loader_), std::move(response_body));

  if (!request_backoff_timer_.IsRunning()) {
    done_ = true;
  }
}

void VivaldiAccountManagerRequestHandler::Retry() {
  request_backoff_.InformOfRequest(false);
  request_backoff_timer_.Start(
      FROM_HERE, request_backoff_.GetTimeUntilRelease(),
      base::BindRepeating(&VivaldiAccountManagerRequestHandler::HandleRequest,
                          base::Unretained(this)));
}

base::Time VivaldiAccountManagerRequestHandler::GetNextRequestTime() const {
  if (request_backoff_timer_.IsRunning()) {
    return base::Time::Now() +
           (request_backoff_timer_.desired_run_time() - base::TimeTicks::Now());
  }
  return base::Time();
}

}  // namespace vivaldi