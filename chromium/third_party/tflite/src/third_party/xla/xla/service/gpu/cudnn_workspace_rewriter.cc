/* Copyright 2024 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/service/gpu/cudnn_workspace_rewriter.h"

#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "xla/hlo/ir/dfs_hlo_visitor_with_default.h"
#include "xla/hlo/ir/hlo_casting_utils.h"
#include "xla/hlo/ir/hlo_clone_context.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/gpu/cublas_cudnn.h"
#include "xla/service/gpu/gpu_fused_mha_runner.h"
#include "xla/service/gpu/stream_executor_util.h"
#include "xla/shape.h"
#include "xla/shape_util.h"
#include "xla/status_macros.h"
#include "xla/stream_executor/cuda/cuda_dnn.h"
#include "xla/stream_executor/dnn.h"
#include "xla/util.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace gpu {

namespace {

// create cuDNN graphs from HloCustomCall
absl::StatusOr<se::gpu::CudnnGraph> HloCustomCallToCuDnnGraph(
    se::dnn::DnnSupport& dnn_support, HloCustomCallInstruction* custom_call) {
  if (IsFwdCustomCallTofMHA(*custom_call)) {
    TF_ASSIGN_OR_RETURN(const xla::gpu::CudnnfMHAKind kind,
                        xla::gpu::GetCudnnfMHAKind(custom_call));
    std::optional<Shape> mask_shape, bias_shape;
    {
      bool has_bias = kind == CudnnfMHAKind::kScaleBiasSoftmax ||
                      kind == CudnnfMHAKind::kScaleBiasSoftmaxDropout;

      if (has_bias) {
        const HloInstruction* bias = custom_call->operand(3);
        bias_shape = bias->shape();
      }
    }

    TF_ASSIGN_OR_RETURN(
        const auto gpu_config,
        custom_call->backend_config<xla::gpu::GpuBackendConfig>());
    const xla::gpu::CudnnfMHABackendConfig& config =
        gpu_config.cudnn_fmha_backend_config();
    Shape intermediate_tensor_shape(config.intermediate_tensor_shape());
    absl::InlinedVector<Shape, 2> output_shapes = {
        ShapeUtil::GetSubshape(custom_call->shape(), {0})};

    bool has_activation =
        xla::ShapeUtil::TupleElementCount(custom_call->shape()) == 3;
    if (has_activation) {
      output_shapes.push_back(
          ShapeUtil::GetSubshape(custom_call->shape(), {1}));
    }

    Shape q_shape = custom_call->operand(0)->shape();
    Shape k_shape = custom_call->operand(1)->shape();
    Shape v_shape = custom_call->operand(2)->shape();
    TF_ASSIGN_OR_RETURN(CudnnfMHAMaskKind cudnn_mask_type,
                        AsCudnnFmhaMaskKind(config.mask_type()));
    GpufMHADescriptor descriptor = {kind,
                                    config,
                                    cudnn_mask_type,
                                    q_shape,
                                    k_shape,
                                    v_shape,
                                    intermediate_tensor_shape,
                                    output_shapes,
                                    config.bmm1_dot_dimension_numbers(),
                                    config.bmm2_dot_dimension_numbers(),
                                    mask_shape,
                                    bias_shape};

    TF_ASSIGN_OR_RETURN(GpufMHAConfig fmha_config,
                        GpufMHAConfig::For(descriptor));
    TF_ASSIGN_OR_RETURN(
        se::dnn::FMHAMaskKind dnn_mask_type,
        GetDNNFmhaMaskKindFromCudnnFmhaMaskKind(fmha_config.mask_type));
    TF_ASSIGN_OR_RETURN(
        se::gpu::CudnnGraph graph,
        se::gpu::GetCudnnFlashAttentionOperationGraph(
            dnn_support, fmha_config.lhs_bmm1, fmha_config.rhs_bmm1,
            fmha_config.rhs_bmm2, fmha_config.output, fmha_config.bias,
            fmha_config.activation, static_cast<float>(*fmha_config.fmha_scale),
            fmha_config.dropout_rate && *fmha_config.dropout_rate > 0.0,
            fmha_config.dropout_rate, dnn_mask_type));
    return std::move(graph);
  } else {
    TF_ASSIGN_OR_RETURN(
        auto gpu_config,
        custom_call->backend_config<xla::gpu::GpuBackendConfig>());
    xla::gpu::CudnnfMHABackendConfig& config =
        *gpu_config.mutable_cudnn_fmha_backend_config();

    int input_index = 0;
    Shape bmm1_grad_gemm1_rhs_shape =
        custom_call->operand(input_index++)->shape();
    Shape bmm1_grad_gemm2_rhs_shape =
        custom_call->operand(input_index++)->shape();
    Shape bmm2_grad_gemm2_rhs_shape =
        custom_call->operand(input_index++)->shape();
    Shape bmm2_grad_gemm1_lhs_shape(config.intermediate_tensor_shape());
    input_index++;
    Shape d_output_shape = custom_call->operand(input_index++)->shape();

    TF_ASSIGN_OR_RETURN(const CudnnfMHAKind kind,
                        GetCudnnfMHAKind(custom_call));
    std::optional<Shape> mask_shape;

    bool has_bias = (kind == CudnnfMHAKind::kBackwardScaleBiasSoftmax ||
                     kind == CudnnfMHAKind::kBackwardScaleBiasSoftmaxDropout);
    std::optional<Shape> bias_shape;
    if (has_bias) {
      bias_shape = custom_call->operand(input_index++)->shape();
    }

    std::optional<Shape> fwd_output_shape =
        custom_call->operand(input_index++)->shape();
    if (config.mask_type() == xla::gpu::CudnnfMHABackendConfig::PADDING ||
        config.mask_type() ==
            xla::gpu::CudnnfMHABackendConfig::PADDING_CAUSAL) {
      // skip q_seqlen and kv_seqlen
      input_index += 2;
    }
    TF_RET_CHECK(input_index == custom_call->operand_count());

    int output_index = 0;
    Shape d_bmm1_lhs_shape =
        ShapeUtil::GetSubshape(custom_call->shape(), {output_index++});
    Shape d_bmm1_rhs_shape =
        ShapeUtil::GetSubshape(custom_call->shape(), {output_index++});
    Shape d_bmm2_rhs_shape =
        ShapeUtil::GetSubshape(custom_call->shape(), {output_index++});
    std::optional<Shape> d_s_shape;
    std::optional<Shape> d_bias_shape;
    bool has_dbias = custom_call->shape().tuple_shapes().size() == 5;
    if (has_dbias) {
      d_bias_shape =
          ShapeUtil::GetSubshape(custom_call->shape(), {output_index++});
    }
    // The last one is the workspace.
    TF_RET_CHECK(output_index ==
                 custom_call->shape().tuple_shapes().size() - 1);
    TF_ASSIGN_OR_RETURN(CudnnfMHAMaskKind cudnn_mask_type,
                        AsCudnnFmhaMaskKind(config.mask_type()));

    const DebugOptions& debug_options =
        custom_call->GetModule()->config().debug_options();
    bool force_deterministic =
        debug_options.xla_gpu_deterministic_ops() ||
        debug_options.xla_gpu_exclude_nondeterministic_ops();
    // set the correct force_deterministic attribute here
    config.set_force_deterministic(force_deterministic);
    TF_RETURN_IF_ERROR(custom_call->set_backend_config(gpu_config));

    GpufMHABackwardDescriptor descriptor = {
        kind,
        config,
        cudnn_mask_type,
        bmm1_grad_gemm1_rhs_shape,
        bmm1_grad_gemm2_rhs_shape,
        bmm2_grad_gemm1_lhs_shape,
        bmm2_grad_gemm2_rhs_shape,
        d_output_shape,
        d_bmm1_lhs_shape,
        d_bmm1_rhs_shape,
        d_bmm2_rhs_shape,
        config.bmm1_grad_gemm1_dot_dimension_numbers(),
        config.bmm1_grad_gemm2_dot_dimension_numbers(),
        config.bmm2_grad_gemm1_dot_dimension_numbers(),
        config.bmm2_grad_gemm2_dot_dimension_numbers(),
        d_s_shape,
        fwd_output_shape,
        mask_shape,
        d_bias_shape,
        bias_shape,
        force_deterministic};

    TF_ASSIGN_OR_RETURN(GpufMHABackwardConfig fmha_config,
                        GpufMHABackwardConfig::For(descriptor));
    TF_ASSIGN_OR_RETURN(
        se::dnn::FMHAMaskKind dnn_mask_type,
        GetDNNFmhaMaskKindFromCudnnFmhaMaskKind(fmha_config.mask_type));

    TF_ASSIGN_OR_RETURN(
        se::gpu::CudnnGraph graph,
        se::gpu::GetCudnnFlashAttentionBackwardOperationGraph(
            dnn_support, fmha_config.bmm1_grad_gemm1_rhs,
            fmha_config.bmm1_grad_gemm2_rhs, fmha_config.bmm2_grad_gemm1_lhs,
            fmha_config.bmm2_grad_gemm2_rhs, fmha_config.d_output,
            fmha_config.d_bmm1_lhs, fmha_config.d_bmm1_rhs,
            fmha_config.d_bmm2_rhs, fmha_config.bias, fmha_config.dropout_rate,
            fmha_config.seed, *fmha_config.fmha_scale,
            fmha_config.dropout_rate && *fmha_config.dropout_rate > 0.0,
            fmha_config.bias != std::nullopt, dnn_mask_type,
            force_deterministic));
    return std::move(graph);
  }
}

class CuDnnCustomCallVisitor : public DfsHloRewriteVisitor {
 public:
  explicit CuDnnCustomCallVisitor(se::dnn::DnnSupport& dnn_support)
      : dnn_support_(dnn_support) {}

  absl::Status HandleCustomCall(HloInstruction* hlo) override {
    if (!IsCustomCallTofMHA(*hlo)) {
      // don't do anything about other cuDNN custom calls
      return absl::OkStatus();
    }
    TF_ASSIGN_OR_RETURN(auto gpu_config,
                        hlo->backend_config<GpuBackendConfig>());

    TF_ASSIGN_OR_RETURN(
        se::gpu::CudnnGraph graph,
        HloCustomCallToCuDnnGraph(dnn_support_,
                                  DynCast<HloCustomCallInstruction>(hlo)));
    auto workspace = graph.Graph().get_workspace_size();
    if (workspace != 0) {
      // rewrite custom call to have correct workspace size
      VLOG(4) << "Rewriting: " << hlo->ToString();
      Shape* shape = hlo->mutable_shape();
      shape->mutable_tuple_shapes(shape->tuple_shapes_size() - 1)
          ->set_dimensions(0, workspace);
      MarkAsChanged();
    }
    return absl::OkStatus();
  }

 private:
  se::dnn::DnnSupport& dnn_support_;
};

}  // namespace

absl::StatusOr<bool> CuDnnWorkspaceRewriter::Run(
    HloModule* module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  XLA_SCOPED_LOGGING_TIMER("cuDNN workspace rewriter");
  return CuDnnCustomCallVisitor(dnn_support_)
      .RunOnModule(module, execution_threads);
}

}  // namespace gpu
}  // namespace xla
