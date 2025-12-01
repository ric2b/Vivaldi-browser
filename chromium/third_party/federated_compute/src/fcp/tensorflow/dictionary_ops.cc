/*
 * Copyright 2022 Google LLC
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
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/dictionary/dictionary.h"
#include "fcp/dictionary/dictionary.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/public/version.h"
#include "tensorflow/tsl/platform/tstring.h"

namespace tf = tensorflow;

namespace fcp {
namespace tensorflow {

using fcp::dictionary::Dictionary;
using fcp::dictionary::DictionaryDescription;

namespace {

// Base class for ops that work with a Dictionary.
//
// Subclasses need to provide Compute and register appropriately using
// REGISTER_OP.
class AbstractDictionaryOp : public tf::OpKernel {
 public:
  explicit AbstractDictionaryOp(tf::OpKernelConstruction* context,
                                int32_t num_expected_inputs)
      : tf::OpKernel(context), num_expected_inputs_(num_expected_inputs) {
    std::string dictionary_description_string;
    OP_REQUIRES_OK(context,
                   context->GetAttr("dictionary_description_proto",
                                    &dictionary_description_string));

    DictionaryDescription parsed_dictionary_description;
    OP_REQUIRES(context,
                parsed_dictionary_description.ParseFromString(
                    dictionary_description_string),
                tf::errors::InvalidArgument(
                    "Cannot parse provided DictionaryDescription."));

    if (parsed_dictionary_description.has_vocabulary()) {
      // Fully specified dictionary.
      absl::StatusOr<std::unique_ptr<Dictionary>> dictionary(
          Dictionary::Create(parsed_dictionary_description));
      OP_REQUIRES(context, dictionary.ok(),
                  tf::errors::InvalidArgument(dictionary.status().ToString()));
      dictionary_ = *std::move(dictionary);
      parsed_dictionary_description.clear_vocabulary();  // Save space.
    }
    dictionary_description_ = parsed_dictionary_description;
  }

  void Compute(tf::OpKernelContext* context) override {
    FCP_CHECK(num_expected_inputs_ == context->num_inputs());

    // Use the dictionary_ constructed at setup.
    OP_REQUIRES(context, dictionary_ != nullptr,
                tf::errors::InvalidArgument(
                "DictionaryDescription does not contain a vocabulary. "));
    absl::Status status = DoCompute(context, *dictionary_);
    OP_REQUIRES(context, status.ok(),
                tf::errors::InvalidArgument(std::string(status.message())));
  }

 protected:
  // Computes using the given dictionary.
  virtual absl::Status DoCompute(tf::OpKernelContext* context,
                                 const Dictionary& dictionary) = 0;

 private:
  DictionaryDescription dictionary_description_;
  std::unique_ptr<Dictionary> dictionary_;
  const int32_t num_expected_inputs_;
};

}  // namespace

class DictionarySize : public AbstractDictionaryOp {
 public:
  explicit DictionarySize(tf::OpKernelConstruction* context)
      : AbstractDictionaryOp(context, 0 /* num_expected_inputs */) {}

 protected:
  absl::Status DoCompute(tf::OpKernelContext* context,
                         const Dictionary& dictionary) override {
    tf::Tensor* size_tensor;
    auto status =
        context->allocate_output(0, tf::TensorShape({}), &size_tensor);
    if (!status.ok()) {
#if TF_GRAPH_DEF_VERSION < 1467
      return absl::InternalError(status.error_message());
#else
      return absl::InternalError(status.message());
#endif
    }
    size_tensor->flat<int64_t>()(0) = dictionary.Size();
    return absl::OkStatus();
  }
};

REGISTER_KERNEL_BUILDER(Name("DictionarySize").Device(tf::DEVICE_CPU),
                        DictionarySize);
REGISTER_OP("DictionarySize")
    .Output("size: int64")
    .Attr("dictionary_description_proto: string = ''")
    .SetShapeFn(::tensorflow::shape_inference::ScalarShape)
    .Doc(R"doc(
Returns the number of ids in the given dictionary.

The dictionary should be fully specified at construction time via the
dictionary_description_proto.

dictionary_description_proto: A `DictionaryDescription` as a string.
)doc");

class DictionaryLookup : public AbstractDictionaryOp {
 public:
  explicit DictionaryLookup(tf::OpKernelConstruction* context)
      : AbstractDictionaryOp(context, 1 /* num_expected_inputs */) {}

