// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_values.h"

RustValues::RustValues() : crate_type_(RustValues::CRATE_AUTO) {}

RustValues::~RustValues() = default;