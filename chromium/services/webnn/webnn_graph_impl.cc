// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <math.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/dcheck_is_on.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_buffer_impl.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/graph_impl_dml.h"
#endif

namespace webnn {

namespace {

// Maps the id to its `mojo::Operand`.
using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;

webnn::InputOperandLayout MojoInputOperandLayoutToComponent(
    webnn::mojom::InputOperandLayout layout) {
  switch (layout) {
    case webnn::mojom::InputOperandLayout::kChannelsFirst:
      return webnn::InputOperandLayout::kNchw;
    case webnn::mojom::InputOperandLayout::kChannelsLast:
      return webnn::InputOperandLayout::kNhwc;
  }
  NOTREACHED_NORETURN();
}

webnn::ReduceKind MojoReduceTypeToComponent(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return webnn::ReduceKind::kL1;
    case mojom::Reduce::Kind::kL2:
      return webnn::ReduceKind::kL2;
    case mojom::Reduce::Kind::kLogSum:
      return webnn::ReduceKind::kLogSum;
    case mojom::Reduce::Kind::kLogSumExp:
      return webnn::ReduceKind::kLogSumExp;
    case mojom::Reduce::Kind::kMax:
      return webnn::ReduceKind::kMax;
    case mojom::Reduce::Kind::kMean:
      return webnn::ReduceKind::kMean;
    case mojom::Reduce::Kind::kMin:
      return webnn::ReduceKind::kMin;
    case mojom::Reduce::Kind::kProduct:
      return webnn::ReduceKind::kProduct;
    case mojom::Reduce::Kind::kSum:
      return webnn::ReduceKind::kSum;
    case mojom::Reduce::Kind::kSumSquare:
      return webnn::ReduceKind::kSumSquare;
  }
  NOTREACHED_NORETURN();
}

webnn::RecurrentNetworkDirection MojoRecurrentNetworkDirectionToComponent(
    mojom::RecurrentNetworkDirection direction) {
  switch (direction) {
    case mojom::RecurrentNetworkDirection::kForward:
      return webnn::RecurrentNetworkDirection::kForward;
    case mojom::RecurrentNetworkDirection::kBackward:
      return webnn::RecurrentNetworkDirection::kBackward;
    case mojom::RecurrentNetworkDirection::kBoth:
      return webnn::RecurrentNetworkDirection::kBoth;
  }
}

bool ValidateClampAttributes(const mojom::Clamp& clamp) {
  if (std::isnan(clamp.min_value) || std::isnan(clamp.max_value)) {
    // The min or max value are nan.
    return false;
  }
  if (clamp.min_value >= clamp.max_value) {
    // The min value must be below the max value.
    return false;
  }
  return true;
}

bool ValidateEluAttributes(const mojom::Elu& elu) {
  if (std::isnan(elu.alpha) || elu.alpha <= 0.0f) {
    // The value of alpha must be greater than 0.
    return false;
  }

  return true;
}

bool ValidateHardSigmoidAttributes(const mojom::HardSigmoid& hard_sigmoid) {
  if (std::isnan(hard_sigmoid.alpha) || std::isnan(hard_sigmoid.beta)) {
    // The value of alpha and beta should not be NAN.
    return false;
  }

  return true;
}

bool ValidateLeakyReluAttributes(const mojom::LeakyRelu& leaky_relu) {
  if (std::isnan(leaky_relu.alpha)) {
    // The value of alpha should not be NAN.
    return false;
  }

  return true;
}

bool ValidateLinearAttributes(const mojom::Linear& linear) {
  if (std::isnan(linear.alpha) || std::isnan(linear.beta)) {
    // The values of alpha and beta should not be NAN.
    return false;
  }

  return true;
}

bool ValidateActivation(const mojom::Activation& activation) {
  switch (activation.which()) {
    case mojom::Activation::Tag::kElu:
      return ValidateEluAttributes(*activation.get_elu());
    case mojom::Activation::Tag::kHardSigmoid:
      return ValidateHardSigmoidAttributes(*activation.get_hard_sigmoid());
    case mojom::Activation::Tag::kLeakyRelu:
      return ValidateLeakyReluAttributes(*activation.get_leaky_relu());
    case mojom::Activation::Tag::kLinear:
      return ValidateLinearAttributes(*activation.get_linear());
    case mojom::Activation::Tag::kGelu:
    case mojom::Activation::Tag::kRelu:
    case mojom::Activation::Tag::kSigmoid:
    case mojom::Activation::Tag::kSoftplus:
    case mojom::Activation::Tag::kSoftsign:
    case mojom::Activation::Tag::kTanh:
      return true;
  }
}

const mojom::Operand* GetMojoOperand(const IdToOperandMap& id_to_operand_map,
                                     uint64_t operand_id) {
  const auto operand_iterator = id_to_operand_map.find(operand_id);
  if (operand_iterator == id_to_operand_map.end()) {
    // There is no operand for the id.
    return nullptr;
  }
  return operand_iterator->second.get();
}

webnn::BatchNormalizationAttributes ConvertToBatchNormalizationAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::BatchNormalization& batch_normalization) {
  webnn::BatchNormalizationAttributes component_attributes;
  const auto& scale_operand_id = batch_normalization.scale_operand_id;
  if (scale_operand_id) {
    const mojom::Operand& scale_operand =
        *id_to_operand_map.at(scale_operand_id.value());
    component_attributes.scale = scale_operand.descriptor;
  }
  const auto& bias_operand_id = batch_normalization.bias_operand_id;
  if (bias_operand_id) {
    const mojom::Operand& bias_operand =
        *id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias = bias_operand.descriptor;
  }
  component_attributes.axis = batch_normalization.axis;
  component_attributes.label = batch_normalization.label;

  return component_attributes;
}

template <typename Conv2dAttributesType>
Conv2dAttributesType ConvertToConv2dAttributes(
    const webnn::ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2d& conv2d,
    std::optional<OperandDescriptor> bias_operand) {
  Conv2dAttributesType attributes_base;
  // Convert padding, strides, dilations.
  auto& mojo_padding = conv2d.padding;
  attributes_base.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = mojo_padding->beginning->height,
                                  .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d<uint32_t>{.height = mojo_padding->ending->height,
                                        .width = mojo_padding->ending->width}};
  attributes_base.strides = webnn::Size2d<uint32_t>{
      .height = conv2d.strides->height, .width = conv2d.strides->width};
  attributes_base.dilations = webnn::Size2d<uint32_t>{
      .height = conv2d.dilations->height, .width = conv2d.dilations->width};

  // Convert groups, input layout and bias.
  attributes_base.groups = conv2d.groups;
  attributes_base.input_layout = context_properties.input_operand_layout;
  attributes_base.bias_operand = std::move(bias_operand);

  attributes_base.label = conv2d.label;

  return std::move(attributes_base);
}

webnn::Conv2dAttributes ConvertToConv2dAttributes(
    const webnn::ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2d& conv2d,
    std::optional<OperandDescriptor> bias_operand) {
  auto component_attributes =
      ConvertToConv2dAttributes<webnn::Conv2dAttributes>(
          context_properties, id_to_operand_map, conv2d,
          std::move(bias_operand));
  switch (context_properties.input_operand_layout) {
    case webnn::InputOperandLayout::kNchw:
      // "channelsFirst": [batches, input_channels, height, width]
      component_attributes.filter_layout = Conv2dFilterOperandLayout::kOihw;
      break;
    case webnn::InputOperandLayout::kNhwc:
      // "channelsLast": [batches, height, width, input_channels]
      // For regular conv2d, ohwi filter layout is expected by default.
      // For depthwise conv2d, ihwo filter layout is expected by default.
      const auto* const input =
          GetMojoOperand(id_to_operand_map, conv2d.input_operand_id);
      CHECK(input);
      CHECK_EQ(input->descriptor.Rank(), 4u);
      const uint32_t input_channels = input->descriptor.shape()[3];
      const auto* const output =
          GetMojoOperand(id_to_operand_map, conv2d.output_operand_id);
      CHECK(output);
      CHECK_EQ(output->descriptor.Rank(), 4u);
      const uint32_t output_channels = output->descriptor.shape()[3];
      // Depthwise conv2d is "options.groups == input_channels ==
      // output_channels".
      const bool depthwise = webnn::IsDepthwiseConv2d(
          input_channels, output_channels, conv2d.groups);
      component_attributes.filter_layout =
          depthwise ? Conv2dFilterOperandLayout::kIhwo
                    : Conv2dFilterOperandLayout::kOhwi;
      break;
  }
  return component_attributes;
}

webnn::LstmAttributes ConvertToLstmAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Lstm& lstm) {
  webnn::LstmAttributes attributes;
  attributes.return_sequence = lstm.return_sequence;
  attributes.direction =
      MojoRecurrentNetworkDirectionToComponent(lstm.direction);
  attributes.activation_count = lstm.activations.size();

  if (lstm.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, lstm.bias_operand_id.value());
    attributes.bias = bias->descriptor;
  }
  if (lstm.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, lstm.recurrent_bias_operand_id.value());
    attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  if (lstm.peephole_weight_operand_id.has_value()) {
    const auto* peephole_weight = GetMojoOperand(
        id_to_operand_map, lstm.peephole_weight_operand_id.value());
    attributes.peephole_weight = peephole_weight->descriptor;
  }
  if (lstm.initial_hidden_state_operand_id.has_value()) {
    const auto* initial_hidden_state = GetMojoOperand(
        id_to_operand_map, lstm.initial_hidden_state_operand_id.value());
    attributes.initial_hidden_state = initial_hidden_state->descriptor;
  }
  if (lstm.initial_cell_state_operand_id.has_value()) {
    const auto* initial_cell_state = GetMojoOperand(
        id_to_operand_map, lstm.initial_cell_state_operand_id.value());
    attributes.initial_cell_state = initial_cell_state->descriptor;
  }

  return attributes;
}

