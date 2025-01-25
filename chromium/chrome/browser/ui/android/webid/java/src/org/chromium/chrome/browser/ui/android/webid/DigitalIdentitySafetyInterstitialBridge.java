// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.webid.DigitalIdentityInterstitialType;
import org.chromium.content_public.browser.webid.DigitalIdentityRequestStatusForMetrics;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.url.Origin;

/**
 * Initiates showing modal dialog asking user whether they want to share their identity with
 * website.
 */
public class DigitalIdentitySafetyInterstitialBridge {
    private long mNativeDigitalIdentitySafetyInterstitialBridgeAndroid;

    private DigitalIdentitySafetyInterstitialController mController;

    private DigitalIdentitySafetyInterstitialBridge(
            long digitalIdentitySafetyInterstitialBridgeAndroid) {
        mNativeDigitalIdentitySafetyInterstitialBridgeAndroid =
                digitalIdentitySafetyInterstitialBridgeAndroid;
    }

    @CalledByNative
    public static DigitalIdentitySafetyInterstitialBridge create(
            long digitalIdentitySafetyInterstitialBridgeAndroid) {
        return new DigitalIdentitySafetyInterstitialBridge(
                digitalIdentitySafetyInterstitialBridgeAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeDigitalIdentitySafetyInterstitialBridgeAndroid = 0;
        mController = null;
    }

    @CalledByNative
    public void showInterstitial(
            WindowAndroid window,
            Origin origin,
            @DigitalIdentityInterstitialType int interstitialType) {
        if (window == null) {
            onDone(DigitalIdentityRequestStatusForMetrics.ERROR_OTHER);
            return;
        }

        Activity activity = window.getActivity().get();
        ModalDialogManager modalDialogManager =
                (activity instanceof ModalDialogManagerHolder)
                        ? ((ModalDialogManagerHolder) activity).getModalDialogManager()
                        : null;

        if (modalDialogManager == null) {
            onDone(DigitalIdentityRequestStatusForMetrics.ERROR_OTHER);
            return;
        }

        mController = new DigitalIdentitySafetyInterstitialController(origin);
        mController.show(
                modalDialogManager,
                interstitialType,
                (/*DialogDismissalCause*/ Integer dismissalCause) -> {
                    onDone(
                            dismissalCause.intValue()
                                            == DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    ? DigitalIdentityRequestStatusForMetrics.SUCCESS
                                    : DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED);
                });
    }

    @CalledByNative
    public void abort() {
        if (mController != null) {
            mController.abort();
        }
    }

    public void onDone(@DigitalIdentityRequestStatusForMetrics int statusForMetrics) {
        mController = null;

        if (mNativeDigitalIdentitySafetyInterstitialBridgeAndroid != 0) {
            DigitalIdentitySafetyInterstitialBridgeJni.get()
                    .onInterstitialDone(
                            mNativeDigitalIdentitySafetyInterstitialBridgeAndroid,
                            statusForMetrics);
        }
    }

    @NativeMethods
    interface Natives {
        void onInterstitialDone(
                long nativeDigitalIdentitySafetyInterstitialBridgeAndroid,
                @DigitalIdentityRequestStatusForMetrics int statusForMetrics);
    }
}
