/* Copyright 2023 The OpenXLA Authors.

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

#include "xla/service/gpu/softmax_rewriter_triton.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_casting_utils.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/hlo/utils/hlo_query.h"
#include "xla/layout_util.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/gpu/hlo_traversal.h"
#include "xla/service/gpu/ir_emission_utils.h"
#include "xla/service/gpu/model/fusion_analysis_cache.h"
#include "xla/service/gpu/model/gpu_indexing_performance_model.h"
#include "xla/service/gpu/model/symbolic_tile_analysis.h"
#include "xla/service/gpu/model/tiled_hlo_computation.h"
#include "xla/service/gpu/triton_support.h"
#include "xla/service/instruction_fusion.h"
#include "xla/shape.h"
#include "xla/shape_util.h"
#include "xla/status_macros.h"
#include "xla/stream_executor/device_description.h"
#include "xla/util.h"
#include "xla/xla_data.pb.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/logging.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace gpu {
namespace {

using hlo_query::IsBroadcastOfParameter;
using hlo_query::IsBroadcastOfScalarConstant;

bool HasDefaultLayout(const Shape& shape) {
  return shape.has_layout() &&
         LayoutUtil::IsMonotonicWithDim0Major(shape.layout());
}

// Returns true if a trivially connected producer of 'consumer' with opcode
// 'opcode' exists. If such an instruction is found, the value of 'producer' is
// set to it. The definition of "trivial" operations is as given in
// 'IsTriviallyFusible'.
bool TrivialEdge(HloInstruction** producer, HloInstruction* consumer,
                 HloOpcode opcode, const se::GpuComputeCapability& gpu_version);

bool BitcastIsTilingNoop(HloInstruction* bitcast,
                         const se::GpuComputeCapability& gpu_version) {
  CHECK_EQ(bitcast->opcode(), HloOpcode::kBitcast);

  if (ShapeUtil::IsEffectiveScalar(bitcast->shape())) {
    return true;
  }

  // In the Softmax rewriter for now, tiling is derived from a hero reduction
  // operation, which should be reducing its input on the last axis. Therefore,
  // a bitcast is always a no-op with regards to a tile if
  //   (1) it does not change the size of the reduction dimension of its input
  //       (the last one); if its input is already reduced, then (1) is true
  //       by default
  //   (2) the layout of its output is ordered in the same way as the layout of
  //       its input. This is a fuzzy definition, but since we assume fusible
  //       ops to always have a default layout, we can just check if both the
  //       bitcast and its input have a default layout
  auto last_dimension = [](const HloInstruction* instr) {
    return instr->shape().dimensions().back();
  };

  HloInstruction* reduce = nullptr;
  TrivialEdge(&reduce, bitcast->mutable_operand(0), HloOpcode::kReduce,
              gpu_version);

  return (HasDefaultLayout(bitcast->shape()) &&
          HasDefaultLayout(bitcast->operand(0)->shape()) &&
          (reduce != nullptr ||
           last_dimension(bitcast->operand(0)) == last_dimension(bitcast)));
}

inline bool HasOneUse(const HloInstruction* instr) {
  return instr->user_count() == 1;
}

// Supports two types of broadcast of parameters. Either to one batch
// dim, or one reduction dim. For example the following cases are supported:
//
// Case #1:
// p = f32[a] parameter(0)
// b = f32[a,x] broadcast(p), dimensions={0}
//
// Case #2:
// p = f32[a] parameter(0)
// b = f32[x,a] broadcast(p), dimensions={1}
//
// Case #3:
// p = f32[a,b] parameter(0)
// b = f32[x,a,b] broadcast(p), dimensions={1,2}
//
// Other broadcast tiling patterns are currently unsupported.
// See b/328049138 for details.
//
// Unsupported case #1:
// p = f32[a] parameter(0)
// b = f32[x,a,y] broadcast(p), dimensions={1}
//
// Unsupported case #2:
// p = f32[a,b] parameter(0)
// b = f32[x,a,y,b] broadcast(p), dimensions={1,3}
//
// Unsupported case #3:
// p = f32[a] parameter(0)
// b = f32[x,y,a] broadcast(p), dimensions={2}
//
// Unsupported case #4:
// p = f32[a,b] parameter(0)
// b = f32[a,x,b] broadcast(p), dimensions={0,2}
bool IsBatchOrReductionDimBroadcast(const HloInstruction& hlo) {
  CHECK_EQ(hlo.opcode(), HloOpcode::kBroadcast)
      << "Expected broadcast " << hlo.ToShortString();
  CHECK_EQ(hlo.operand(0)->opcode(), HloOpcode::kParameter)
      << "Expected parameter " << hlo.operand(0)->ToShortString();

  const HloBroadcastInstruction* broadcast =
      Cast<HloBroadcastInstruction>(&hlo);

  const HloParameterInstruction* parameter =
      Cast<HloParameterInstruction>(hlo.operand(0));

  // Support only one dim broadcast.
  if (parameter->shape().dimensions_size() + 1 !=
      broadcast->shape().dimensions_size()) {
    return false;
  }

  // It is enough to ensure that the broadcast does not preserve both last, and
  // first dimensions of the parameter at the same time. Otherwise the broadcast
  // is the unsupported case #4.
  //
  // Preserve the first dim:
  //   p = f32[a,b] parameter(0)
  //   b1 = f32[a,b,c] broadcast(p), dimensions={0,1}
  bool preserve_first_dim = broadcast->dimensions().front() == 0;
  // Preserve the last dim:
  //   p = f32[a,b] parameter(0)
  //   b1 = f32[c,a,b] broadcast(p), dimensions={1,2}
  bool preserve_last_dim = broadcast->dimensions().back() ==
                           broadcast->shape().dimensions_size() - 1;
  // We do not want to preserve both first and last dim, as it means the
  // broadcast is not expanding on outermost dims.
  return !(preserve_first_dim && preserve_last_dim);
}

bool IsBroadcastOfAScalar(const HloInstruction& hlo) {
  CHECK_EQ(hlo.opcode(), HloOpcode::kBroadcast)
      << "Expected broadcast " << hlo.ToShortString();
  return ShapeUtil::IsScalar(hlo.operand(0)->shape());
}

bool IsSingleRowParameterBroadcast(const HloInstruction& hlo) {
  CHECK_EQ(hlo.opcode(), HloOpcode::kBroadcast)
      << "Expected broadcast " << hlo.ToShortString();
  CHECK_EQ(hlo.operand(0)->opcode(), HloOpcode::kParameter)
      << "Expected parameter " << hlo.operand(0)->ToShortString();

  const HloBroadcastInstruction* broadcast =
      Cast<HloBroadcastInstruction>(&hlo);
  const HloParameterInstruction* parameter =
      Cast<HloParameterInstruction>(hlo.operand(0));

  if (parameter->shape().dimensions_size() != 1) {
    return false;
  }
  return broadcast->dimensions()[0] == broadcast->shape().dimensions_size() - 1;
}

bool IsSupportedBroadcastOfParameter(const HloInstruction& hlo) {
  return IsBroadcastOfParameter(hlo) &&
         (IsBatchOrReductionDimBroadcast(hlo) || IsBroadcastOfAScalar(hlo) ||
          IsSingleRowParameterBroadcast(hlo));
}

// Chooses which operand to use for fusion processing. Taking in a unary or
// binary instruction, returns the first non-splat operand. If none is
// present, returns any operand.
HloInstruction* ChooseOperandForFusionProcessing(HloInstruction* instr) {
  CHECK_GT(instr->operand_count(), 0);
  CHECK_LE(instr->operand_count(), 2);

  // TODO(b/326217416): Extend the broadcast of splat constants/parameters to a
  // broadcast of any op.
  if (instr->operand_count() > 1 &&
      (IsBroadcastOfScalarConstant(*instr->operand(0)) ||
       IsSupportedBroadcastOfParameter(*instr->operand(0)))) {
    return instr->mutable_operand(1);
  }
  return instr->mutable_operand(0);
}

bool IsTriviallyFusible(HloInstruction* instr,
                        const se::GpuComputeCapability& gpu_version,
                        int num_allowed_users = 1) {
  // Checks whether an op is trivially fusible. An op is said to be trivially
  // fusible if it does not increase the amount of memory read/written by the
  // resulting fusion, is compatible with any chosen tiling, and can be
  // codegen'd using Triton. The op is allowed to have up to num_allowed_users
  // users.
  if (instr->user_count() > num_allowed_users ||
      !HasDefaultLayout(instr->shape())) {
    return false;
  }

  if (instr->opcode() == HloOpcode::kBitcast &&
      BitcastIsTilingNoop(instr, gpu_version)) {
    return true;
  }

  if (instr->IsElementwise() && instr->operand_count() == 1) {
    return static_cast<bool>(IsTritonSupportedInstruction(*instr, gpu_version));
  }

  // Elementwise binary ops are trivially fusible if the operands are the same,
  // or if exactly one of the operands is a splat constant.
  if (instr->IsElementwiseBinary()) {
    const HloInstruction* operand_0 = instr->operand(0);
    const HloInstruction* operand_1 = instr->operand(1);

    // Elementwise binary ops should be fused if both operands are the same and
    // if the operand is triton supported.
    if (operand_0 == operand_1) {
      return static_cast<bool>(
          IsTritonSupportedInstruction(*instr, gpu_version));
    }

    // For simplicity we only fuse elementwise binary ops with splat operands
    // if they contain one non-splat operand.
    // TODO(b/326217416): Extend the broadcast of splat constants/parameters to
    // a broadcast of any op.
    if ((IsBroadcastOfScalarConstant(*operand_0) ||
         IsSupportedBroadcastOfParameter(*operand_0)) ^
        (IsBroadcastOfScalarConstant(*operand_1) ||
         IsSupportedBroadcastOfParameter(*operand_1))) {
      return static_cast<bool>(
          IsTritonSupportedInstruction(*instr, gpu_version));
    }
  }

  return false;
}

bool TrivialEdge(HloInstruction** producer, HloInstruction* consumer,
                 HloOpcode opcode,
                 const se::GpuComputeCapability& gpu_version) {
  while (consumer->opcode() != opcode) {
    if (IsTriviallyFusible(consumer, gpu_version)) {
      consumer = ChooseOperandForFusionProcessing(consumer);
    } else {
      return false;
    }
  }

  *producer = consumer;
  return true;
}

bool IsTriviallyConnectedProducerOf(
    HloInstruction* producer, HloInstruction* consumer,
    const se::GpuComputeCapability& gpu_version) {
  if (producer == consumer) {
    return true;
  }

  HloInstruction* found_producer = consumer;
  while (
      TrivialEdge(&found_producer, consumer, producer->opcode(), gpu_version)) {
    if (found_producer == producer) {
      return true;
    }

    if (!IsTriviallyFusible(found_producer, gpu_version)) {
      return false;
    }

    consumer = found_producer->mutable_operand(0);
  }

  return false;
}

// Finds the first non-fusible producer of a diamond. This instruction is either
//   1. the direct producer of the diamond, if that producer is used more than
//      twice and/or is not otherwise trivially fusible
//   2. the first parent instruction of the producer of the diamond such that
//      that instruction is used more than once, and/or is not trivially
//      fusible.
HloInstruction* FindFirstNonFusibleDiamondProducer(
    HloInstruction* diamond_producer,
    const se::GpuComputeCapability& gpu_version) {
  if (IsTriviallyFusible(diamond_producer, gpu_version,
                         /*num_allowed_users=*/2)) {
    diamond_producer = ChooseOperandForFusionProcessing(diamond_producer);
    while (IsTriviallyFusible(diamond_producer, gpu_version)) {
      diamond_producer = ChooseOperandForFusionProcessing(diamond_producer);
    }
  }

  return diamond_producer;
}