webnn::LstmCellAttributes ConvertToLstmCellAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::LstmCell& lstm_cell) {
  webnn::LstmCellAttributes attributes;
  attributes.activation_count = lstm_cell.activations.size();

  if (lstm_cell.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, lstm_cell.bias_operand_id.value());
    attributes.bias = bias->descriptor;
  }
  if (lstm_cell.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, lstm_cell.recurrent_bias_operand_id.value());
    attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  if (lstm_cell.peephole_weight_operand_id.has_value()) {
    const auto* peephole_weight = GetMojoOperand(
        id_to_operand_map, lstm_cell.peephole_weight_operand_id.value());
    attributes.peephole_weight = peephole_weight->descriptor;
  }

  return attributes;
}

webnn::ConvTranspose2dAttributes ConvertToConvTranspose2dAttributes(
    const webnn::ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2d& conv2d,
    std::optional<OperandDescriptor> bias_operand) {
  auto component_attributes =
      ConvertToConv2dAttributes<webnn::ConvTranspose2dAttributes>(
          context_properties, id_to_operand_map, conv2d,
          std::move(bias_operand));

  // Convert the output sizes that fetched from dimensions of output operand.
  auto* output = GetMojoOperand(id_to_operand_map, conv2d.output_operand_id);
  CHECK_EQ(output->descriptor.Rank(), 4u);
  webnn::Size2d<uint32_t> output_sizes;
  switch (context_properties.input_operand_layout) {
    case webnn::InputOperandLayout::kNchw:
      // "channelsFirst": [batches, input_channels, height, width]
      output_sizes.height = output->descriptor.shape()[2];
      output_sizes.width = output->descriptor.shape()[3];
      component_attributes.filter_layout =
          ConvTranspose2dFilterOperandLayout::kIohw;
      break;
    case webnn::InputOperandLayout::kNhwc:
      // "channelsLast": [batches, height, width, input_channels]
      output_sizes.height = output->descriptor.shape()[1];
      output_sizes.width = output->descriptor.shape()[2];
      component_attributes.filter_layout =
          ConvTranspose2dFilterOperandLayout::kOhwi;
      break;
  }
  component_attributes.output_sizes = std::move(output_sizes);

  return component_attributes;
}

webnn::LayerNormalizationAttributes ConvertToLayerNormalizationAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::LayerNormalization& layer_normalization) {
  webnn::LayerNormalizationAttributes component_attributes;
  const auto& scale_operand_id = layer_normalization.scale_operand_id;
  if (scale_operand_id.has_value()) {
    const mojom::Operand& scale_operand =
        *id_to_operand_map.at(scale_operand_id.value());
    component_attributes.scale = scale_operand.descriptor;
  }

  const auto& bias_operand_id = layer_normalization.bias_operand_id;
  if (bias_operand_id.has_value()) {
    const mojom::Operand& bias_operand =
        *id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias = bias_operand.descriptor;
  }

  return component_attributes;
}

webnn::Pool2dAttributes ConvertToPool2dAttributes(
    const webnn::ContextProperties& context_properties,
    const webnn::mojom::Pool2d& pool2d,
    const mojom::Operand* output) {
  webnn::Pool2dAttributes component_attributes;
  auto& window_dimensions = pool2d.window_dimensions;
  component_attributes.window_dimensions = webnn::Size2d<uint32_t>{
      .height = window_dimensions->height, .width = window_dimensions->width};
  auto& mojo_padding = pool2d.padding;
  component_attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = mojo_padding->beginning->height,
                                  .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d<uint32_t>{.height = mojo_padding->ending->height,
                                        .width = mojo_padding->ending->width}};
  component_attributes.strides = webnn::Size2d<uint32_t>{
      .height = pool2d.strides->height, .width = pool2d.strides->width};
  component_attributes.dilations = webnn::Size2d<uint32_t>{
      .height = pool2d.dilations->height, .width = pool2d.dilations->width};
  component_attributes.layout = context_properties.input_operand_layout;
  CHECK_EQ(output->descriptor.Rank(), 4u);
  switch (component_attributes.layout) {
    case webnn::InputOperandLayout::kNchw:
      component_attributes.output_sizes =
          webnn::Size2d<uint32_t>{.height = output->descriptor.shape()[2],
                                  .width = output->descriptor.shape()[3]};
      break;
    case webnn::InputOperandLayout::kNhwc:
      component_attributes.output_sizes =
          webnn::Size2d<uint32_t>{.height = output->descriptor.shape()[1],
                                  .width = output->descriptor.shape()[2]};
      break;
  }
  return component_attributes;
}

webnn::GemmAttributes ConvertToGemmAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::Gemm& gemm) {
  webnn::GemmAttributes component_attributes;
  auto& c_operand_id = gemm.c_operand_id;
  if (c_operand_id) {
    const mojom::Operand& c_operand =
        *id_to_operand_map.at(c_operand_id.value());
    component_attributes.c_operand = c_operand.descriptor;
  }
  component_attributes.alpha = gemm.alpha;
  component_attributes.beta = gemm.beta;
  component_attributes.a_transpose = gemm.a_transpose;
  component_attributes.b_transpose = gemm.b_transpose;
  return component_attributes;
}

webnn::GruAttributes ConvertToGruAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Gru& gru) {
  webnn::GruAttributes component_attributes;
  if (gru.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, gru.bias_operand_id.value());
    component_attributes.bias = bias->descriptor;
  }
  if (gru.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, gru.recurrent_bias_operand_id.value());
    component_attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  if (gru.initial_hidden_state_operand_id.has_value()) {
    const auto* initial_hidden_state = GetMojoOperand(
        id_to_operand_map, gru.initial_hidden_state_operand_id.value());
    component_attributes.initial_hidden_state =
        initial_hidden_state->descriptor;
  }

  component_attributes.return_sequence = gru.return_sequence;
  component_attributes.direction =
      MojoRecurrentNetworkDirectionToComponent(gru.direction);
  component_attributes.activation_count = gru.activations.size();

  return component_attributes;
}

webnn::GruCellAttributes ConvertToGruCellAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::GruCell& gru_cell) {
  webnn::GruCellAttributes component_attributes;
  if (gru_cell.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, gru_cell.bias_operand_id.value());
    component_attributes.bias = bias->descriptor;
  }
  if (gru_cell.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, gru_cell.recurrent_bias_operand_id.value());
    component_attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  component_attributes.activation_count = gru_cell.activations.size();

  return component_attributes;
}

webnn::InstanceNormalizationAttributes ConvertToInstanceNormalizationAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::InstanceNormalization& instance_normalization) {
  webnn::InstanceNormalizationAttributes component_attributes;
  const auto& scale_operand_id = instance_normalization.scale_operand_id;
  if (scale_operand_id) {
    const mojom::Operand& scale_operand =
        *id_to_operand_map.at(scale_operand_id.value());
    component_attributes.scale = scale_operand.descriptor;
  }
  const auto& bias_operand_id = instance_normalization.bias_operand_id;
  if (bias_operand_id) {
    const mojom::Operand& bias_operand =
        *id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias = bias_operand.descriptor;
  }
  component_attributes.layout =
      MojoInputOperandLayoutToComponent(instance_normalization.layout);

  return component_attributes;
}

webnn::SliceAttributes ConvertToSliceAttributes(
    const webnn::mojom::Slice& slice) {
  webnn::SliceAttributes component_attributes;
  component_attributes.starts.reserve(slice.starts_and_sizes.size());
  component_attributes.sizes.reserve(slice.starts_and_sizes.size());
  for (const auto& start_and_size : slice.starts_and_sizes) {
    component_attributes.starts.push_back(start_and_size->start);
    component_attributes.sizes.push_back(start_and_size->size);
  }
  return component_attributes;
}

template <typename Operation>
bool ValidateUnaryOperation(const IdToOperandMap& id_to_operand_map,
                            const Operation& operation,
                            const webnn::SupportedDataTypes& input_constraint,
                            base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(operation.input_operand_id)) {
    return false;
  }
  processed_operands.insert(operation.output_operand_id);

  const auto* input =
      GetMojoOperand(id_to_operand_map, operation.input_operand_id);
  const auto* output =
      GetMojoOperand(id_to_operand_map, operation.output_operand_id);
  if (!input || !output || output == input) {
    // The unary operator is invalid.
    return false;
  }

  const auto input_data_type = input->descriptor.data_type();
  if (!input_constraint.Has(input_data_type)) {
    // The data type is not in the constraint.
    return false;
  }
  return output->descriptor == input->descriptor;
}

bool ValidateCastOperation(const IdToOperandMap& id_to_operand_map,
                           const mojom::ElementWiseUnary& operation,
                           base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(operation.input_operand_id)) {
    return false;
  }
  processed_operands.insert(operation.output_operand_id);

  const auto* input =
      GetMojoOperand(id_to_operand_map, operation.input_operand_id);
  const auto* output =
      GetMojoOperand(id_to_operand_map, operation.output_operand_id);
  if (!input || !output || output == input) {
    // The unary operator is invalid.
    return false;
  }
  if (!base::ranges::equal(output->descriptor.shape(),
                           input->descriptor.shape())) {
    // The output shape is not expected.
    return false;
  }

  return true;
}

