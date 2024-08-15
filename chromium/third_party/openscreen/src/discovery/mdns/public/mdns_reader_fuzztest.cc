// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/common/config.h"
#include "discovery/mdns/public/mdns_reader.h"

namespace openscreen::discovery {
void Fuzz(const uint8_t* data, size_t size) {
  MdnsReader reader(Config{}, data, size);
  reader.Read();
}
}  // namespace openscreen::discovery
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  openscreen::discovery::Fuzz(data, size);
  return 0;
}