// Creates a fusion corresponding to the input diamond chain. The resulting
// fusion instruction is added to the module, but is not yet inserted into the
// graph as a replacement of the original instructions.
//
// TODO(b/347956491): this awkward abstraction is needed to work around
// limitations of HloFusionAdaptor, which underpins the implementation of
// SymbolicTileAnalysis. We need to come up with a better solution.
absl::StatusOr<HloFusionInstruction*> MakeFusionForDiamondChain(
    const DiamondChainDescriptor& diamond_chain) {
  auto [root, producer] = diamond_chain;

  std::string suggested_name = "triton_softmax";
  HloComputation::Builder builder(absl::StrCat(suggested_name, "_computation"));
  // Original instruction -> fused one.
  absl::flat_hash_map<const HloInstruction*, HloInstruction*>
      old_to_new_mapping;

  int param = 0;
  old_to_new_mapping[producer] =
      builder.AddInstruction(HloInstruction::CreateParameter(
          param, producer->shape(), absl::StrCat("parameter_", param)));
  param++;

  std::vector<HloInstruction*> parameters = {producer};

  std::function<void(HloInstruction*)> create_computation =
      [&](HloInstruction* instr) -> void {
    if (old_to_new_mapping.contains(instr)) {
      return;
    }
    std::vector<HloInstruction*> new_operands;
    for (HloInstruction* operand : instr->mutable_operands()) {
      create_computation(operand);
      new_operands.push_back(old_to_new_mapping[operand]);
    }
    if (instr->opcode() == HloOpcode::kParameter) {
      old_to_new_mapping[instr] =
          builder.AddInstruction(HloInstruction::CreateParameter(
              param, instr->shape(), absl::StrCat("parameter_", param)));
      parameters.push_back(instr);
      param++;
    } else {
      old_to_new_mapping[instr] = builder.AddInstruction(
          instr->CloneWithNewOperands(instr->shape(), new_operands));
    }
  };
  create_computation(root);

  HloComputation* computation =
      root->GetModule()->AddComputationAndUnifyNamesAndIds(builder.Build(),
                                                           /*is_entry=*/false);

  HloInstruction* softmax_fusion =
      root->parent()->AddInstruction(HloInstruction::CreateFusion(
          root->shape(), HloInstruction::FusionKind::kCustom, parameters,
          computation));

  softmax_fusion->GetModule()->SetAndUniquifyInstrName(softmax_fusion,
                                                       "triton_softmax");
  TF_ASSIGN_OR_RETURN(auto gpu_config,
                      softmax_fusion->backend_config<GpuBackendConfig>());
  FusionBackendConfig& backend_config =
      *gpu_config.mutable_fusion_backend_config();
  backend_config.set_kind(std::string(kTritonFusionKind));
  TF_RETURN_IF_ERROR(softmax_fusion->set_backend_config(gpu_config));
  return xla::Cast<HloFusionInstruction>(softmax_fusion);
}