bool ValidateBatchNormalization(
    const IdToOperandMap& id_to_operand_map,
    const mojom::BatchNormalization& batch_normalization,
    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(batch_normalization.input_operand_id) ||
      !processed_operands.contains(batch_normalization.mean_operand_id) ||
      !processed_operands.contains(batch_normalization.variance_operand_id)) {
    return false;
  }
  processed_operands.insert(batch_normalization.output_operand_id);

  const auto* input =
      GetMojoOperand(id_to_operand_map, batch_normalization.input_operand_id);
  const auto* mean =
      GetMojoOperand(id_to_operand_map, batch_normalization.mean_operand_id);
  const auto* variance = GetMojoOperand(
      id_to_operand_map, batch_normalization.variance_operand_id);
  const auto* output =
      GetMojoOperand(id_to_operand_map, batch_normalization.output_operand_id);
  if (!input || !mean || !variance || !output || output == input ||
      output == mean || output == variance) {
    // The batchNormalization operator is invalid.
    return false;
  }
  const auto& scale_operand_id = batch_normalization.scale_operand_id;
  if (scale_operand_id &&
      (!id_to_operand_map.contains(scale_operand_id.value()) ||
       !processed_operands.contains(scale_operand_id.value()))) {
    // The scale operand is invalid.
    return false;
  }
  const auto& bias_operand_id = batch_normalization.bias_operand_id;
  if (bias_operand_id &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(bias_operand_id.value()))) {
    // The bias operand is invalid.
    return false;
  }

  const auto validated_output = ValidateBatchNormalizationAndInferOutput(
      input->descriptor, mean->descriptor, variance->descriptor,
      ConvertToBatchNormalizationAttributes(id_to_operand_map,
                                            batch_normalization));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateArgMinMax(const ContextProperties& context_properties,
                       const IdToOperandMap& id_to_operand_map,
                       const mojom::ArgMinMax& arg_min_max,
                       base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(arg_min_max.input_operand_id)) {
    return false;
  }
  processed_operands.insert(arg_min_max.output_operand_id);

  const auto* input =
      GetMojoOperand(id_to_operand_map, arg_min_max.input_operand_id);
  const auto* output =
      GetMojoOperand(id_to_operand_map, arg_min_max.output_operand_id);
  if (!input || !output || output == input) {
    // The argMinMax operator is invalid.
    return false;
  }

  const auto validated_output = ValidateArgMinMaxAndInferOutput(
      context_properties, input->descriptor, arg_min_max.axes,
      output->descriptor.data_type(), arg_min_max.keep_dimensions);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateClamp(const IdToOperandMap& id_to_operand_map,
                   const mojom::Clamp& clamp,
                   base::flat_set<uint64_t>& processed_operands) {
  if (!ValidateUnaryOperation(id_to_operand_map, clamp,
                              SupportedDataTypes::All(), processed_operands)) {
    return false;
  }
  if (!ValidateClampAttributes(clamp)) {
    return false;
  }

  return true;
}

bool ValidateConcat(const ContextProperties& context_properties,
                    const IdToOperandMap& id_to_operand_map,
                    const mojom::Concat& concat,
                    base::flat_set<uint64_t>& processed_operands) {
  auto* output = GetMojoOperand(id_to_operand_map, concat.output_operand_id);
  if (!output) {
    // The concat operator is invalid.
    return false;
  }

  std::vector<OperandDescriptor> inputs;
  inputs.reserve(concat.input_operand_ids.size());
  for (const auto& input_operand_id : concat.input_operand_ids) {
    if (!processed_operands.contains(input_operand_id)) {
      return false;
    }

    auto* input = GetMojoOperand(id_to_operand_map, input_operand_id);
    if (!input || input == output) {
      return false;
    }
    inputs.push_back(input->descriptor);
  }

  auto validated_output =
      ValidateConcatAndInferOutput(context_properties, inputs, concat.axis);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }
  processed_operands.insert(concat.output_operand_id);

  return true;
}

bool ValidateConv2d(const ContextProperties& context_properties,
                    const IdToOperandMap& id_to_operand_map,
                    const mojom::Conv2d& conv2d,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(conv2d.input_operand_id) ||
      !processed_operands.contains(conv2d.filter_operand_id)) {
    return false;
  }

  auto* input = GetMojoOperand(id_to_operand_map, conv2d.input_operand_id);
  auto* filter = GetMojoOperand(id_to_operand_map, conv2d.filter_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, conv2d.output_operand_id);
  if (!input || !filter || !output || output == input || output == filter) {
    // The conv2d operator is invalid.
    return false;
  }

  // The input and output rank need to be validated before converting to
  // `webnn::Conv2dAttributes`.
  if (input->descriptor.Rank() != 4 || output->descriptor.Rank() != 4) {
    // The element of input and output dimensions should be 4.
    return false;
  }

  std::optional<OperandDescriptor> bias_operand;
  auto& bias_operand_id = conv2d.bias_operand_id;
  if (bias_operand_id) {
    if (!processed_operands.contains(bias_operand_id.value())) {
      return false;
    }
    const auto bias_operand_iterator =
        id_to_operand_map.find(bias_operand_id.value());
    if (bias_operand_iterator == id_to_operand_map.end()) {
      // Invalid bias operand.
      return false;
    }
    bias_operand = bias_operand_iterator->second->descriptor;
  }
  processed_operands.insert(conv2d.output_operand_id);

  std::optional<base::expected<OperandDescriptor, std::string>>
      validated_output;
  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect: {
      validated_output = ValidateConv2dAndInferOutput(
          input->descriptor, filter->descriptor,
          ConvertToConv2dAttributes(context_properties, id_to_operand_map,
                                    conv2d, std::move(bias_operand)));
      break;
    }

    case mojom::Conv2d::Kind::kTransposed: {
      validated_output = ValidateConvTranspose2dAndInferOutput(
          input->descriptor, filter->descriptor,
          ConvertToConvTranspose2dAttributes(context_properties,
                                             id_to_operand_map, conv2d,
                                             std::move(bias_operand)));
      break;
    }
  }
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool IsLogicalElementWiseBinary(mojom::ElementWiseBinary::Kind kind) {
  return kind == mojom::ElementWiseBinary::Kind::kEqual ||
         kind == mojom::ElementWiseBinary::Kind::kGreater ||
         kind == mojom::ElementWiseBinary::Kind::kGreaterOrEqual ||
         kind == mojom::ElementWiseBinary::Kind::kLesser ||
         kind == mojom::ElementWiseBinary::Kind::kLesserOrEqual;
}

bool ValidateElementWiseBinaryDataTypes(
    const mojom::Operand* lhs,
    const mojom::Operand* rhs,
    const mojom::Operand* output,
    const mojom::ElementWiseBinary& operation) {
  if (lhs->descriptor.data_type() != rhs->descriptor.data_type()) {
    // The input types don't match.
    return false;
  }

  if (IsLogicalElementWiseBinary(operation.kind)) {
    if (output->descriptor.data_type() != OperandDataType::kUint8) {
      // For logical operations, the output data type must be uint8.
      return false;
    }
  } else {
    // For all other operations, the input and output data types must match.
    if (output->descriptor.data_type() != lhs->descriptor.data_type()) {
      return false;
    }
  }

  return true;
}

bool ValidateElementWiseBinary(const IdToOperandMap& id_to_operand_map,
                               const mojom::ElementWiseBinary& operation,
                               base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(operation.lhs_operand_id) ||
      !processed_operands.contains(operation.rhs_operand_id)) {
    return false;
  }
  processed_operands.insert(operation.output_operand_id);

  auto* a = GetMojoOperand(id_to_operand_map, operation.lhs_operand_id);
  auto* b = GetMojoOperand(id_to_operand_map, operation.rhs_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, operation.output_operand_id);

  if (!a || !b || !output || output == a || output == b) {
    // The elementWise binary operator is invalid.
    return false;
  }

  if (!ValidateElementWiseBinaryDataTypes(a, b, output, operation)) {
    return false;
  }

  auto dims_output =
      BroadcastShapes(a->descriptor.shape(), b->descriptor.shape());
  if (!dims_output) {
    // The input shapes are not broadcastable.
    return false;
  }
  if (!base::ranges::equal(output->descriptor.shape(), dims_output.value())) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateElu(const IdToOperandMap& id_to_operand_map,
                 const mojom::Elu& elu,
                 base::flat_set<uint64_t>& processed_operands) {
  if (!ValidateUnaryOperation(id_to_operand_map, elu,
                              DataTypeConstraint::kFloat, processed_operands)) {
    return false;
  }
  if (!ValidateEluAttributes(elu)) {
    return false;
  }

  return true;
}

static constexpr auto kUnaryOperatorConstraints =
    base::MakeFixedFlatMap<mojom::ElementWiseUnary::Kind,
                           webnn::SupportedDataTypes>({
        {mojom::ElementWiseUnary::Kind::kAbs,
         DataTypeConstraint::kFloat16To32Int8To32},
        {mojom::ElementWiseUnary::Kind::kCeil, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kCos, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kExp, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kFloor, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kLog, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kNeg,
         DataTypeConstraint::kFloat16To32Int8To32},
        {mojom::ElementWiseUnary::Kind::kSin, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kTan, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kLogicalNot, {OperandDataType::kUint8}},
        {mojom::ElementWiseUnary::Kind::kIdentity, SupportedDataTypes::All()},
        {mojom::ElementWiseUnary::Kind::kSqrt, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kErf, DataTypeConstraint::kFloat},
        {mojom::ElementWiseUnary::Kind::kReciprocal,
         DataTypeConstraint::kFloat},
    });

