/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/client/http/testing/http_test_server.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/base/scheduler.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace fcp {
namespace client {
namespace http {
namespace {
using ::tensorflow::serving::net_http::EventExecutor;
using ::tensorflow::serving::net_http::HTTPServerInterface;
using ::tensorflow::serving::net_http::RequestHandlerOptions;
using ::tensorflow::serving::net_http::ServerOptions;
using ::tensorflow::serving::net_http::ServerRequestInterface;

// Echoes the request back to the client
void EchoHandler(ServerRequestInterface* req) {
  std::string response;

  absl::StrAppend(&response, "HTTP Method: ", req->http_method(), "\n");
  absl::StrAppend(&response, "Request Uri: ", req->uri_path(), "\n");

  absl::StrAppend(&response, "Request Headers:\n");
  for (absl::string_view header : req->request_headers()) {
    absl::StrAppend(&response, header, ": ", req->GetRequestHeader(header),
                    "\n");
  }

  absl::StrAppend(&response, "Request Body:\n");
  // Read the request body
  int64_t num_bytes;
  while (true) {
    auto request_chunk = req->ReadRequestBytes(&num_bytes);
    if (request_chunk == nullptr) {
      break;
    }
    absl::StrAppend(&response,
                    absl::string_view(request_chunk.get(), num_bytes));
  }

  req->WriteResponseString(response);

  SetContentTypeHTML(req);
  req->Reply();
}

// Non-blocking event executor needed for an event-driven web server
class ThreadPoolEventExecutor final : public EventExecutor {
 public:
  explicit ThreadPoolEventExecutor(int num_threads)
      : thread_pool_scheduler_(CreateThreadPoolScheduler(num_threads)) {}
  ~ThreadPoolEventExecutor() override {
    thread_pool_scheduler_->WaitUntilIdle();
  }

  void Schedule(std::function<void()> fn) override {
    thread_pool_scheduler_->Schedule(fn);
  }

 private:
  std::unique_ptr<Scheduler> thread_pool_scheduler_;
};
}  // namespace

absl::StatusOr<std::unique_ptr<HTTPServerInterface>> CreateHttpTestServer(
    const std::string& uri, int port, int num_threads) {
  auto options = std::make_unique<ServerOptions>();
  options->AddPort(port);
  options->SetExecutor(std::make_unique<ThreadPoolEventExecutor>(num_threads));

  std::unique_ptr<HTTPServerInterface> http_server =
      CreateEvHTTPServer(std::move(options));
  if (http_server == nullptr) {
    return absl::InternalError("Failed to create EvHTTPServer");
  }

  RequestHandlerOptions handler_options;
  http_server->RegisterRequestHandler(uri, EchoHandler, handler_options);
  return http_server;
}

}  // namespace http
}  // namespace client
}  // namespace fcp