absl::Status FuseDiamondChainImpl(
    const DiamondChainDescriptor& diamond_chain,
    GpuPerformanceModelWithIndexingAnalysis& indexing_performance_model) {
  TF_ASSIGN_OR_RETURN(HloFusionInstruction * softmax_fusion,
                      MakeFusionForDiamondChain(diamond_chain));
  HloInstruction* root = diamond_chain.root;

  auto fusion_adaptor = HloFusionAdaptor::ForInstruction(softmax_fusion);

  TF_ASSIGN_OR_RETURN(
      TiledRunTimeDataOrError tiled_runtime_data_or,
      indexing_performance_model.TryFindBestTilingForFusion(*fusion_adaptor));

  if (const auto* fusion_decision =
          std::get_if<FusionDecision>(&tiled_runtime_data_or)) {
    return absl::FailedPreconditionError(absl::StrCat(
        "SymbolicTileAnalysis failed. ", fusion_decision->Explain()));
  }

  TiledRunTimeData tiled_runtime_data =
      std::get<TiledRunTimeData>(std::move(tiled_runtime_data_or));

  TF_ASSIGN_OR_RETURN(auto backend_config,
                      softmax_fusion->backend_config<GpuBackendConfig>());
  *backend_config.mutable_fusion_backend_config()
       ->mutable_block_level_fusion_config() =
      tiled_runtime_data.block_level_parameters.ToBlockLevelFusionConfig();
  TF_RETURN_IF_ERROR(softmax_fusion->set_backend_config(backend_config));

  if (root->IsRoot()) {
    root->parent()->set_root_instruction(softmax_fusion);
    TF_RETURN_IF_ERROR(
        root->parent()->RemoveInstructionAndUnusedOperands(root));
  } else {
    TF_RETURN_IF_ERROR(
        root->parent()->ReplaceInstruction(root, softmax_fusion));
  }

  VLOG(5) << softmax_fusion->ToString();
  return absl::OkStatus();
}