bool ValidateElementWiseUnary(const IdToOperandMap& id_to_operand_map,
                              const mojom::ElementWiseUnary& operation,
                              base::flat_set<uint64_t>& processed_operands) {
  // List the validation of cast operator separately because its output data
  // type is different from the input data type.
  if (operation.kind == mojom::ElementWiseUnary::Kind::kCast) {
    return ValidateCastOperation(id_to_operand_map, operation,
                                 processed_operands);
  }
  const auto constraints_iterator =
      kUnaryOperatorConstraints.find(operation.kind);
  CHECK(constraints_iterator != kUnaryOperatorConstraints.end());
  return ValidateUnaryOperation(id_to_operand_map, operation,
                                constraints_iterator->second,
                                processed_operands);
}

bool ValidateExpand(const IdToOperandMap& id_to_operand_map,
                    const mojom::Expand& expand,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(expand.input_operand_id)) {
    return false;
  }
  processed_operands.insert(expand.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, expand.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, expand.output_operand_id);
  if (!input || !output || output == input) {
    // The expand operator is invalid.
    return false;
  }
  if (output->descriptor.data_type() != input->descriptor.data_type()) {
    // The output data type doesn't match input data type.
    return false;
  }

  auto output_shape = BroadcastShapes(input->descriptor.shape(),
                                      output->descriptor.shape(), false);
  if (!output_shape) {
    // The input shape is not broadcastable to the output shape.
    return false;
  }
  CHECK(base::ranges::equal(output_shape.value(), output->descriptor.shape()));

  return true;
}

bool ValidateGather(const ContextProperties& context_properties,
                    const IdToOperandMap& id_to_operand_map,
                    const mojom::Gather& gather,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(gather.input_operand_id) ||
      !processed_operands.contains(gather.indices_operand_id)) {
    return false;
  }
  processed_operands.insert(gather.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, gather.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, gather.output_operand_id);
  auto* indices = GetMojoOperand(id_to_operand_map, gather.indices_operand_id);
  if (!input || !output || !indices || output == input || output == indices) {
    // The gather operator is invalid.
    return false;
  }

  auto validated_output = ValidateGatherAndInferOutput(
      context_properties, input->descriptor, indices->descriptor, gather.axis);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateGemm(const IdToOperandMap& id_to_operand_map,
                  const mojom::Gemm& gemm,
                  base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(gemm.a_operand_id) ||
      !processed_operands.contains(gemm.b_operand_id)) {
    return false;
  }
  processed_operands.insert(gemm.output_operand_id);

  auto* a = GetMojoOperand(id_to_operand_map, gemm.a_operand_id);
  auto* b = GetMojoOperand(id_to_operand_map, gemm.b_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, gemm.output_operand_id);
  if (!a || !b || !output || output == a || output == b) {
    // The gemm operator is invalid.
    return false;
  }
  auto& c_operand_id = gemm.c_operand_id;
  if (c_operand_id && (!id_to_operand_map.contains(c_operand_id.value()) ||
                       !processed_operands.contains(c_operand_id.value()))) {
    // The third operand is invalid.
    return false;
  }
  auto validated_output = ValidateGemmAndInferOutput(
      a->descriptor, b->descriptor,
      ConvertToGemmAttributes(id_to_operand_map, gemm));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateGru(const IdToOperandMap& id_to_operand_map,
                 const mojom::Gru& gru,
                 base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(gru.input_operand_id) ||
      !processed_operands.contains(gru.weight_operand_id) ||
      !processed_operands.contains(gru.recurrent_weight_operand_id)) {
    return false;
  }

  const auto* input = GetMojoOperand(id_to_operand_map, gru.input_operand_id);
  const auto* weight = GetMojoOperand(id_to_operand_map, gru.weight_operand_id);
  const auto* recurrent_weight =
      GetMojoOperand(id_to_operand_map, gru.recurrent_weight_operand_id);
  if (!input || !weight || !recurrent_weight) {
    return false;
  }

  const auto& bias_operand_id = gru.bias_operand_id;
  if (bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(gru.bias_operand_id))) {
    return false;
  }
  const auto& recurrent_bias_operand_id = gru.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(recurrent_bias_operand_id.value()) ||
       !processed_operands.contains(gru.recurrent_bias_operand_id))) {
    return false;
  }
  const auto& initial_hidden_state_operand_id =
      gru.initial_hidden_state_operand_id;
  if (initial_hidden_state_operand_id.has_value() &&
      (!id_to_operand_map.contains(initial_hidden_state_operand_id.value()) ||
       !processed_operands.contains(gru.initial_hidden_state_operand_id))) {
    return false;
  }

  for (uint64_t output_operand_id : gru.output_operand_ids) {
    if (output_operand_id == gru.input_operand_id ||
        output_operand_id == gru.weight_operand_id ||
        output_operand_id == gru.recurrent_weight_operand_id) {
      return false;
    }
    if (bias_operand_id == output_operand_id ||
        recurrent_bias_operand_id == output_operand_id ||
        initial_hidden_state_operand_id == output_operand_id) {
      return false;
    }
    processed_operands.insert(output_operand_id);
  }

  const auto validated_outputs = ValidateGruAndInferOutput(
      input->descriptor, weight->descriptor, recurrent_weight->descriptor,
      gru.steps, gru.hidden_size,
      ConvertToGruAttributes(id_to_operand_map, gru));
  if (!validated_outputs.has_value()) {
    return false;
  }
  if (gru.output_operand_ids.size() != validated_outputs->size()) {
    return false;
  }
  for (size_t i = 0; i < validated_outputs->size(); ++i) {
    const auto* output =
        GetMojoOperand(id_to_operand_map, gru.output_operand_ids[i]);
    if (!output) {
      return false;
    }
    if (validated_outputs->at(i) != output->descriptor) {
      return false;
    }
  }

  for (const auto& activation : gru.activations) {
    if (!ValidateActivation(*activation)) {
      return false;
    }
  }

  return true;
}

bool ValidateGruCell(const IdToOperandMap& id_to_operand_map,
                     const mojom::GruCell& gru_cell,
                     base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(gru_cell.input_operand_id) ||
      !processed_operands.contains(gru_cell.weight_operand_id) ||
      !processed_operands.contains(gru_cell.recurrent_weight_operand_id) ||
      !processed_operands.contains(gru_cell.hidden_state_operand_id)) {
    return false;
  }

  const mojom::Operand* input =
      GetMojoOperand(id_to_operand_map, gru_cell.input_operand_id);
  const mojom::Operand* weight =
      GetMojoOperand(id_to_operand_map, gru_cell.weight_operand_id);
  const mojom::Operand* recurrent_weight =
      GetMojoOperand(id_to_operand_map, gru_cell.recurrent_weight_operand_id);
  const mojom::Operand* hidden_state =
      GetMojoOperand(id_to_operand_map, gru_cell.hidden_state_operand_id);
  if (!input || !weight || !recurrent_weight || !hidden_state) {
    return false;
  }

  const std::optional<uint32_t>& bias_operand_id = gru_cell.bias_operand_id;
  if (bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(gru_cell.bias_operand_id))) {
    return false;
  }
  const std::optional<uint32_t>& recurrent_bias_operand_id =
      gru_cell.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(recurrent_bias_operand_id.value()) ||
       !processed_operands.contains(gru_cell.recurrent_bias_operand_id))) {
    return false;
  }

  if (gru_cell.output_operand_id == gru_cell.input_operand_id ||
      gru_cell.output_operand_id == gru_cell.weight_operand_id ||
      gru_cell.output_operand_id == gru_cell.recurrent_weight_operand_id ||
      gru_cell.output_operand_id == gru_cell.hidden_state_operand_id ||
      gru_cell.output_operand_id == bias_operand_id ||
      gru_cell.output_operand_id == recurrent_bias_operand_id) {
    return false;
  }
  processed_operands.insert(gru_cell.output_operand_id);

  const base::expected<OperandDescriptor, std::string> validated_output =
      ValidateGruCellAndInferOutput(
          input->descriptor, weight->descriptor, recurrent_weight->descriptor,
          hidden_state->descriptor, gru_cell.hidden_size,
          ConvertToGruCellAttributes(id_to_operand_map, gru_cell));
  if (!validated_output.has_value()) {
    return false;
  }

  const mojom::Operand* output =
      GetMojoOperand(id_to_operand_map, gru_cell.output_operand_id);
  if (!output) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  for (const auto& activation : gru_cell.activations) {
    if (!ValidateActivation(*activation)) {
      return false;
    }
  }

  return true;
}

bool ValidateHardSigmoid(const IdToOperandMap& id_to_operand_map,
                         const mojom::HardSigmoid& hard_sigmoid,
                         base::flat_set<uint64_t>& processed_operands) {
  if (!ValidateUnaryOperation(id_to_operand_map, hard_sigmoid,
                              DataTypeConstraint::kFloat, processed_operands)) {
    return false;
  }
  if (!ValidateHardSigmoidAttributes(hard_sigmoid)) {
    return false;
  }

  return true;
}

