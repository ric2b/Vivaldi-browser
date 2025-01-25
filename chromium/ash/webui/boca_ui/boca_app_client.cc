// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_client.h"

#include "base/check_op.h"

namespace ash {

namespace {

// Non thread safe, life cycle is managed by owner.
BocaAppClient* g_instance = nullptr;

}  // namespace

// static
BocaAppClient* BocaAppClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

BocaAppClient::BocaAppClient() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

BocaAppClient::~BocaAppClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BocaAppClient::Observer::OnBundleUpdated(const boca::Bundle& bundle) {}

void BocaAppClient::Observer::OnProducerCaptionConfigUpdated(
    const boca::CaptionsConfig& config) {}

void BocaAppClient::Observer::OnConsumerCaptionConfigUpdated(
    const boca::CaptionsConfig& config) {}

}  // namespace ash