// Returns `true` if the diamond chain passed as a parameter can be tiled
// correctly using `SymbolicTileAnalysis`.
absl::StatusOr<bool> CanSymbolicTileAnalysisTileDiamondChain(
    const DiamondChainDescriptor& diamond_chain) {
  TF_ASSIGN_OR_RETURN(HloFusionInstruction * softmax_fusion,
                      MakeFusionForDiamondChain(diamond_chain));
  mlir::MLIRContext context;
  SymbolicTileAnalysisOrError symbolic_tile_analysis_or_error =
      SymbolicTileAnalysis::AnalyzeComputation(
          *softmax_fusion->called_computation(), &context);

  bool can_tile = std::holds_alternative<SymbolicTileAnalysis>(
      symbolic_tile_analysis_or_error);

  TF_RETURN_IF_ERROR(diamond_chain.root->GetModule()->RemoveEmbeddedComputation(
      softmax_fusion->called_computation()));
  TF_RETURN_IF_ERROR(
      diamond_chain.root->parent()->RemoveInstruction(softmax_fusion));

  return can_tile;
}

DiamondMatchingDecision MatchesTritonCompatibleClosedReductionDiamondImpl(
    HloInstruction* instr, const se::GpuComputeCapability& cc) {
  if (!instr->IsElementwiseBinary()) {
    return "Root is not elementwise binary.";
  }

  if (!IsTritonSupportedInstruction(*instr, cc)) {
    return "Root is not supported for Triton instruction.";
  }

  HloInstruction* producer;
  HloInstruction* broadcast;
  HloInstruction* reduce;

  if (!TrivialEdge(&broadcast, instr->mutable_operand(1), HloOpcode::kBroadcast,
                   cc)) {
    return "Could not find a trivial connection from root to a broadcast.";
  }

  if (!TrivialEdge(&reduce, broadcast->mutable_operand(0), HloOpcode::kReduce,
                   cc)) {
    return "Could not find a trivial connection from matched broadcast to a "
           "reduction.";
  }

  if (!(HasDefaultLayout(broadcast->shape()) &&
        HasDefaultLayout(reduce->shape()))) {
    return "Broadcast or reduce have non-default layouts.";
  }

  if (CodegenDecision is_supported = IsTritonSupportedInstruction(*reduce, cc);
      !is_supported) {
    VLOG(3) << is_supported.Explain();
    return is_supported;
  }

  if (!HasOneUse(broadcast) || !HasOneUse(reduce)) {
    return "More than one use of broadcast or reduce.";
  }

  producer = reduce->mutable_operand(0);

  if (absl::c_linear_search(broadcast->dimensions(),
                            broadcast->shape().rank() - 1)) {
    return "Broadcast is not along the reduction dimension.";
  }

  while (IsTriviallyFusible(producer, cc)) {
    producer = ChooseOperandForFusionProcessing(producer);
  }

  if (!HasDefaultLayout(producer->shape())) {
    return "Producer has non-default layout.";
  }

  if (!IsTriviallyConnectedProducerOf(producer, instr->mutable_operand(0),
                                      cc)) {
    return "Producer is not trivially connected.";
  }

  if (producer != instr->operand(0) && instr->operand(0)->user_count() != 1) {
    return "Unsupported root-producer connection.";
  }

  VLOG(5) << "Matched Softmax diamond with: ";
  VLOG(5) << "root: " << instr->ToString();
  VLOG(5) << "producer: " << producer->ToString();
  VLOG(5) << "broadcast: " << broadcast->ToString();
  VLOG(5) << "reduce: " << reduce->ToString();

  return producer;
}

