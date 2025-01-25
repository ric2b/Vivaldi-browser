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

#include <stdio.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "third_party/qairt/latest/include/QNN/HTP/QnnHtpDevice.h"
#include "tensorflow/lite/experimental/litert/c/litert_common.h"
#include "tensorflow/lite/experimental/litert/c/litert_logging.h"
#include "tensorflow/lite/experimental/litert/c/litert_model.h"
#include "tensorflow/lite/experimental/litert/c/litert_op_code.h"
#include "tensorflow/lite/experimental/litert/c/litert_support.h"
#include "tensorflow/lite/experimental/litert/cc/litert_support.h"
#include "tensorflow/lite/experimental/litert/core/graph_tools.h"
#include "tensorflow/lite/experimental/litert/vendors/c/litert_compiler_plugin.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/compiler/qnn_compose_graph.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/qnn_manager.h"

using ::litert::qnn::QnnManager;

//
// Configurations
//

namespace {

constexpr char kPluginManufacturer[] = "Qualcomm";

constexpr std::pair<const char*, QnnHtpDevice_Arch_t> kPluginSocModels[] = {
    {"V68", QNN_HTP_DEVICE_ARCH_V68},
    {"V69", QNN_HTP_DEVICE_ARCH_V69},
    {"V73", QNN_HTP_DEVICE_ARCH_V73},
    {"V75", QNN_HTP_DEVICE_ARCH_V75},
};

constexpr auto kNumPluginSocModels =
    sizeof(kPluginSocModels) / sizeof(kPluginSocModels[0]);

std::optional<QnnHtpDevice_Arch_t> FindSocModel(
    absl::string_view soc_model_name) {
  std::optional<QnnHtpDevice_Arch_t> soc_model;
  for (auto i = 0; i < kNumPluginSocModels; ++i) {
    if (soc_model_name == kPluginSocModels[i].first) {
      soc_model = kPluginSocModels[i].second;
      break;
    }
  }
  return soc_model;
}

}  // namespace

const char* LiteRtPluginSocManufacturer() { return kPluginManufacturer; }

LiteRtParamIndex LiteRtPluginNumSupportedSocModels(
    LiteRtCompilerPlugin compiler_plugin) {
  return kNumPluginSocModels;
}

LiteRtStatus LiteRtPluginGetSupportedSocModel(
    LiteRtCompilerPlugin compiler_plugin, LiteRtParamIndex soc_model_idx,
    const char** soc_model_name) {
  if (soc_model_idx < 0 || soc_model_idx >= kNumPluginSocModels) {
    return kLiteRtStatusErrorInvalidArgument;
  }
  *soc_model_name = kPluginSocModels[soc_model_idx].first;
  return kLiteRtStatusOk;
}

//
// Compiled Result Definition
//

struct LiteRtCompiledResultT {
  std::vector<char> context_bin;
  std::vector<std::string> graph_names;
};

LiteRtStatus LiteRtCompiledResultGetByteCode(
    LiteRtCompiledResult compiled_result, const void** byte_code,
    size_t* byte_code_size) {
  *byte_code = compiled_result->context_bin.data();
  *byte_code_size = compiled_result->context_bin.size();
  return kLiteRtStatusOk;
}

LiteRtStatus LiteRtCompiledResultGetCallInfo(
    LiteRtCompiledResult compiled_result, LiteRtParamIndex call_idx,
    const void** call_info, size_t* call_info_size) {
  if (call_idx >= compiled_result->graph_names.size()) {
    return kLiteRtStatusErrorIndexOOB;
  }

  *call_info = compiled_result->graph_names.at(call_idx).data();
  *call_info_size = compiled_result->graph_names.at(call_idx).size();

  return kLiteRtStatusOk;
}

LiteRtStatus LiteRtCompiledResultGetNumCalls(
    LiteRtCompiledResult compiled_result, LiteRtParamIndex* num_calls) {
  *num_calls = compiled_result->graph_names.size();
  return kLiteRtStatusOk;
}

