// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_PRESENTATION_TESTING_MOCK_CONNECTION_DELEGATE_H_
#define OSP_IMPL_PRESENTATION_TESTING_MOCK_CONNECTION_DELEGATE_H_

#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "osp/public/presentation/presentation_connection.h"

namespace openscreen::osp {

class MockConnectionDelegate : public Connection::Delegate {
 public:
  MockConnectionDelegate() = default;
  MockConnectionDelegate(const MockConnectionDelegate&) = delete;
  MockConnectionDelegate& operator=(const MockConnectionDelegate&) = delete;
  MockConnectionDelegate(MockConnectionDelegate&&) noexcept = delete;
  MockConnectionDelegate& operator=(MockConnectionDelegate&&) noexcept = delete;
  ~MockConnectionDelegate() override = default;

  MOCK_METHOD0(OnConnected, void());
  MOCK_METHOD0(OnClosedByRemote, void());
  MOCK_METHOD0(OnDiscarded, void());
  MOCK_METHOD1(OnError, void(const std::string_view message));
  MOCK_METHOD0(OnTerminated, void());
  MOCK_METHOD1(OnStringMessage, void(const std::string_view message));
  MOCK_METHOD1(OnBinaryMessage, void(const std::vector<uint8_t>& data));
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_PRESENTATION_TESTING_MOCK_CONNECTION_DELEGATE_H_