// Returns a vector containing all the single diamonds in the parameter module.
// The diamonds are returned in def-before-use order, and grouped by
// computation.
absl::StatusOr<std::vector<DiamondChainDescriptor>> FindAllFusibleDiamonds(
    HloModule& module,
    const absl::flat_hash_set<absl::string_view>& execution_threads,
    const se::GpuComputeCapability& cc) {
  std::vector<DiamondChainDescriptor> matched_diamonds;

  for (HloComputation* comp :
       module.MakeNonfusionComputations(execution_threads)) {
    if (comp->IsCustomCallComputation()) {
      continue;
    }
    for (HloInstruction* instr : comp->MakeInstructionPostOrder()) {
      auto producer =
          MatchesTritonCompatibleClosedReductionDiamondImpl(instr, cc);
      if (std::holds_alternative<HloInstruction*>(producer)) {
        DiamondChainDescriptor diamond_chain{
            /*root=*/instr, /*producer=*/std::get<HloInstruction*>(producer)};
        // We filter out the diamond chains that cannot be tiled correctly using
        // `SymbolicTileAnalysis`.
        TF_ASSIGN_OR_RETURN(
            bool can_tile_diamond_chain,
            CanSymbolicTileAnalysisTileDiamondChain(diamond_chain));
        if (can_tile_diamond_chain) {
          matched_diamonds.push_back(diamond_chain);
        } else {
          VLOG(5) << "Cannot tile the diamond pattern described by "
                  << "instructions " << instr->ToString() << " and "
                  << std::get<HloInstruction*>(producer)->ToString() << ".";
          continue;
        }

      } else {
        VLOG(5) << "Cannot match the diamond pattern for instruction "
                << instr->ToString()
                << ". Reason: " << std::get<FusionDecision>(producer).Explain();
      }
    }
  }

  return std::move(matched_diamonds);
}

