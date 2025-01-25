// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.settings.SettingsLauncherFactory;
import org.chromium.content_public.browser.WebContents;

// Vivaldi
import org.vivaldi.browser.common.VivaldiUtils;

/** Launches autofill settings subpages. */
public class SettingsLauncherHelper {
    /**
     * Tries showing the settings page for Addresses.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `launchSettingsActivity` was called.
     */
    public static boolean showAutofillProfileSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        // Vivaldi
        if (!VivaldiUtils.isDeviceSecure()) {
            VivaldiUtils.showMissingDeviceLockDialog(context);
            return true;
        }

        RecordUserAction.record("AutofillAddressesViewed");
        SettingsLauncherFactory.createSettingsLauncher()
                .launchSettingsActivity(context, AutofillProfilesFragment.class);
        return true;
    }

    /**
     * Tries showing the settings page for Payments.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `launchSettingsActivity` was called.
     */
    public static boolean showAutofillCreditCardSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        // Vivaldi
        if (!VivaldiUtils.isDeviceSecure()) {
            VivaldiUtils.showMissingDeviceLockDialog(context);
            return true;
        }

        RecordUserAction.record("AutofillCreditCardsViewed");
        SettingsLauncherFactory.createSettingsLauncher()
                .launchSettingsActivity(context, AutofillPaymentMethodsFragment.class);
        return true;
    }

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        showAutofillProfileSettings(webContents.getTopLevelNativeWindow().getActivity().get());
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        showAutofillCreditCardSettings(webContents.getTopLevelNativeWindow().getActivity().get());
    }
}
