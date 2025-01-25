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
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/compiler/legalizations/slice_op_legalization.h"

#include <alloca.h>
#include <stdio.h>

#include <cstdint>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "third_party/qairt/latest/include/QNN/QnnTypes.h"
#include "tensorflow/lite/experimental/litert/c/litert_common.h"
#include "tensorflow/lite/experimental/litert/c/litert_op_code.h"
#include "tensorflow/lite/experimental/litert/c/litert_support.h"
#include "tensorflow/lite/experimental/litert/cc/litert_model.h"
#include "tensorflow/lite/experimental/litert/core/graph_tools.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/common.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/compiler/IR/qnn_op.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/compiler/IR/qnn_tensor.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/compiler/graph_mapper.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/compiler/legalizations/util.h"
#include "tensorflow/lite/experimental/litert/vendors/qualcomm/qnn_manager.h"

namespace litert::qnn {

static constexpr absl::string_view kQnnSliceOpTypeName = "StridedSlice";
static constexpr absl::string_view kDefaultQnnOpPackageName = "qti.aisw";
static constexpr absl::string_view kSliceOpFmt = "slice_%d";

static constexpr int kSliceOpInputSize = 1;
static constexpr int kSliceOpOutputSize = 1;
static constexpr int kSliceOpParamSize = 1;
// QNN StridedSlice op packs "start", "end", and "stride" into a single tensor
// param "ranges".
static constexpr int kRangesParamArgSize = 3;
static constexpr int kRangesParamRank = 2;

LiteRtStatus SliceOpLegalization::LegalizeOp(const Op& src,
                                             Qnn_OpConfig_t& dest,
                                             GraphMapper& graph_mapper) {
  if (src.Code() != kLiteRtOpCodeTflSlice) {
    return kLiteRtStatusLegalizeNoMatch;
  }
  DumpLegalization(*src.Get());
  std::string op_name = absl::StrFormat(kSliceOpFmt, op_counter_++);
  LITERT_RETURN_STATUS_IF_NOT_OK(SetOpInfo(op_name.c_str(),
                                           kDefaultQnnOpPackageName.data(),
                                           kQnnSliceOpTypeName.data(), dest));

  // QNN strided slice op expects 1 input tensor.
  LITERT_ASSIGN_OR_RETURN_STATUS(auto op_ins,
                                 ::graph_tools::GetOpIns(src.Get()));
  LITERT_STACK_ARRAY(Qnn_Tensor_t, qnn_op_ins, kSliceOpInputSize,
                     QNN_TENSOR_INIT);
  LITERT_RETURN_STATUS_IF_NOT_OK(
      graph_mapper.LookupInScope(op_ins[0], qnn_op_ins[0]));

  // QNN strided slice op expects 1 output tensor.
  LITERT_ASSIGN_OR_RETURN_STATUS(auto op_outs,
                                 ::graph_tools::GetOpOuts(src.Get()));
  LITERT_STACK_ARRAY(Qnn_Tensor_t, qnn_op_outs, kSliceOpOutputSize,
                     QNN_TENSOR_INIT);
  LITERT_RETURN_STATUS_IF_NOT_OK(
      graph_mapper.LegalizeAndRegister(op_outs[0], qnn_op_outs[0]));
  LITERT_RETURN_STATUS_IF_NOT_OK(
      graph_mapper.PushToScope(op_outs[0], qnn_op_outs[0]));

  litert::Tensor src_input_tensor(op_ins[0]);
  auto src_input_tensor_rank =
      src_input_tensor.RankedTensorType().Layout().Rank();

  // Prepare qnn strided slice parameters.
  auto src_begin_indices = graph_tools::GetWeights<int32_t>(op_ins[1]).Value();
  auto src_size_indices = graph_tools::GetWeights<int32_t>(op_ins[2]).Value();

  // Check if src_begin_indices and src_size_indices are weights tensors.
  if (src_begin_indices.empty() || src_size_indices.empty()) {
    return kLiteRtStatusErrorInvalidLegalization;
  }

  LITERT_STACK_ARRAY(int32_t, range_tensor_data,
                     src_input_tensor_rank* kRangesParamArgSize,
                     /*init value*/ 0);
  for (int i = 0; i < src_input_tensor_rank; ++i) {
    // Copy begin, end, and stride values from src_begin_indices and
    // src_size_indices to range_tensor_data. Stride is always 1.
    range_tensor_data[i * kRangesParamArgSize] = src_begin_indices[i];
    range_tensor_data[i * kRangesParamArgSize + 1] = src_size_indices[i];
    range_tensor_data[i * kRangesParamArgSize + 2] = 1;
  }

  Qnn_ClientBuffer_t range_tensor_client_buf = BuildDefaultClientBuffer();
  range_tensor_client_buf.data = range_tensor_data;
  range_tensor_client_buf.dataSize =
      src_input_tensor_rank * kRangesParamArgSize * sizeof(int32_t);

  // Construct the const tensor "ranges".
  Qnn_Tensor_t range_tensor = BuildDefaultTensor();
  graph_mapper.AssignTensorName(range_tensor);
  range_tensor.v2.dataType = QNN_DATATYPE_INT_32;
  range_tensor.v2.type = QNN_TENSOR_TYPE_STATIC;
  range_tensor.v2.rank = kRangesParamRank;
  range_tensor.v2.dimensions = new uint32_t[kRangesParamRank];
  range_tensor.v2.dimensions[0] = src_input_tensor_rank;
  range_tensor.v2.dimensions[1] = kRangesParamArgSize;
  range_tensor.v2.memType = QNN_TENSORMEMTYPE_RAW;
  range_tensor.v2.clientBuf = range_tensor_client_buf;
  range_tensor.v2.isDynamicDimensions = nullptr;
  LITERT_RETURN_STATUS_IF_QNN_NOT_OK(
      graph_mapper.Qnn().Api()->tensorCreateGraphTensor(graph_mapper.QnnGraph(),
                                                        &range_tensor));

  Qnn_Param_t range_param = BuildDefaultParam();
  range_param.paramType = QNN_PARAMTYPE_TENSOR;
  range_param.name = "ranges";
  range_param.tensorParam = range_tensor;

  Qnn_Param_t strided_slice_params[] = {range_param};
  dest.v1.inputTensors = qnn_op_ins;
  dest.v1.numOfInputs = kSliceOpInputSize;
  dest.v1.outputTensors = qnn_op_outs;
  dest.v1.numOfOutputs = kSliceOpOutputSize;
  dest.v1.numOfParams = kSliceOpParamSize;
  dest.v1.params = strided_slice_params;

  LITERT_RETURN_STATUS_IF_QNN_NOT_OK(
      graph_mapper.Qnn().Api()->graphAddNode(graph_mapper.QnnGraph(), dest));

  LITERT_LOG(LITERT_INFO, "Legalized slice op", "");

  return kLiteRtStatusOk;
}

}  // namespace litert::qnn