 protected:
  absl::Status DoCompute(tf::OpKernelContext* context,
                         const Dictionary& dictionary) override {
    const tf::Tensor& token_tensor = context->input(0);
    tf::Tensor* ids_tensor;
    auto status =
        context->allocate_output(0, token_tensor.shape(), &ids_tensor);
    if (!status.ok()) {
#if TF_GRAPH_DEF_VERSION < 1467
      return absl::InternalError(status.error_message());
#else
      return absl::InternalError(status.message());
#endif
    }

    if (token_tensor.dtype() != tf::DataType::DT_STRING) {
      return absl::InvalidArgumentError("Expected input of 'tokens'.");
    }
    if (ids_tensor->dtype() != tf::DataType::DT_INT64) {
      return absl::InvalidArgumentError("Expected output of 'ids'.");
    }
    if (token_tensor.shape() != ids_tensor->shape()) {
      return absl::InvalidArgumentError("Wrong shape for ids_tensor");
    }
    const auto tokens_flat = token_tensor.flat<tf::tstring>();
    auto ids_flat = ids_tensor->flat<int64_t>();
    const int64_t num_tokens = tokens_flat.size();
    for (int i = 0; i < num_tokens; ++i) {
      const absl::string_view token = tokens_flat(i);
      if (token.data() == nullptr && !token.empty()) {
        return absl::InternalError(
            absl::StrCat("Encountered token with nullptr data(), type: ",
                         tokens_flat(i).type(), ", size: ", token.size(),
                         ", capacity: ", tokens_flat(i).capacity()));
      }
      ids_flat(i) = dictionary.TokenToId(token);
    }
    return absl::OkStatus();
  }
};

REGISTER_KERNEL_BUILDER(Name("DictionaryLookup").Device(tf::DEVICE_CPU),
                        DictionaryLookup);
REGISTER_OP("DictionaryLookup")
    .Input("tokens: string")
    .Output("token_ids: int64")
    .Attr("dictionary_description_proto: string = ''")
    .SetShapeFn(::tensorflow::shape_inference::UnchangedShape)
    .Doc(R"doc(
Maps each string to an id by lookup in the dictionary.

The dictionary should be fully specified at construction time via the
dictionary_description_proto. Output has the same shape as input.

tokens: A `Tensor` of strings to lookup in the dictionary.
dictionary_description_proto: A `DictionaryDescription` as a string.
)doc");

class DictionaryReverseLookup : public AbstractDictionaryOp {
 public:
  explicit DictionaryReverseLookup(tf::OpKernelConstruction* context)
      : AbstractDictionaryOp(context, 1 /* num_expected_inputs */) {}

 protected:
  absl::Status DoCompute(tf::OpKernelContext* context,
                         const Dictionary& dictionary) override {
    const tf::Tensor& ids_tensor = context->input(0);
    tf::Tensor* token_tensor;
    auto status =
        context->allocate_output(0, ids_tensor.shape(), &token_tensor);
    if (!status.ok()) {
#if TF_GRAPH_DEF_VERSION < 1467
      return absl::InternalError(status.error_message());
#else
      return absl::InternalError(status.message());
#endif
    }

    if (token_tensor->dtype() != tf::DataType::DT_STRING) {
      return absl::InvalidArgumentError("Expected input of 'tokens'.");
    }
    if (ids_tensor.dtype() != tf::DataType::DT_INT64) {
      return absl::InvalidArgumentError("Expected output of 'ids'.");
    }
    if (ids_tensor.shape() != token_tensor->shape()) {
      return absl::InvalidArgumentError("Wrong shape for token_tensor");
    }

    const auto ids_flat = ids_tensor.flat<int64_t>();
    auto tokens_flat = token_tensor->flat<tf::tstring>();
    const int64_t num_tokens = ids_flat.size();
    for (int i = 0; i < num_tokens; ++i) {
      tokens_flat(i) = dictionary.IdToToken(ids_flat(i));
    }
    return absl::OkStatus();
  }
};

REGISTER_KERNEL_BUILDER(Name("DictionaryReverseLookup").Device(tf::DEVICE_CPU),
                        DictionaryReverseLookup);
REGISTER_OP("DictionaryReverseLookup")
    .Input("token_ids: int64")
    .Output("tokens: string")
    .Attr("dictionary_description_proto: string = ''")
    .SetShapeFn(::tensorflow::shape_inference::UnchangedShape)
    .Doc(R"doc(
Maps each id to its string by performing a reverse lookup in the dictionary.

The dictionary should be fully specified at construction time via the
dictionary_description_proto. Output has the same shape as input.

token_ids: A `Tensor` of int64 ids to lookup in the dictionary.
dictionary_description_proto: A `DictionaryDescription` as a string.
)doc");
}  // namespace tensorflow
}  // namespace fcp
