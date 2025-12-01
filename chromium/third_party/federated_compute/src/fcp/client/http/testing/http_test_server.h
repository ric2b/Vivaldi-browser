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

#ifndef FCP_CLIENT_HTTP_TESTING_HTTP_TEST_SERVER_H_
#define FCP_CLIENT_HTTP_TESTING_HTTP_TEST_SERVER_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"

namespace fcp {
namespace client {
namespace http {
// Creates a test server where
// uri: the uri starting with "/"
// port: the port to listen
// num_threads: the number of parallel threads the server has
absl::StatusOr<
    std::unique_ptr<::tensorflow::serving::net_http::HTTPServerInterface>>
CreateHttpTestServer(const std::string& uri, int port, int num_threads);
}  // namespace http
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_HTTP_TESTING_HTTP_TEST_SERVER_H_