// Returns the size of the reduction dimension of the input diamond.
int64_t GetReductionDimensionSizeForDiamond(
    const DiamondChainDescriptor& diamond_chain) {
  HloInstruction* diamond_root = diamond_chain.root;
  HloInstruction* instr = diamond_root->mutable_operand(1);
  while (instr->opcode() != HloOpcode::kReduce) {
    instr = ChooseOperandForFusionProcessing(instr);
  }

  int operand_rank = instr->operand(0)->shape().rank();
  CHECK_EQ(instr->dimensions().size(), 1);
  CHECK_EQ(instr->dimensions(0), operand_rank - 1);
  return instr->operand(0)->shape().dimensions(operand_rank - 1);
}

// Returns a pointer to the last user of `instr` that is trivially fusible.
HloInstruction* GetLastTriviallyFusibleUser(
    HloInstruction* instr, const se::GpuComputeCapability& cc) {
  while (HasOneUse(instr) && !instr->IsRoot() &&
         IsTriviallyFusible(instr->users().front(), cc)) {
    instr = instr->users().front();
  }

  // We do not care about the number of users for the last instruction of the
  // fusion, so attempt to fuse one more instruction with this relaxed
  // restriction.
  if (HasOneUse(instr) && !instr->IsRoot() &&
      IsTriviallyFusible(
          instr->users().front(), cc,
          /*num_allowed_users=*/instr->users().front()->user_count())) {
    instr = instr->users().front();
  }
  return instr;
}

}  // anonymous namespace

DiamondMatchingDecision
SoftmaxRewriterTriton::MatchesTritonCompatibleClosedReductionDiamond(
    HloInstruction* instr) const {
  return MatchesTritonCompatibleClosedReductionDiamondImpl(
      instr, device_info_.gpu_compute_capability());
}

