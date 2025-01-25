// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test_utils.h"

#include <numeric>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

MLGraphBuilder* CreateMLGraphBuilder(ExecutionContext* execution_context,
                                     ScriptState* script_state,
                                     ExceptionState& exception_state,
                                     MLContextOptions* options) {
  ML* ml = MakeGarbageCollected<ML>(execution_context);

  ScriptPromise<MLContext> promise =
      ml->createContext(script_state, options, exception_state);
  ScriptPromiseTester tester(script_state, promise);
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return nullptr;
  }

  MLContext* ml_context = V8MLContext::ToWrappable(tester.Value().GetIsolate(),
                                                   tester.Value().V8Value());
  CHECK(ml_context);
  return MLGraphBuilder::Create(ml_context);
}

MLOperand* BuildInput(MLGraphBuilder* builder,
                      const String& name,
                      const Vector<uint32_t>& dimensions,
                      V8MLOperandDataType::Enum data_type,
                      ExceptionState& exception_state) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setDimensions(dimensions);
  desc->setDataType(data_type);
  return builder->input(name, desc, exception_state);
}

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandDataType::Enum data_type) {
  NotShared<DOMArrayBufferView> buffer_view;
  switch (data_type) {
    case V8MLOperandDataType::Enum::kFloat32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMFloat32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kFloat16: {
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint16Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kInt32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMInt32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kUint32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kInt64: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMBigInt64Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kUint64: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMBigUint64Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kInt8: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMInt8Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kUint8: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint8Array::CreateOrNull(size));
      break;
    }
  }
  return buffer_view;
}

MLOperand* BuildConstant(
    MLGraphBuilder* builder,
    const Vector<uint32_t>& dimensions,
    V8MLOperandDataType::Enum data_type,
    ExceptionState& exception_state,
    std::optional<NotShared<DOMArrayBufferView>> user_buffer_view) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setDimensions(dimensions);
  desc->setDataType(data_type);
  size_t size = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                std::multiplies<uint32_t>());

  NotShared<DOMArrayBufferView> buffer_view =
      user_buffer_view ? std::move(user_buffer_view.value())
                       : CreateDOMArrayBufferView(size, data_type);
  if (buffer_view.Get() == nullptr) {
    return nullptr;
  }
  return builder->constant(desc, buffer_view, exception_state);
}

}  // namespace blink