bool ValidateLayerNormalization(
    const IdToOperandMap& id_to_operand_map,
    const mojom::LayerNormalization& layer_normalization,
    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(layer_normalization.input_operand_id)) {
    return false;
  }
  processed_operands.insert(layer_normalization.output_operand_id);

  const auto* input =
      GetMojoOperand(id_to_operand_map, layer_normalization.input_operand_id);
  const auto* output =
      GetMojoOperand(id_to_operand_map, layer_normalization.output_operand_id);
  if (!input || !output || output == input) {
    // The layerNormalization operator is invalid.
    return false;
  }

  const auto& scale_operand_id = layer_normalization.scale_operand_id;
  if (scale_operand_id &&
      (!id_to_operand_map.contains(scale_operand_id.value()) ||
       !processed_operands.contains(scale_operand_id.value()) ||
       scale_operand_id.value() == layer_normalization.output_operand_id)) {
    // The scale operand is invalid.
    return false;
  }
  const auto& bias_operand_id = layer_normalization.bias_operand_id;
  if (bias_operand_id &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(bias_operand_id.value()) ||
       bias_operand_id.value() == layer_normalization.output_operand_id)) {
    // The bias operand is invalid.
    return false;
  }

  const auto validated_output = ValidateLayerNormalizationAndInferOutput(
      input->descriptor, layer_normalization.axes,
      ConvertToLayerNormalizationAttributes(id_to_operand_map,
                                            layer_normalization));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateLeakyRelu(const IdToOperandMap& id_to_operand_map,
                       const mojom::LeakyRelu& leaky_relu,
                       base::flat_set<uint64_t>& processed_operands) {
  if (!ValidateUnaryOperation(id_to_operand_map, leaky_relu,
                              DataTypeConstraint::kFloat, processed_operands)) {
    return false;
  }
  if (!ValidateLeakyReluAttributes(leaky_relu)) {
    return false;
  }

  return true;
}

bool ValidateLinear(const IdToOperandMap& id_to_operand_map,
                    const mojom::Linear& linear,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!ValidateUnaryOperation(id_to_operand_map, linear,
                              DataTypeConstraint::kFloat, processed_operands)) {
    return false;
  }
  if (!ValidateLinearAttributes(linear)) {
    return false;
  }

  return true;
}

bool ValidateLstm(const IdToOperandMap& id_to_operand_map,
                  const mojom::Lstm& lstm,
                  base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(lstm.input_operand_id) ||
      !processed_operands.contains(lstm.weight_operand_id) ||
      !processed_operands.contains(lstm.recurrent_weight_operand_id)) {
    return false;
  }

  const auto* input = GetMojoOperand(id_to_operand_map, lstm.input_operand_id);
  const auto* weight =
      GetMojoOperand(id_to_operand_map, lstm.weight_operand_id);
  const auto* recurrent_weight =
      GetMojoOperand(id_to_operand_map, lstm.recurrent_weight_operand_id);
  if (!input || !weight || !recurrent_weight) {
    return false;
  }

  const auto& bias_operand_id = lstm.bias_operand_id;
  if (bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(lstm.bias_operand_id))) {
    return false;
  }
  const auto& recurrent_bias_operand_id = lstm.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(recurrent_bias_operand_id.value()) ||
       !processed_operands.contains(lstm.recurrent_bias_operand_id))) {
    return false;
  }
  const auto& peephole_weight_operand_id = lstm.peephole_weight_operand_id;
  if (peephole_weight_operand_id.has_value() &&
      (!id_to_operand_map.contains(peephole_weight_operand_id.value()) ||
       !processed_operands.contains(lstm.peephole_weight_operand_id))) {
    return false;
  }
  const auto& initial_hidden_state_operand_id =
      lstm.initial_hidden_state_operand_id;
  if (initial_hidden_state_operand_id.has_value() &&
      (!id_to_operand_map.contains(initial_hidden_state_operand_id.value()) ||
       !processed_operands.contains(lstm.initial_hidden_state_operand_id))) {
    return false;
  }
  const auto& initial_cell_state_operand_id =
      lstm.initial_cell_state_operand_id;
  if (initial_cell_state_operand_id.has_value() &&
      (!id_to_operand_map.contains(initial_cell_state_operand_id.value()) ||
       !processed_operands.contains(lstm.initial_cell_state_operand_id))) {
    return false;
  }

  for (uint64_t output_operand_id : lstm.output_operand_ids) {
    if (output_operand_id == lstm.input_operand_id ||
        output_operand_id == lstm.weight_operand_id ||
        output_operand_id == lstm.recurrent_weight_operand_id) {
      return false;
    }
    if ((initial_hidden_state_operand_id.has_value() &&
         initial_hidden_state_operand_id.value() == output_operand_id) ||
        (initial_cell_state_operand_id.has_value() &&
         initial_cell_state_operand_id.value() == output_operand_id)) {
      return false;
    }
    processed_operands.insert(output_operand_id);
  }

  const auto validated_outputs = ValidateLstmAndInferOutput(
      input->descriptor, weight->descriptor, recurrent_weight->descriptor,
      lstm.steps, lstm.hidden_size,
      ConvertToLstmAttributes(id_to_operand_map, lstm));
  if (!validated_outputs.has_value()) {
    return false;
  }
  if (lstm.output_operand_ids.size() != validated_outputs->size()) {
    return false;
  }
  for (size_t i = 0; i < validated_outputs->size(); ++i) {
    const auto* output =
        GetMojoOperand(id_to_operand_map, lstm.output_operand_ids[i]);
    if (!output) {
      return false;
    }
    if (validated_outputs->at(i) != output->descriptor) {
      return false;
    }
  }

  for (const auto& activation : lstm.activations) {
    if (!ValidateActivation(*activation)) {
      return false;
    }
  }

  return true;
}

bool ValidateLstmCell(const IdToOperandMap& id_to_operand_map,
                      const mojom::LstmCell& lstm_cell,
                      base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(lstm_cell.input_operand_id) ||
      !processed_operands.contains(lstm_cell.weight_operand_id) ||
      !processed_operands.contains(lstm_cell.recurrent_weight_operand_id) ||
      !processed_operands.contains(lstm_cell.hidden_state_operand_id) ||
      !processed_operands.contains(lstm_cell.cell_state_operand_id)) {
    return false;
  }

  const mojom::Operand* input =
      GetMojoOperand(id_to_operand_map, lstm_cell.input_operand_id);
  const mojom::Operand* weight =
      GetMojoOperand(id_to_operand_map, lstm_cell.weight_operand_id);
  const mojom::Operand* recurrent_weight =
      GetMojoOperand(id_to_operand_map, lstm_cell.recurrent_weight_operand_id);
  const mojom::Operand* hidden_state =
      GetMojoOperand(id_to_operand_map, lstm_cell.hidden_state_operand_id);
  const mojom::Operand* cell_state =
      GetMojoOperand(id_to_operand_map, lstm_cell.cell_state_operand_id);
  if (!input || !weight || !recurrent_weight || !hidden_state || !cell_state) {
    return false;
  }

  const std::optional<uint64_t> bias_operand_id = lstm_cell.bias_operand_id;
  if (bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(bias_operand_id.value()))) {
    return false;
  }
  const std::optional<uint64_t> recurrent_bias_operand_id =
      lstm_cell.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value() &&
      (!id_to_operand_map.contains(recurrent_bias_operand_id.value()) ||
       !processed_operands.contains(recurrent_bias_operand_id.value()))) {
    return false;
  }
  const std::optional<uint64_t> peephole_weight_operand_id =
      lstm_cell.peephole_weight_operand_id;
  if (peephole_weight_operand_id.has_value() &&
      (!id_to_operand_map.contains(peephole_weight_operand_id.value()) ||
       !processed_operands.contains(peephole_weight_operand_id.value()))) {
    return false;
  }

  for (uint64_t output_operand_id : lstm_cell.output_operand_ids) {
    if (output_operand_id == lstm_cell.input_operand_id ||
        output_operand_id == lstm_cell.weight_operand_id ||
        output_operand_id == lstm_cell.recurrent_weight_operand_id ||
        output_operand_id == lstm_cell.hidden_state_operand_id ||
        output_operand_id == lstm_cell.cell_state_operand_id) {
      return false;
    }
    processed_operands.insert(output_operand_id);
  }

  const base::expected<std::vector<webnn::OperandDescriptor>, std::string>
      validated_outputs = ValidateLstmCellAndInferOutput(
          input->descriptor, weight->descriptor, recurrent_weight->descriptor,
          hidden_state->descriptor, cell_state->descriptor,
          lstm_cell.hidden_size,
          ConvertToLstmCellAttributes(id_to_operand_map, lstm_cell));
  if (!validated_outputs.has_value()) {
    return false;
  }
  if (lstm_cell.output_operand_ids.size() != validated_outputs->size()) {
    return false;
  }
  for (size_t i = 0; i < validated_outputs->size(); ++i) {
    const mojom::Operand* output =
        GetMojoOperand(id_to_operand_map, lstm_cell.output_operand_ids[i]);
    if (!output) {
      return false;
    }
    if (validated_outputs->at(i) != output->descriptor) {
      return false;
    }
  }

  if (!base::ranges::all_of(lstm_cell.activations,
                            [](const mojom::ActivationPtr& activation) {
                              return ValidateActivation(*activation);
                            })) {
    return false;
  }

  return true;
}