absl::StatusOr<std::vector<DiamondChainDescriptor>>
SoftmaxRewriterTriton::FindAllFusibleDiamondChains(
    HloModule& module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) const {
  const se::GpuComputeCapability& cc = device_info_.gpu_compute_capability();
  TF_ASSIGN_OR_RETURN(std::vector<DiamondChainDescriptor> matched_diamonds,
                      FindAllFusibleDiamonds(module, execution_threads, cc));

  if (matched_diamonds.empty()) {
    return std::vector<DiamondChainDescriptor>();
  }

  // If we matched several diamonds, it may be possible for some of them to be
  // fused together. This is the case if the following conditions hold:
  //   1. The path between the root of diamond n towards the producer of
  //      diamond n+1 is composed only of trivially fusible operations. In that
  //      case, the first non-trivially fusible producer of diamond n+1 must be
  //      exactly the root of diamond n.
  //   2. The root of diamond n/first non-fusible producer of diamond n+1 must
  //      have
  //        a. exactly one user if it is not exactly the producer of diamond
  //           n+1;
  //        b/ exactly two users otherwise.
  //   3. The axis being reduced must have the same length in all the diamonds
  //      being fused together.
  //
  // Crucially, this approach relies on a diamond root never being considered a
  // trivially fusible operation.
  std::vector<DiamondChainDescriptor> diamond_chains;
  diamond_chains.reserve(matched_diamonds.size());

  HloInstruction* current_fusion_producer =
      FindFirstNonFusibleDiamondProducer(matched_diamonds.front().producer, cc);
  int current_reduce_dimension_size =
      GetReductionDimensionSizeForDiamond(matched_diamonds.front());

  for (int diamond_idx = 1; diamond_idx < matched_diamonds.size();
       ++diamond_idx) {
    HloInstruction* diamond_producer = matched_diamonds[diamond_idx].producer;
    HloInstruction* previous_diamond_root =
        matched_diamonds[diamond_idx - 1].root;

    HloInstruction* first_non_fusible_diamond_producer =
        FindFirstNonFusibleDiamondProducer(diamond_producer, cc);

    int diamond_reduce_dimension_size =
        GetReductionDimensionSizeForDiamond(matched_diamonds[diamond_idx]);

    if (first_non_fusible_diamond_producer == previous_diamond_root &&  // 1
        ((first_non_fusible_diamond_producer != diamond_producer &&
          HasOneUse(first_non_fusible_diamond_producer)) ||  // 2.a
         (first_non_fusible_diamond_producer == diamond_producer &&
          first_non_fusible_diamond_producer->user_count() == 2)) &&  // 2.b
        diamond_reduce_dimension_size == current_reduce_dimension_size) {  // 3
      continue;
    }

    // The "last trivially fusible user" chain of diamond chain n should never
    // intersect with the "first non fusible diamond producer" chain of diamond
    // chain n+1: if these chains intersected, then all the intermediate ops
    // between the diamond chains could be trivially fused, and both diamond
    // chains could be fused into a single diamond chain. Note that this only
    // holds insofar as we do not allow fusing in bitcasts that modify the last
    // dimension of the input array. It is however possible for the last
    // trivially fusible user of diamond chain n to be the first non fusible
    // diamond producer of diamond chain n+1.
    diamond_chains.push_back(DiamondChainDescriptor{
        GetLastTriviallyFusibleUser(previous_diamond_root, cc),
        current_fusion_producer,
    });

    current_fusion_producer = first_non_fusible_diamond_producer;
    current_reduce_dimension_size = diamond_reduce_dimension_size;
  }

  // The last diamond chain is still open; close it.
  diamond_chains.push_back(DiamondChainDescriptor{
      GetLastTriviallyFusibleUser(matched_diamonds.back().root, cc),
      current_fusion_producer});

  // We filter out the diamond chains that cannot be tiled correctly using
  // `SymbolicTileAnalysis`.
  std::vector<DiamondChainDescriptor> filtered_diamond_chains;
  for (const DiamondChainDescriptor& diamond_chain : diamond_chains) {
    TF_ASSIGN_OR_RETURN(bool can_tile_diamond_chain,
                        CanSymbolicTileAnalysisTileDiamondChain(diamond_chain));
    if (can_tile_diamond_chain) {
      filtered_diamond_chains.push_back(diamond_chain);
    }
  }
  return filtered_diamond_chains;
}

absl::Status SoftmaxRewriterTriton::FuseDiamondChain(
    const DiamondChainDescriptor& diamond_chain) {
  HloFusionAnalysisCache fusion_analysis_cache(device_info_);
  GpuPerformanceModelWithIndexingAnalysis indexing_performance_model(
      &device_info_, &fusion_analysis_cache, shape_size_, &mlir_context_);

  return FuseDiamondChainImpl(diamond_chain, indexing_performance_model);
}

absl::StatusOr<bool> SoftmaxRewriterTriton::Run(
    HloModule* module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  TF_RETURN_IF_ERROR(EnsureTritonSupportsComputeCapability(
      device_info_.gpu_compute_capability()));

  TF_ASSIGN_OR_RETURN(std::vector<DiamondChainDescriptor> diamond_chains,
                      FindAllFusibleDiamondChains(*module, execution_threads));

  if (diamond_chains.empty()) {
    return false;
  }

  // The diamond chains must be emitted in reverse order, to make sure that
  // producer instructions are emitted correctly when the root of
  // diamond chain n is exactly the producer of diamond chain n+1.
  for (auto diamond_chain = diamond_chains.rbegin();
       diamond_chain != diamond_chains.rend(); ++diamond_chain) {
    TF_RET_CHECK(FuseDiamondChain(*diamond_chain).ok());
  }
  return true;
}

}  // namespace gpu
}  // namespace xla