void LiteRtCompiledResultDestroy(LiteRtCompiledResult compiled_result) {
  delete compiled_result;
}

//
// Plugin Definition
//

// Plugins can hold state.
struct LiteRtCompilerPluginT {};

LiteRtStatus LiteRtPluginInit(LiteRtCompilerPlugin* compiler_plugin) {
  auto* plugin = new LiteRtCompilerPluginT;
  *compiler_plugin = plugin;
  return kLiteRtStatusOk;
}

void LiteRtPluginDestroy(LiteRtCompilerPlugin compiler_plugin) {
  delete compiler_plugin;
}

namespace {

bool IsOpSupported(LiteRtOp op) {
  using TyInfo = graph_tools::RankedTypeInfo;

  // NOTE: Currently we are demoing by just mapping simple f32 mul ops.
  // In the limit this function withh want to leverage QNN SDK's getSuportedOps
  // feature (along with our op/type mappings).

  static const TyInfo supported_op_type = {kLiteRtElementTypeFloat32, {2, 2}};
  return graph_tools::MatchOpType(op, {supported_op_type, supported_op_type},
                                  {supported_op_type}, kLiteRtOpCodeTflMul);
}

}  // namespace

LiteRtStatus LiteRtPluginPartitionModel(LiteRtCompilerPlugin compiler_plugin,
                                        LiteRtModel model,
                                        LiteRtOpList selected_ops) {
  LITERT_ASSIGN_OR_RETURN_STATUS(auto subgraph,
                                 graph_tools::GetSubgraph(model));
  LITERT_ASSIGN_OR_RETURN_STATUS(auto ops,
                                 graph_tools::GetSubgraphOps(subgraph));

  for (auto op : ops) {
    if (!IsOpSupported(op)) {
      continue;
    }

    LITERT_RETURN_STATUS_IF_NOT_OK(LiteRtPushOp(selected_ops, op));
  }

  return kLiteRtStatusOk;
}

LiteRtStatus LiteRtPluginCompile(LiteRtCompilerPlugin compiler_plugin,
                                 const char* soc_model,
                                 LiteRtSubgraphArray partitions,
                                 LiteRtParamIndex num_partitions,
                                 LiteRtCompiledResult* compiled_result) {
  auto opt_soc_model = FindSocModel(soc_model);

  auto backend_configs = QnnManager::DefaultBackendConfigs();
  auto qnn_manager = QnnManager::Create(
      backend_configs, /*shared_library_dir=*/std::nullopt, opt_soc_model);
  if (!qnn_manager.ok()) {
    LITERT_LOG(LITERT_ERROR, "%s", qnn_manager.status().message().data());
    return kLiteRtStatusErrorRuntimeFailure;
  }

  auto context_configs = QnnManager::DefaultContextConfigs();
  auto context_handle = (*qnn_manager)->CreateContextHandle(context_configs);
  if (!context_handle.ok()) {
    LITERT_LOG(LITERT_ERROR, "%s", context_handle.status().message().data());
    return kLiteRtStatusErrorRuntimeFailure;
  }

  auto result = std::make_unique<LiteRtCompiledResultT>();

  // TODO: Support multiple partitions in QCC plugin compile.
  LITERT_ENSURE_SUPPORTED(num_partitions, 1);
  {
    std::string& entry_point_name = result->graph_names.emplace_back();
    entry_point_name = "qnn_partition_0";
    LITERT_RETURN_STATUS_IF_NOT_OK(litert::qnn::ComposeGraph(
        **qnn_manager, context_handle->get(), partitions[0], entry_point_name));
  }

  LITERT_RETURN_STATUS_IF_NOT_OK(
      (*qnn_manager)
          ->GenerateContextBinary(context_handle->get(), result->context_bin));

  *compiled_result = result.release();

  return kLiteRtStatusOk;
}
