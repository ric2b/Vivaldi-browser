// Copyright 2024 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "tensorflow/lite/c/c_api_opaque.h"
#include "tensorflow/lite/c/c_api_types.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/delegates/utils/simple_opaque_delegate.h"
#include "tensorflow/lite/experimental/litert/c/litert_dispatch_delegate.h"
#include "tensorflow/lite/experimental/litert/c/litert_logging.h"
#include "tensorflow/lite/experimental/litert/core/dispatch/dispatch_delegate_kernel.h"
#include "tensorflow/lite/experimental/litert/core/dispatch/dispatch_delegate_options.h"
#include "tensorflow/lite/experimental/litert/vendors/c/litert_dispatch.h"

namespace {

// A TFL Delegate that can recognize subgraphs that run on Dispatch API capable
// accelerators, e.g. TPU, DSP, ... It replaces such subgraphs and offloads
// their work through the Dispatch API.
class DispatchDelegate : public tflite::SimpleOpaqueDelegateInterface {
 public:
  static TfLiteOpaqueDelegate* Create(LiteRtDispatchDelegateOptions* options_) {
    litert::DispatchDelegateOptionsPtr options(
        options_, LiteRtDestroyDispatchDelegateOptions);
    if (!options) {
      LITERT_LOG(LITERT_ERROR, "Null input");
      return nullptr;
    }

    std::unique_ptr<DispatchDelegate> managed_sb_delegate(
        new DispatchDelegate(std::move(options)));
    return tflite::TfLiteOpaqueDelegateFactory::CreateSimpleDelegate(
        std::move(managed_sb_delegate),
        kTfLiteDelegateFlagsAllowDynamicTensors);
  }

  bool IsNodeSupportedByDelegate(const TfLiteOperator* op,
                                 const TfLiteOpaqueNode* node,
                                 TfLiteOpaqueContext* context) const override;

  TfLiteStatus Initialize(TfLiteOpaqueContext* context) override;

  const char* Name() const override;

  std::unique_ptr<tflite::SimpleOpaqueDelegateKernelInterface>
  CreateDelegateKernelInterface() override;

 private:
  static constexpr absl::string_view kDelegateName = "DispatchDelegate";
  static constexpr absl::string_view kDispatchNodeCustomCode = "dispatch_node";

  explicit DispatchDelegate(litert::DispatchDelegateOptionsPtr&& options)
      : options_(std::move(options)) {}

  litert::DispatchDelegateOptionsPtr options_;
  int dispatch_graph_name_id_ = 0;
};

bool DispatchDelegate::IsNodeSupportedByDelegate(
    const TfLiteOperator* op, const TfLiteOpaqueNode* node,
    TfLiteOpaqueContext* context) const {
  auto custom_code = absl::string_view(TfLiteOperatorGetCustomName(op));
  return custom_code == kDispatchNodeCustomCode;
}

TfLiteStatus DispatchDelegate::Initialize(TfLiteOpaqueContext* context) {
  return kTfLiteOk;
}

const char* DispatchDelegate::Name() const { return kDelegateName.data(); }

std::unique_ptr<tflite::SimpleOpaqueDelegateKernelInterface>
DispatchDelegate::CreateDelegateKernelInterface() {
  std::string dispatch_graph_name =
      absl::StrFormat("DispatchGraph_%d", dispatch_graph_name_id_++);

  auto kernel = litert::internal::DispatchDelegateKernel::Create(
      std::move(dispatch_graph_name), *options_);
  if (kernel.ok()) {
    return std::move(*kernel);
  } else {
    LITERT_LOG(LITERT_ERROR, "Failed to create a dispatch delegate kernel: %s",
               kernel.status().message().data());
    return nullptr;
  }
}

}  // namespace

LiteRtDispatchDelegateOptions* LiteRtCreateDefaultDispatchDelegateOptions() {
  return new LiteRtDispatchDelegateOptions;
}

TfLiteStatus LiteRtAddDispatchDelegateOption(
    LiteRtDispatchDelegateOptions* options, LiteRtDispatchOption option) {
  if (!options) {
    LITERT_LOG(LITERT_ERROR, "Null input");
    return kTfLiteError;
  }

  options->AddOption(option);
  return kTfLiteOk;
}

TfLiteStatus LiteRtAddDispatchDelegateExecInfoOption(
    LiteRtDispatchDelegateOptions* options, const char* exec_tag,
    const void* bytecode_addr, size_t bytecode_size,
    const char* function_name) {
  if (!options || !exec_tag || !bytecode_addr) {
    LITERT_LOG(LITERT_ERROR, "Null input");
    return kTfLiteError;
  }

  LiteRtDispatchDelegateOptions::ExecInfo exec_info;
  exec_info.bytecode =
      absl::MakeSpan(static_cast<const uint8_t*>(bytecode_addr), bytecode_size);
  if (function_name) {
    exec_info.function_name = function_name;
  }

  options->AddExecInfo(exec_tag, std::move(exec_info));
  return kTfLiteOk;
}

void LiteRtDestroyDispatchDelegateOptions(
    LiteRtDispatchDelegateOptions* options) {
  delete options;
}

TfLiteDelegate* LiteRtCreateDispatchDelegate(
    LiteRtDispatchDelegateOptions* options) {
  return DispatchDelegate::Create(options);
}

void LiteRtDestroyDispatchDelegate(TfLiteOpaqueDelegate* delegate) {
  tflite::TfLiteOpaqueDelegateFactory::DeleteSimpleDelegate(delegate);
}

namespace litert {

DispatchDelegateOptionsPtr CreateDispatchDelegateOptionsPtr() {
  return {LiteRtCreateDefaultDispatchDelegateOptions(),
          LiteRtDestroyDispatchDelegateOptions};
}

DispatchDelegatePtr CreateDispatchDelegatePtr(
    DispatchDelegateOptionsPtr&& options) {
  return DispatchDelegatePtr(LiteRtCreateDispatchDelegate(options.release()),
                             LiteRtDestroyDispatchDelegate);
}
}  // namespace litert