bool ValidateInstanceNormalization(
    const IdToOperandMap& id_to_operand_map,
    const mojom::InstanceNormalization& instance_normalization,
    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(instance_normalization.input_operand_id)) {
    return false;
  }
  processed_operands.insert(instance_normalization.output_operand_id);

  const auto* input = GetMojoOperand(id_to_operand_map,
                                     instance_normalization.input_operand_id);
  const auto* output = GetMojoOperand(id_to_operand_map,
                                      instance_normalization.output_operand_id);
  if (!input || !output || output == input) {
    // The instanceNormalization operator is invalid.
    return false;
  }
  const auto& scale_operand_id = instance_normalization.scale_operand_id;
  if (scale_operand_id &&
      (!id_to_operand_map.contains(scale_operand_id.value()) ||
       !processed_operands.contains(scale_operand_id.value()) ||
       scale_operand_id.value() == instance_normalization.output_operand_id)) {
    // The scale operand is invalid.
    return false;
  }
  const auto& bias_operand_id = instance_normalization.bias_operand_id;
  if (bias_operand_id &&
      (!id_to_operand_map.contains(bias_operand_id.value()) ||
       !processed_operands.contains(bias_operand_id.value()) ||
       bias_operand_id.value() == instance_normalization.output_operand_id)) {
    // The bias operand is invalid.
    return false;
  }

  const auto validated_output = ValidateInstanceNormalizationAndInferOutput(
      input->descriptor, ConvertToInstanceNormalizationAttributes(
                             id_to_operand_map, instance_normalization));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateMatmul(const IdToOperandMap& id_to_operand_map,
                    const mojom::Matmul& matmul,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(matmul.a_operand_id) ||
      !processed_operands.contains(matmul.b_operand_id)) {
    return false;
  }
  processed_operands.insert(matmul.output_operand_id);

  auto* a = GetMojoOperand(id_to_operand_map, matmul.a_operand_id);
  auto* b = GetMojoOperand(id_to_operand_map, matmul.b_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, matmul.output_operand_id);
  if (!a || !b || !output || output == a || output == b) {
    // The matmul operator is invalid.
    return false;
  }
  auto validated_output =
      ValidateMatmulAndInferOutput(a->descriptor, b->descriptor);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidatePad(const IdToOperandMap& id_to_operand_map,
                 const mojom::Pad& pad,
                 base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(pad.input_operand_id)) {
    return false;
  }
  processed_operands.insert(pad.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, pad.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, pad.output_operand_id);
  if (!input || !output || output == input) {
    // The pad operator is invalid.
    return false;
  }

  auto validated_output = ValidatePadAndInferOutput(
      input->descriptor, pad.beginning_padding, pad.ending_padding);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidatePool2d(const ContextProperties& context_properties,
                    const IdToOperandMap& id_to_operand_map,
                    const mojom::Pool2d& pool2d,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(pool2d.input_operand_id)) {
    return false;
  }
  processed_operands.insert(pool2d.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, pool2d.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, pool2d.output_operand_id);
  if (!input || !output || output == input) {
    // The pool2d operator is invalid.
    return false;
  }

  if (pool2d.kind == mojom::Pool2d::Kind::kAveragePool2d ||
      pool2d.kind == mojom::Pool2d::Kind::kL2Pool2d) {
    if (!(input->descriptor.data_type() == OperandDataType::kFloat32 ||
          input->descriptor.data_type() == OperandDataType::kFloat16)) {
      return false;
    }
  }

  if (output->descriptor.Rank() != 4) {
    return false;
  }
  auto validated_output = ValidatePool2dAndInferOutput(
      input->descriptor,
      ConvertToPool2dAttributes(context_properties, pool2d, output));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidatePrelu(const IdToOperandMap& id_to_operand_map,
                   const mojom::Prelu& prelu,
                   base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(prelu.input_operand_id) ||
      !processed_operands.contains(prelu.slope_operand_id)) {
    return false;
  }
  processed_operands.insert(prelu.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, prelu.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, prelu.output_operand_id);
  auto* slope = GetMojoOperand(id_to_operand_map, prelu.slope_operand_id);
  if (!input || !output || !slope || output == input || output == slope) {
    // The prelu operator is invalid.
    return false;
  }

  auto validated_output = ValidatePreluAndInferOutput(
      input->descriptor, slope->descriptor, prelu.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateResample2d(const IdToOperandMap& id_to_operand_map,
                        const mojom::Resample2d& resample2d,
                        base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(resample2d.input_operand_id)) {
    return false;
  }
  processed_operands.insert(resample2d.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, resample2d.input_operand_id);
  auto* output =
      GetMojoOperand(id_to_operand_map, resample2d.output_operand_id);
  if (!input || !output || output == input) {
    // The resample2d operator is invalid.
    return false;
  }

  // Validate and infer the output for resample2d with given scales or with the
  // sizes from output dimensions along axes.
  absl::variant<base::span<const float>, base::span<const uint32_t>>
      scales_or_sizes;
  const auto& axes = resample2d.axes;
  std::vector<uint32_t> sizes;
  if (resample2d.scales) {
    scales_or_sizes = resample2d.scales.value();
  } else {
    const auto& output_dimensions = output->descriptor.shape();
    if (axes.size() != 2 || axes[0] >= output_dimensions.size() ||
        axes[1] >= output_dimensions.size()) {
      return false;
    }
    sizes = {output_dimensions[axes[0]], output_dimensions[axes[1]]};
    scales_or_sizes = sizes;
  }

  auto validated_output = ValidateResample2dAndInferOutput(
      input->descriptor, scales_or_sizes, axes, resample2d.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateReshape(const IdToOperandMap& id_to_operand_map,
                     const mojom::Reshape& reshape,
                     base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(reshape.input_operand_id)) {
    return false;
  }
  processed_operands.insert(reshape.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, reshape.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, reshape.output_operand_id);
  if (!input || !output || output == input) {
    // The reshape operator is invalid.
    return false;
  }
  if (output->descriptor.data_type() != input->descriptor.data_type()) {
    return false;
  }

  if (input->descriptor.NumberOfElements() !=
      output->descriptor.NumberOfElements()) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateSlice(const IdToOperandMap& id_to_operand_map,
                   const mojom::Slice& slice,
                   base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(slice.input_operand_id)) {
    return false;
  }
  processed_operands.insert(slice.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, slice.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, slice.output_operand_id);

  if (!input || !output || output == input) {
    // The slice operator is invalid.
    return false;
  }

  auto validated_output = ValidateSliceAndInferOutput(
      input->descriptor, ConvertToSliceAttributes(slice));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateSoftmax(const IdToOperandMap& id_to_operand_map,
                     const mojom::Softmax& softmax,
                     base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(softmax.input_operand_id)) {
    return false;
  }
  processed_operands.insert(softmax.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, softmax.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, softmax.output_operand_id);
  if (!input || !output || output == input) {
    // The softmax operator is invalid.
    return false;
  }
  auto validated_output =
      ValidateSoftmaxAndInferOutput(input->descriptor, softmax.axis);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateSplit(const IdToOperandMap& id_to_operand_map,
                   const mojom::Split& split,
                   base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(split.input_operand_id)) {
    return false;
  }

  auto* input = GetMojoOperand(id_to_operand_map, split.input_operand_id);
  if (!input) {
    // The split operator is invalid.
    return false;
  }
  std::vector<uint32_t> splits;
  splits.reserve(split.output_operand_ids.size());
  for (uint64_t output_id : split.output_operand_ids) {
    auto* output = GetMojoOperand(id_to_operand_map, output_id);
    if (!output || input == output) {
      return false;
    }

    if (split.axis >= output->descriptor.Rank()) {
      return false;
    }
    splits.push_back(output->descriptor.shape()[split.axis]);
    processed_operands.insert(output_id);
  }

  auto validated_output = ValidateSplitAndInferOutput(
      input->descriptor, {.splits = splits, .axis = split.axis});
  if (!validated_output.has_value()) {
    return false;
  }

  if (split.output_operand_ids.size() != validated_output->size()) {
    // The number of specified outputs did not match the expected number of
    // outputs.
    return false;
  }

  for (uint32_t i = 0; i < validated_output->size(); ++i) {
    auto* output =
        GetMojoOperand(id_to_operand_map, split.output_operand_ids[i]);
    if (validated_output->at(i) != output->descriptor) {
      return false;
    }
  }

  return true;
}

bool ValidateTranspose(const IdToOperandMap& id_to_operand_map,
                       const mojom::Transpose& transpose,
                       base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(transpose.input_operand_id)) {
    return false;
  }
  processed_operands.insert(transpose.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, transpose.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, transpose.output_operand_id);
  if (!input || !output || output == input) {
    // The transpose operator is invalid.
    return false;
  }

  auto validated_output =
      ValidateTransposeAndInferOutput(input->descriptor, transpose.permutation);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateTriangular(const IdToOperandMap& id_to_operand_map,
                        const mojom::Triangular& triangular,
                        base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(triangular.input_operand_id)) {
    return false;
  }
  processed_operands.insert(triangular.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, triangular.input_operand_id);
  auto* output =
      GetMojoOperand(id_to_operand_map, triangular.output_operand_id);
  if (!input || !output || output == input) {
    // The triangular operator is invalid.
    return false;
  }

  base::expected<OperandDescriptor, std::string> validated_output =
      ValidateTriangularAndInferOutput(input->descriptor);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateWhere(const ContextProperties& context_properties,
                   const IdToOperandMap& id_to_operand_map,
                   const mojom::Where& where,
                   base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(where.condition_operand_id) ||
      !processed_operands.contains(where.true_value_operand_id) ||
      !processed_operands.contains(where.false_value_operand_id)) {
    return false;
  }
  processed_operands.insert(where.output_operand_id);

  auto* condition =
      GetMojoOperand(id_to_operand_map, where.condition_operand_id);
  auto* true_value =
      GetMojoOperand(id_to_operand_map, where.true_value_operand_id);
  auto* false_value =
      GetMojoOperand(id_to_operand_map, where.false_value_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, where.output_operand_id);
  if (!condition || !true_value || !false_value || !output ||
      output == condition || output == true_value || output == false_value) {
    // The where operator is invalid.
    return false;
  }

  auto validated_output_descriptor = ValidateWhereAndInferOutput(
      context_properties, condition->descriptor, true_value->descriptor,
      false_value->descriptor);
  if (!validated_output_descriptor.has_value()) {
    return false;
  }
  if (validated_output_descriptor != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateReduce(const IdToOperandMap& id_to_operand_map,
                    const mojom::Reduce& reduce,
                    base::flat_set<uint64_t>& processed_operands) {
  if (!processed_operands.contains(reduce.input_operand_id)) {
    return false;
  }
  processed_operands.insert(reduce.output_operand_id);

  auto* input = GetMojoOperand(id_to_operand_map, reduce.input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, reduce.output_operand_id);
  if (!input || !output || output == input) {
    // The reduce operator is invalid.
    return false;
  }

  auto validated_output = ValidateReduceAndInferOutput(
      MojoReduceTypeToComponent(reduce.kind), input->descriptor, reduce.axes,
      reduce.keep_dimensions);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool ValidateOperation(const ContextProperties& context_properties,
                       const IdToOperandMap& id_to_operand_map,
                       const mojom::Operation& operation,
                       base::flat_set<uint64_t>& processed_operands) {
  switch (operation.which()) {
    case mojom::Operation::Tag::kArgMinMax:
      return ValidateArgMinMax(context_properties, id_to_operand_map,
                               *operation.get_arg_min_max(),
                               processed_operands);
    case mojom::Operation::Tag::kBatchNormalization:
      return ValidateBatchNormalization(id_to_operand_map,
                                        *operation.get_batch_normalization(),
                                        processed_operands);
    case mojom::Operation::Tag::kClamp:
      return ValidateClamp(id_to_operand_map, *operation.get_clamp(),
                           processed_operands);
    case mojom::Operation::Tag::kConcat:
      return ValidateConcat(context_properties, id_to_operand_map,
                            *operation.get_concat(), processed_operands);
    case mojom::Operation::Tag::kConv2d:
      return ValidateConv2d(context_properties, id_to_operand_map,
                            *operation.get_conv2d(), processed_operands);
    case mojom::Operation::Tag::kElementWiseBinary:
      return ValidateElementWiseBinary(id_to_operand_map,
                                       *operation.get_element_wise_binary(),
                                       processed_operands);
    case mojom::Operation::Tag::kElu:
      return ValidateElu(id_to_operand_map, *operation.get_elu(),
                         processed_operands);
    case mojom::Operation::Tag::kElementWiseUnary:
      return ValidateElementWiseUnary(id_to_operand_map,
                                      *operation.get_element_wise_unary(),
                                      processed_operands);
    case mojom::Operation::Tag::kExpand:
      return ValidateExpand(id_to_operand_map, *operation.get_expand(),
                            processed_operands);
    case mojom::Operation::Tag::kGather:
      return ValidateGather(context_properties, id_to_operand_map,
                            *operation.get_gather(), processed_operands);
    case mojom::Operation::Tag::kGelu:
      return ValidateUnaryOperation(id_to_operand_map, *operation.get_gelu(),
                                    DataTypeConstraint::kFloat,
                                    processed_operands);
    case mojom::Operation::Tag::kGemm:
      return ValidateGemm(id_to_operand_map, *operation.get_gemm(),
                          processed_operands);
    case mojom::Operation::Tag::kGru:
      return ValidateGru(id_to_operand_map, *operation.get_gru(),
                         processed_operands);
    case mojom::Operation::Tag::kGruCell:
      return ValidateGruCell(id_to_operand_map, *operation.get_gru_cell(),
                             processed_operands);
    case mojom::Operation::Tag::kHardSigmoid:
      return ValidateHardSigmoid(
          id_to_operand_map, *operation.get_hard_sigmoid(), processed_operands);
    case mojom::Operation::Tag::kHardSwish:
      return ValidateUnaryOperation(
          id_to_operand_map, *operation.get_hard_swish(),
          DataTypeConstraint::kFloat, processed_operands);
    case mojom::Operation::Tag::kLayerNormalization:
      return ValidateLayerNormalization(id_to_operand_map,
                                        *operation.get_layer_normalization(),
                                        processed_operands);
    case mojom::Operation::Tag::kInstanceNormalization:
      return ValidateInstanceNormalization(
          id_to_operand_map, *operation.get_instance_normalization(),
          processed_operands);
    case mojom::Operation::Tag::kLeakyRelu:
      return ValidateLeakyRelu(id_to_operand_map, *operation.get_leaky_relu(),
                               processed_operands);
    case mojom::Operation::Tag::kLinear:
      return ValidateLinear(id_to_operand_map, *operation.get_linear(),
                            processed_operands);
    case mojom::Operation::Tag::kLstm:
      return ValidateLstm(id_to_operand_map, *operation.get_lstm(),
                          processed_operands);
    case mojom::Operation::Tag::kLstmCell:
      return ValidateLstmCell(id_to_operand_map, *operation.get_lstm_cell(),
                              processed_operands);
    case mojom::Operation::Tag::kMatmul:
      return ValidateMatmul(id_to_operand_map, *operation.get_matmul(),
                            processed_operands);
    case mojom::Operation::Tag::kPad:
      return ValidatePad(id_to_operand_map, *operation.get_pad(),
                         processed_operands);
    case mojom::Operation::Tag::kPool2d:
      return ValidatePool2d(context_properties, id_to_operand_map,
                            *operation.get_pool2d(), processed_operands);
    case mojom::Operation::Tag::kPrelu:
      return ValidatePrelu(id_to_operand_map, *operation.get_prelu(),
                           processed_operands);
    case mojom::Operation::Tag::kReduce:
      return ValidateReduce(id_to_operand_map, *operation.get_reduce(),
                            processed_operands);
    case mojom::Operation::Tag::kResample2d:
      return ValidateResample2d(id_to_operand_map, *operation.get_resample2d(),
                                processed_operands);
    case mojom::Operation::Tag::kReshape:
      return ValidateReshape(id_to_operand_map, *operation.get_reshape(),
                             processed_operands);
    case mojom::Operation::Tag::kRelu:
      return ValidateUnaryOperation(id_to_operand_map, *operation.get_relu(),
                                    DataTypeConstraint::kFloat16To32Int8To32,
                                    processed_operands);
    case mojom::Operation::Tag::kSlice:
      return ValidateSlice(id_to_operand_map, *operation.get_slice(),
                           processed_operands);
    case mojom::Operation::Tag::kSigmoid:
      return ValidateUnaryOperation(id_to_operand_map, *operation.get_sigmoid(),
                                    DataTypeConstraint::kFloat,
                                    processed_operands);
    case mojom::Operation::Tag::kSoftmax:
      return ValidateSoftmax(id_to_operand_map, *operation.get_softmax(),
                             processed_operands);
    case mojom::Operation::Tag::kSoftplus:
      return ValidateUnaryOperation(
          id_to_operand_map, *operation.get_softplus(),
          DataTypeConstraint::kFloat, processed_operands);
    case mojom::Operation::Tag::kSoftsign:
      return ValidateUnaryOperation(
          id_to_operand_map, *operation.get_softsign(),
          DataTypeConstraint::kFloat, processed_operands);
    case mojom::Operation::Tag::kSplit:
      return ValidateSplit(id_to_operand_map, *operation.get_split(),
                           processed_operands);
    case mojom::Operation::Tag::kTanh:
      return ValidateUnaryOperation(id_to_operand_map, *operation.get_tanh(),
                                    DataTypeConstraint::kFloat,
                                    processed_operands);
    case mojom::Operation::Tag::kTranspose:
      return ValidateTranspose(id_to_operand_map, *operation.get_transpose(),
                               processed_operands);
    case mojom::Operation::Tag::kTriangular:
      return ValidateTriangular(id_to_operand_map, *operation.get_triangular(),
                                processed_operands);
    case mojom::Operation::Tag::kWhere:
      return ValidateWhere(context_properties, id_to_operand_map,
                           *operation.get_where(), processed_operands);
  }
  NOTREACHED_NORETURN();
}

// Return false if the named inputs for computation don't match the built
// graph's expectation.
bool ValidateInputsForComputation(
    const base::flat_map<std::string, mojo_base::BigBuffer>& named_inputs,
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  return base::ranges::equal(
      named_inputs, names_to_descriptors,
      [](const auto& input, const auto& input_spec) {
        const auto& [input_name, input_buffer] = input;
        const auto& [input_spec_name, input_spec_descriptor] = input_spec;
        return input_name == input_spec_name &&
               input_buffer.size() == input_spec_descriptor.PackedByteLength();
      });
}

// Return false if the named buffers for dispatch don't match the built
// graph's expectation.
bool ValidateWebNNBuffers(
    const base::flat_map<std::string_view, WebNNBufferImpl*>& named_buffers,
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  return base::ranges::equal(
      named_buffers, names_to_descriptors,
      [](const auto& named_buffer, const auto& buffer_spec) {
        const auto& [buffer_name, buffer_impl] = named_buffer;
        const auto& [buffer_spec_name, buffer_spec_descriptor] = buffer_spec;
        return buffer_name == buffer_spec_name &&
               buffer_impl->data_type() == buffer_spec_descriptor.data_type() &&
               buffer_impl->shape() == buffer_spec_descriptor.shape();
      });
}

// Return false if the same buffer was specified in inputs and outputs.
bool ValidateWebNNBuffersUsage(
    const base::flat_map<std::string, base::UnguessableToken>& named_inputs,
    const base::flat_map<std::string, base::UnguessableToken>& named_outputs) {
  // Validate that output buffers are unique.
  std::set<base::UnguessableToken> output_buffers;
  for (const auto& named_output : named_outputs) {
    output_buffers.insert(named_output.second);
  }

  if (output_buffers.size() != named_outputs.size()) {
    return false;
  }

  // Validate buffers used for input and output are unique.
  for (const auto& named_input : named_inputs) {
    if (output_buffers.contains(named_input.second)) {
      return false;
    }
  }

  return true;
}

}  // namespace

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors,
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors,
    base::PassKey<WebNNGraphImpl> pass_key)
    : input_names_to_descriptors(std::move(input_names_to_descriptors)),
      output_names_to_descriptors(std::move(output_names_to_descriptors)) {}

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    const ComputeResourceInfo&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(const ComputeResourceInfo&) =
    default;

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    ComputeResourceInfo&&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(ComputeResourceInfo&&) = default;

WebNNGraphImpl::ComputeResourceInfo::~ComputeResourceInfo() = default;

WebNNGraphImpl::WebNNGraphImpl(WebNNContextImpl* context,
                               ComputeResourceInfo compute_resource_info)
    : compute_resource_info_(std::move(compute_resource_info)),
      context_(context) {
  CHECK(context_);
#if DCHECK_IS_ON()
  context_->AssertCalledOnValidSequence();
#endif
}

WebNNGraphImpl::~WebNNGraphImpl() = default;

std::optional<WebNNGraphImpl::ComputeResourceInfo>
WebNNGraphImpl::ValidateGraph(const ContextProperties& context_properties,
                              const mojom::GraphInfo& graph_info) {
  // The input operands of graph can be empty.
  if (graph_info.id_to_operand_map.empty() || graph_info.operations.empty() ||
      graph_info.output_operands.empty()) {
    return std::nullopt;
  }

  // Keeps track of operands as they are visited in order to assert that they
  // are topologically sorted with inputs pointing to predecessor's outputs or
  // graph inputs.
  base::flat_set<uint64_t> processed_operands;

  // Keeps track of input and output names in order to assert they are unique.
  base::flat_map<std::string, OperandDescriptor> inputs;
  base::flat_map<std::string, OperandDescriptor> outputs;
  inputs.reserve(graph_info.input_operands.size());
  outputs.reserve(graph_info.output_operands.size());

  // Validate all operands in the graph for the dimensions and the byte length
  // of operand that can't be out of range, and hold the temporary information
  // of inputs, constants, outputs for further validation.
  std::vector<uint64_t> graph_inputs;
  graph_inputs.reserve(graph_info.input_operands.size());
  std::vector<uint64_t> graph_outputs;
  graph_outputs.reserve(graph_info.output_operands.size());
  base::flat_map<uint64_t, size_t> constant_id_to_byte_length_map;
  for (auto& [id, operand] : graph_info.id_to_operand_map) {
    const std::optional<std::string>& name = operand->name;
    switch (operand->kind) {
      case mojom::Operand::Kind::kInput: {
        if (!name || name.value().empty()) {
          // The name of input is empty.
          return std::nullopt;
        }
        if (!inputs.try_emplace(*name, operand->descriptor).second) {
          // Input names must be unique.
          return std::nullopt;
        }
        graph_inputs.push_back(id);
        processed_operands.insert(id);
        break;
      }
      case mojom::Operand::Kind::kOutput: {
        // The intermediate operands have no the name value, only the graph
        // outputs have the name.
        if (name) {
          if (name.value().empty()) {
            // The name of output is empty.
            return std::nullopt;
          }
          if (!outputs.try_emplace(*name, operand->descriptor).second) {
            // Output names must be unique.
            return std::nullopt;
          }
          graph_outputs.push_back(id);
        } else {
          // The intermediate operand that connects with two operators has no
          // the name value.
        }
        break;
      }
      case mojom::Operand::Kind::kConstant: {
        if (name) {
          // Constant operand should not have a name.
          return std::nullopt;
        }
        constant_id_to_byte_length_map[id] =
            operand->descriptor.PackedByteLength();
        processed_operands.insert(id);
        break;
      }
    }
  }

  // The `id_to_operand_map` is an ordered map, so the `graph_inputs` and
  // `graph_outputs` are also an ordered array for the value id, the
  // `input_operands` and `graph_outputs` are also an ordered array configured
  // in blink side.
  if (graph_info.input_operands != graph_inputs ||
      graph_info.output_operands != graph_outputs) {
    return std::nullopt;
  }

  // Validate the constant weight data are valid.
  if (!base::ranges::equal(graph_info.constant_id_to_buffer_map,
                           constant_id_to_byte_length_map,
                           [](const auto& iter_a, const auto& iter_b) {
                             // Compare the constant id with the key of map and
                             // the byte length of buffer with value of map.
                             return iter_a.first == iter_b.first &&
                                    iter_a.second.size() == iter_b.second;
                           })) {
    return std::nullopt;
  }

  // Validate the operations which are sorted in the topological order.
  for (auto& operation : graph_info.operations) {
    if (!ValidateOperation(context_properties, graph_info.id_to_operand_map,
                           *operation, processed_operands)) {
      return std::nullopt;
    }
  }

  return ComputeResourceInfo(std::move(inputs), std::move(outputs),
                             base::PassKey<WebNNGraphImpl>());
}

// static
bool WebNNGraphImpl::IsValidForTesting(
    const ContextProperties& context_properties,
    const mojom::GraphInfo& graph_info) {
  return ValidateGraph(context_properties, graph_info).has_value();
}

void WebNNGraphImpl::Compute(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  if (!ValidateInputsForComputation(
          named_inputs, compute_resource_info_.input_names_to_descriptors)) {
    mojo::ReportBadMessage(
        "The inputs for computation don't match the built graph's "
        "expectation.");

    // `mojo::ReportBadMessage()` will kill the renderer process, but Mojo
    // complains if the callback is not run. Just run it with nonsense
    // arguments.
    std::move(callback).Run(mojom::ComputeResult::NewError(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Unexpected inputs received from the caller.")));
    return;
  }

  // Call ComputeImpl() implemented by an `mojom::WebNNGraph` backend.
  ComputeImpl(std::move(named_inputs), std::move(callback));
}

void WebNNGraphImpl::Dispatch(
    const base::flat_map<std::string, base::UnguessableToken>& named_inputs,
    const base::flat_map<std::string, base::UnguessableToken>& named_outputs) {
  if (!ValidateWebNNBuffersUsage(named_inputs, named_outputs)) {
    mojo::ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Resolve the token of a input MLBuffer to the corresponding `WebNNBuffer`
  // instance.
  std::vector<std::pair<std::string_view, WebNNBufferImpl*>>
      name_to_input_buffers;
  name_to_input_buffers.reserve(named_inputs.size());
  for (const auto& [name, buffer_handle] : named_inputs) {
    base::optional_ref<WebNNBufferImpl> input_buffer =
        context_->GetWebNNBufferImpl(buffer_handle);
    if (!input_buffer.has_value()) {
      return;
    }
    name_to_input_buffers.emplace_back(name, input_buffer.as_ptr());
  }
  base::flat_map<std::string_view, WebNNBufferImpl*> name_to_input_buffer_map(
      std::move(name_to_input_buffers));
  if (!ValidateWebNNBuffers(
          name_to_input_buffer_map,
          compute_resource_info_.input_names_to_descriptors)) {
    mojo::ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Resolve the token of a output MLBuffer to the corresponding `WebNNBuffer`
  // instance.
  std::vector<std::pair<std::string_view, WebNNBufferImpl*>>
      name_to_output_buffers;
  name_to_output_buffers.reserve(named_outputs.size());
  for (const auto& [name, buffer_handle] : named_outputs) {
    base::optional_ref<WebNNBufferImpl> output_buffer =
        context_->GetWebNNBufferImpl(buffer_handle);
    if (!output_buffer.has_value()) {
      return;
    }
    name_to_output_buffers.emplace_back(name, output_buffer.as_ptr());
  }

  base::flat_map<std::string_view, WebNNBufferImpl*> name_to_output_buffer_map(
      std::move(name_to_output_buffers));
  if (!ValidateWebNNBuffers(
          name_to_output_buffer_map,
          compute_resource_info_.output_names_to_descriptors)) {
    mojo::ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Call DispatchImpl() implemented by an `mojom::WebNNGraph` backend.
  DispatchImpl(name_to_input_buffer_map, name_to_output_buffer_map);
}

}  // namespace webnn
