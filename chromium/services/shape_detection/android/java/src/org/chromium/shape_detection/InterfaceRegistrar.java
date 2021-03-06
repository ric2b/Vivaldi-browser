// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.FaceDetectionProvider;
import org.chromium.shape_detection.mojom.TextDetection;

@JNINamespace("shape_detection")
class InterfaceRegistrar {
    static MessagePipeHandle messagePipeHandleFromNative(int nativeHandle) {
        return CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle();
    }

    @CalledByNative
    static void bindBarcodeDetectionProvider(int nativeHandle) {
        BarcodeDetectionProvider impl = BarcodeDetectionProviderImpl.create();
        if (impl != null) {
            BarcodeDetectionProvider.MANAGER.bind(impl, messagePipeHandleFromNative(nativeHandle));
        }
    }

    @CalledByNative
    static void bindFaceDetectionProvider(int nativeHandle) {
        FaceDetectionProvider.MANAGER.bind(
                new FaceDetectionProviderImpl(), messagePipeHandleFromNative(nativeHandle));
    }

    @CalledByNative
    static void bindTextDetection(int nativeHandle) {
        TextDetection impl = TextDetectionImpl.create();
        if (impl != null) {
            TextDetection.MANAGER.bind(impl, messagePipeHandleFromNative(nativeHandle));
        }
    }
}
