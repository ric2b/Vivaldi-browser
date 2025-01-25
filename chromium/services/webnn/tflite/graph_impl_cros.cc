// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_impl_cros.h"

#include <vector>

#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/tflite/context_impl_cros.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace webnn::tflite {

// static
void GraphImplCrOS::CreateAndBuild(
    ContextImplCrOS* context_impl,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  base::expected<flatbuffers::DetachedBuffer, std::string> conversion_result =
      GraphBuilderTflite::CreateAndBuild(context_impl->properties(),
                                         *graph_info);
  if (!conversion_result.has_value()) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, conversion_result.error())));
    return;
  }

  context_impl->LoadModel(
      std::move(conversion_result.value()),
      base::BindOnce(
          [](base::WeakPtr<WebNNContextImpl> context,
             ComputeResourceInfo compute_resource_info,
             WebNNContextImpl::CreateGraphImplCallback callback,
             ml::model_loader::mojom::LoadModelResult result,
             mojo::PendingRemote<ml::model_loader::mojom::Model> pending_remote,
             ml::model_loader::mojom::ModelInfoPtr tensor_info) {
            if (!context ||
                result != ml::model_loader::mojom::LoadModelResult::kOk) {
              std::move(callback).Run(base::unexpected(
                  mojom::Error::New(mojom::Error::Code::kUnknownError,
                                    "Failed to load model with ml service.")));
              return;
            }

            // TODO(crbug.com/330806169): Pass `WebNNGraph` directly to ML
            // Service and not have to bounce through the browser process.
            std::move(callback).Run(base::WrapUnique(new GraphImplCrOS(
                static_cast<ContextImplCrOS*>(context.get()),
                std::move(compute_resource_info), std::move(pending_remote))));
          },
          context_impl->AsWeakPtr(), std::move(compute_resource_info),
          std::move(callback)));
}

GraphImplCrOS::~GraphImplCrOS() = default;

GraphImplCrOS::GraphImplCrOS(
    ContextImplCrOS* context_impl,
    ComputeResourceInfo compute_resource_info,
    mojo::PendingRemote<ml::model_loader::mojom::Model> pending_remote)
    : WebNNGraphImpl(context_impl, std::move(compute_resource_info)) {
  model_remote_.Bind(std::move(pending_remote));
}

void GraphImplCrOS::ComputeImpl(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  // TODO(crbug.com/330856251): Optimize inference time with shared memory.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> input_tensors;
  input_tensors.reserve(named_inputs.size());
  for (const auto& [name, buffer] : named_inputs) {
    input_tensors.emplace_back(
        name, std::vector<uint8_t>(buffer.begin(), buffer.end()));
  }

  model_remote_->Compute(
      std::move(input_tensors),
      base::BindOnce(
          [](mojom::WebNNGraph::ComputeCallback callback,
             ml::model_loader::mojom::ComputeResult compute_result,
             const std::optional<base::flat_map<
                 std::string, std::vector<uint8_t>>>& output_tensors) {
            if (compute_result != ml::model_loader::mojom::ComputeResult::kOk ||
                !output_tensors.has_value()) {
              std::move(callback).Run(ToError<mojom::ComputeResult>(
                  mojom::Error::Code::kUnknownError,
                  "Failed to obtain the computation result."));
              return;
            }
            std::vector<std::pair<std::string, mojo_base::BigBuffer>>
                named_outputs;
            named_outputs.reserve(output_tensors->size());
            for (const auto& [name, buffer] : *output_tensors) {
              named_outputs.emplace_back(name, mojo_base::BigBuffer(buffer));
            }

            std::move(callback).Run(mojom::ComputeResult::NewNamedOutputs(
                std::move(named_outputs)));
          },
          std::move(callback)));
}

void GraphImplCrOS::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs) {
  // TODO(crbug.com/40278771): Implement MLTensor for TFLite. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
}

}  // namespace webnn::tflite
