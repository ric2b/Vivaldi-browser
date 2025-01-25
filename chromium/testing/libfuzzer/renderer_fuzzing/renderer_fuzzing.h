// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_LIBFUZZER_RENDERER_FUZZING_RENDERER_FUZZING_H_
#define TESTING_LIBFUZZER_RENDERER_FUZZING_RENDERER_FUZZING_H_

#include <unordered_map>

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

class RendererFuzzerBase {
 public:
  virtual void Run(
      const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
      blink::ThreadSafeBrowserInterfaceBrokerProxy*
          process_interface_broker_proxy,
      std::vector<uint8_t>&& input,
      base::OnceClosure done_closure) = 0;
  virtual const char* Id() = 0;
};

class RendererFuzzing {
 private:
  std::unordered_map<std::string, RendererFuzzerBase*> fuzzers_;

 public:
  bool RegisterFuzzer(RendererFuzzerBase* fuzzer) {
    fuzzers_[fuzzer->Id()] = fuzzer;
    return true;
  }

  static void Run(
      const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
      blink::ThreadSafeBrowserInterfaceBrokerProxy*
          process_interface_broker_proxy,
      const std::string& fuzzer_id,
      std::vector<uint8_t>&& input,
      base::OnceClosure done_closure);

  static RendererFuzzing* GetInstance();
};

#define REGISTER_RENDERER_FUZZER(klass)    \
  static bool RegisterFuzzerForClass_##T = \
      RendererFuzzing::GetInstance()->RegisterFuzzer(new klass)

#endif  // TESTING_LIBFUZZER_RENDERER_FUZZING_RENDERER_FUZZING_H_
