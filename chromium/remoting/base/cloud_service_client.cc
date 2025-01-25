// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/cloud_service_client.h"

#include "base/functional/bind.h"
#include "base/strings/stringize_macros.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/version.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/remote_access_service.pb.h"
#include "remoting/proto/remoting/v1/cloud_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
constexpr net::NetworkTrafficAnnotationTag
    kProvisionGceInstanceTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "remoting_cloud_provision_gce_instance",
            R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Registers a new Chrome Remote Desktop host for a GCE instance."
          trigger:
            "User runs the remoting_start_host command by typing it on the "
            "terminal. Third party administrators might implement scripts to "
            "run this automatically, but the Chrome Remote Desktop product "
            "does not come with such scripts."
          user_data {
            type: EMAIL
            type: OTHER
          }
          data:
            "The email address of the account to configure CRD for and the "
            "display name of the new remote access host instance which will be "
            "shown in the Chrome Remote Desktop client website."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-03-29"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the start-host utility is not run with the cloud-user flag."
          policy_exception_justification:
            "Not implemented."
        })");

using LegacyProvisionGceInstanceRequest =
    remoting::apis::v1::ProvisionGceInstanceRequest;
using ProvisionGceInstanceRequest =
    google::internal::remoting::cloud::v1alpha::ProvisionGceInstanceRequest;
}  // namespace

namespace remoting {

CloudServiceClient::CloudServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

CloudServiceClient::~CloudServiceClient() = default;

void CloudServiceClient::LegacyProvisionGceInstance(
    const std::string& owner_email,
    const std::string& display_name,
    const std::string& public_key,
    const std::optional<std::string>& existing_directory_id,
    LegacyProvisionGceInstanceCallback callback) {
  constexpr char path[] = "/v1/cloud:provisionGceInstance";

  auto request = std::make_unique<LegacyProvisionGceInstanceRequest>();
  request->set_owner_email(owner_email);
  request->set_display_name(display_name);
  request->set_public_key(public_key);
  request->set_version(STRINGIZE(VERSION));
  if (existing_directory_id.has_value() && !existing_directory_id->empty()) {
    request->set_existing_directory_id(*existing_directory_id);
  }

  CHECK(!http_client_.has_value());
  http_client_.emplace(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                       /*token_getter=*/nullptr, url_loader_factory_);

  ExecuteRequest(kProvisionGceInstanceTrafficAnnotation, path,
                 google_apis::GetRemotingAPIKey(), std::move(request),
                 std::move(callback));
}

void CloudServiceClient::ProvisionGceInstance(
    const std::string& owner_email,
    const std::string& display_name,
    const std::string& public_key,
    const std::optional<std::string>& existing_directory_id,
    const std::string& api_key,
    ProvisionGceInstanceCallback callback) {
  constexpr char path[] = "/v1alpha/access:provisionGceInstance";

  auto request = std::make_unique<ProvisionGceInstanceRequest>();
  request->set_owner_email(owner_email);
  request->set_display_name(display_name);
  request->set_public_key(public_key);
  request->set_version(STRINGIZE(VERSION));
  if (existing_directory_id.has_value() && !existing_directory_id->empty()) {
    request->set_existing_directory_id(*existing_directory_id);
  }

  CHECK(!http_client_.has_value());
  http_client_.emplace(ServiceUrls::GetInstance()->remoting_cloud_endpoint(),
                       /*token_getter=*/nullptr, url_loader_factory_);

  ExecuteRequest(kProvisionGceInstanceTrafficAnnotation, path, api_key,
                 std::move(request), std::move(callback));
}

void CloudServiceClient::CancelPendingRequests() {
  if (http_client_.has_value()) {
    http_client_->CancelPendingRequests();
  }
}

template <typename CallbackType>
void CloudServiceClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& path,
    const std::string& api_key,
    std::unique_ptr<google::protobuf::MessageLite> request_message,
    CallbackType callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(traffic_annotation);
  request_config->path = path;
  request_config->api_key = api_key;
  request_config->authenticated = false;
  request_config->request_message = std::move(request_message);
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(std::move(callback));
  http_client_->ExecuteRequest(std::move(request));
}

}  // namespace remoting
