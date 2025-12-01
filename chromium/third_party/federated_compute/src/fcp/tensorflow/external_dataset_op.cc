/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "fcp/base/random_token.h"
#include "fcp/tensorflow/external_dataset.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/strcat.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/public/version.h"

namespace fcp {

/**
 * ExternalDataset op-kernel. Delegates to an ExternalDatasetProvider, found
 * from the ExternalDatasetProviderRegistry (a HostObjectRegistry).
 *
 * Inputs:
 *   selector: An opaque string scalar. Forwarded to the stub.
 *   token: String scalar. It should encode a token obtained from
 *          ExternalDatasetProviderRegistry::Register.
 *
 * See TensorFlow's guide to making custom dataset ops:
 * https://www.tensorflow.org/guide/extend/formats
 */
class ExternalDatasetOp : public tensorflow::data::DatasetOpKernel {
 public:
  using tensorflow::data::DatasetOpKernel::DatasetOpKernel;

  void MakeDataset(tensorflow::OpKernelContext* ctx,
                   tensorflow::data::DatasetBase** output) override {
    tensorflow::tstring token_str;
    OP_REQUIRES_OK(ctx,
                   tensorflow::data::ParseScalarArgument<tensorflow::tstring>(
                       ctx, "token", &token_str));
    absl::Span<char const> token_bytes = token_str;
    OP_REQUIRES(ctx, token_bytes.size() == kRandomTokenSizeInBytes,
                tensorflow::errors::InvalidArgument(absl::StrFormat(
                    "Tokens have a fixed size. Expected: %d; Actual %d",
                    kRandomTokenSizeInBytes, token_bytes.size())));
    RandomToken token = RandomToken::FromBytes(token_bytes);

    tensorflow::tstring selector_str;
    OP_REQUIRES_OK(ctx,
                   tensorflow::data::ParseScalarArgument<tensorflow::tstring>(
                       ctx, "selector", &selector_str));

    std::optional<std::shared_ptr<ExternalDatasetProvider>> maybe_provider =
        ExternalDatasetProviderRegistry::TryLookup(token);
    OP_REQUIRES(ctx, maybe_provider.has_value(),
                tensorflow::errors::InvalidArgument(
                    "A dataset provider is not currently registered for the "
                    "provided token: ",
                    token.ToPrintableString()));

    std::shared_ptr<ExternalDatasetProvider> provider =
        *std::move(maybe_provider);
    absl::StatusOr<std::unique_ptr<ExternalDataset>> maybe_dataset =
        provider->MakeDataset(selector_str);
    // The provider might not like the given selector.
    if (!maybe_dataset.ok()) {
      ctx->SetStatus(maybe_dataset.status());
      return;
    }

    *output = new Dataset(ctx, std::move(maybe_dataset).value());
  }

 private:
  class Dataset : public tensorflow::data::DatasetBase {
   public:
    Dataset(tensorflow::OpKernelContext* ctx,
            std::unique_ptr<ExternalDataset> stub)
        : DatasetBase(tensorflow::data::DatasetContext(ctx)),
          stub_(std::move(stub)) {}

    std::unique_ptr<tensorflow::data::IteratorBase> MakeIteratorInternal(
        const std::string& prefix) const override {
      std::unique_ptr<ExternalDatasetIterator> iter = stub_->MakeIterator();
      Iterator::Params params{this, absl::StrCat(prefix, "::ExternalDataset")};
      return std::unique_ptr<tensorflow::data::IteratorBase>(
          new Iterator(params, std::move(iter)));
    }

    // Each iterator element is just a scalar string.

    const tensorflow::DataTypeVector& output_dtypes() const override {
      static auto* const dtypes =
          new tensorflow::DataTypeVector({tensorflow::DT_STRING});
      return *dtypes;
    }

    const std::vector<tensorflow::PartialTensorShape>& output_shapes()
        const override {
      static std::vector<tensorflow::PartialTensorShape>* shapes =
          new std::vector<tensorflow::PartialTensorShape>({{}});
      return *shapes;
    }

    std::string DebugString() const override {
      return "ExternalDatasetOp::Dataset";
    }

    absl::Status InputDatasets(
        std::vector<const DatasetBase*>* inputs) const override {
      // ExternalDatast has no input datasets, so just return OK.
      return absl::OkStatus();
    }

// The `DatasetBase::CheckExternalState()` method was introduced on 8/7/2019. We
// use the `TF_GRAPH_DEF_VERSION` value (which is updated daily) to determine if
// we should add its override.
#if TF_GRAPH_DEF_VERSION > 125
    absl::Status CheckExternalState() const override {
      return absl::OkStatus();
    }
#endif

   protected:
    absl::Status AsGraphDefInternal(tensorflow::data::SerializationContext* ctx,
                                    DatasetGraphDefBuilder* b,
                                    tensorflow::Node** output) const override {
      return ::tensorflow::errors::Unimplemented(
          DebugString(), " does not support serialization.");
    }

   private:
    class Iterator : public tensorflow::data::DatasetIterator<Dataset> {
     public:
      explicit Iterator(const Params& params,
                        std::unique_ptr<ExternalDatasetIterator> stub)
          : DatasetIterator<Dataset>(params), stub_(std::move(stub)) {}

      absl::Status GetNextInternal(tensorflow::data::IteratorContext* ctx,
                                   std::vector<tensorflow::Tensor>* out_tensors,
                                   bool* end_of_sequence) override {
        absl::StatusOr<std::string> maybe_element;
        {
          absl::MutexLock _(&mu_);
          maybe_element = stub_->GetNext();
        }

        if (maybe_element.ok()) {
          std::string element = std::move(maybe_element).value();

          // The {} at the end specifies a scalar tensor.
          tensorflow::Tensor element_tensor(ctx->allocator({}),
                                            tensorflow::DT_STRING, {});
          element_tensor.scalar<tensorflow::tstring>()() = element;

          *end_of_sequence = false;
          out_tensors->push_back(std::move(element_tensor));
          return absl::OkStatus();
        } else {
          *end_of_sequence = true;
          if (maybe_element.status().code() == absl::StatusCode::kOutOfRange) {
            return absl::OkStatus();
          } else {
            return maybe_element.status();
          }
        }
      }

     protected:
      absl::Status SaveInternal(
// `::tensorflow::data::SerializationContext` argument was added on
// 2020-03-17 when `TF_GRAPH_DEF_VERSION` was defined to 343.
#if TF_GRAPH_DEF_VERSION > 343
          tensorflow::data::SerializationContext* ctx,
#endif
          tensorflow::data::IteratorStateWriter* writer) override {
        return ::tensorflow::errors::Unimplemented(
            "Save / Restore of an ExternalDataset iterator is not supported");
      }
      absl::Status RestoreInternal(
          tensorflow::data::IteratorContext* ctx,
          tensorflow::data::IteratorStateReader* reader) override {
        return ::tensorflow::errors::Unimplemented(
            "Save / Restore of an ExternalDataset iterator is not supported");
      }

     private:
      std::unique_ptr<ExternalDatasetIterator> stub_;
      absl::Mutex mu_;
    };

    // Private members of Dataset

    std::unique_ptr<ExternalDataset> stub_;
  };
};

REGISTER_OP("ExternalDataset")
    .Input("token: string")
    .Input("selector: string")
    .Output("handle: variant")
    .SetIsStateful()
    .SetShapeFn(tensorflow::shape_inference::ScalarShape);

REGISTER_KERNEL_BUILDER(Name("ExternalDataset").Device(tensorflow::DEVICE_CPU),
                        ExternalDatasetOp);

}  // namespace fcp
