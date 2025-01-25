// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.base.cached_flags.CachedFieldTrialParameter;
import org.chromium.base.cached_flags.CachedFlag;
import org.chromium.base.cached_flags.IntCachedFieldTrialParameter;

import java.util.ArrayList;
import java.util.List;

/** This is the place where we define these: List of Omnibox features and parameters. */
public class OmniboxFeatures {
    // Threshold for low RAM devices. We won't be showing suggestion images
    // on devices that have less RAM than this to avoid bloat and reduce user-visible
    // slowdown while spinning up an image decompression process.
    // We set the threshold to 1.5GB to reduce number of users affected by this restriction.
    private static final int LOW_MEMORY_THRESHOLD_KB = (int) (1.5 * 1024 * 1024);

    // Maximum number of attempts to retrieve page behind the default match per Omnibox input
    // session.
    public static final int DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION = 5;

    // Auto-populated list of Omnibox cached feature flags.
    // Each flag created via newFlag() will be automatically added to this list.
    private static final List<CachedFlag> sCachedFlags = new ArrayList<>();
    private static final List<CachedFieldTrialParameter> sCachedParams = new ArrayList<>();

    /// Holds the information whether logic should focus on preserving memory on this device.
    private static Boolean sIsLowMemoryDevice;

    public static final CachedFlag sOmniboxAnswerActions =
            newFlag(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS, false);

    public static final CachedFlag sAnimateSuggestionsListAppearance =
            newFlag(OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE, false);

    public static final CachedFlag sTouchDownTriggerForPrefetch =
            newFlag(OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH, false);

    public static final CachedFlag sQueryTilesInZPSOnNTP =
            newFlag(OmniboxFeatureList.QUERY_TILES_IN_ZPS_ON_NTP, false);

    public static final CachedFlag sRichInlineAutocomplete =
            newFlag(OmniboxFeatureList.RICH_AUTOCOMPLETION, false);

    /**
     * Whether GeolocationHeader should use {@link
     * com.google.android.gms.location.FusedLocationProviderClient} to determine the location sent
     * in omnibox requests.
     */
    public static final CachedFlag sUseFusedLocationProvider =
            newFlag(OmniboxFeatureList.USE_FUSED_LOCATION_PROVIDER, false);

    public static final CachedFlag sAsyncViewInflation =
            newFlag(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION, false);

    public static final CachedFlag sElegantTextHeight =
            newFlag(OmniboxFeatureList.OMNIBOX_ELEGANT_TEXT_HEIGHT, false);

    public static final BooleanCachedFieldTrialParameter QUERY_TILES_SHOW_AS_CAROUSEL =
            newBooleanParam(sQueryTilesInZPSOnNTP, "QueryTilesShowAsCarousel", false);

    public static final BooleanCachedFieldTrialParameter sAnswerActionsShowAboveKeyboard =
            newBooleanParam(sOmniboxAnswerActions, "AnswerActionsShowAboveKeyboard", false);

    public static final BooleanCachedFieldTrialParameter sAnswerActionsShowIfUrlsPresent =
            newBooleanParam(sOmniboxAnswerActions, "ShowIfUrlsPresent", false);

    public static final BooleanCachedFieldTrialParameter sAnswerActionsShowRichCard =
            newBooleanParam(sOmniboxAnswerActions, "ShowRichCard", false);

    public static final IntCachedFieldTrialParameter sTouchDownTriggerMaxPrefetchesPerSession =
            newIntParam(
                    sTouchDownTriggerForPrefetch,
                    "max_prefetches_per_omnibox_session",
                    DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION);

    public static final BooleanCachedFieldTrialParameter sRichInlineShowFullUrl =
            newBooleanParam(sRichInlineAutocomplete, "rich_autocomplete_full_url", false);

    public static final IntCachedFieldTrialParameter sRichInlineMinimumInputChars =
            newIntParam(
                    sRichInlineAutocomplete,
                    "rich_autocomplete_minimum_characters",
                    Integer.MAX_VALUE);

    /**
     * Create an instance of a CachedFeatureFlag.
     *
     * @param featureName the name of the feature flag
     * @param defaultValue the default value to return if the feature state is unknown
     */
    private static CachedFlag newFlag(String featureName, boolean defaultValue) {
        var cachedFlag = new CachedFlag(OmniboxFeatureMap.getInstance(), featureName, defaultValue);
        sCachedFlags.add(cachedFlag);
        return cachedFlag;
    }

    /**
     * Create an instance of a BooleanCachedFieldTrialParameter.
     *
     * <p>Newly created flag will be automatically added to list of persisted feature flags.
     *
     * @param flag the Feature flag the parameter is associated with
     * @param variationName the name of the associated parameter
     * @param defaultValue the default value to return if the feature state is unknown
     */
    private static BooleanCachedFieldTrialParameter newBooleanParam(
            CachedFlag flag, String variationName, boolean defaultValue) {
        var param =
                new BooleanCachedFieldTrialParameter(
                        OmniboxFeatureMap.getInstance(), flag.getFeatureName(), variationName, defaultValue);
        sCachedParams.add(param);
        return param;
    }

    /**
     * Create an instance of a IntCachedFieldTrialParameter.
     *
     * <p>Newly created flag will be automatically added to list of persisted feature flags.
     *
     * @param flag the Feature flag the parameter is associated with
     * @param variationName the name of the associated parameter
     * @param defaultValue the default value to return if the feature state is unknown
     */
    private static IntCachedFieldTrialParameter newIntParam(
            CachedFlag flag, String variationName, int defaultValue) {
        var param =
                new IntCachedFieldTrialParameter(
                        OmniboxFeatureMap.getInstance(), flag.getFeatureName(), variationName, defaultValue);
        sCachedParams.add(param);
        return param;
    }

    /** Retrieve list of CachedFlags that should be cached. */
    public static List<CachedFlag> getFieldTrialsToCache() {
        return sCachedFlags;
    }

    /** Retrieve list of FieldTrialParams that should be cached. */
    public static List<CachedFieldTrialParameter> getFieldTrialParamsToCache() {
        return sCachedParams;
    }

    /**
     * Returns whether the omnibox's recycler view pool should be pre-warmed prior to initial use.
     */
    public static boolean shouldPreWarmRecyclerViewPool() {
        return !isLowMemoryDevice();
    }

    /**
     * Returns whether the device is to be considered low-end for any memory intensive operations.
     */
    public static boolean isLowMemoryDevice() {
        if (sIsLowMemoryDevice == null) {
            sIsLowMemoryDevice =
                    (SysUtils.amountOfPhysicalMemoryKB() < LOW_MEMORY_THRESHOLD_KB
                            && !CommandLine.getInstance()
                                    .hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE));
        }
        return sIsLowMemoryDevice;
    }

    /**
     * Returns whether a touch down event on a search suggestion should send a signal to prefetch
     * the corresponding page.
     */
    public static boolean isTouchDownTriggerForPrefetchEnabled() {
        return sTouchDownTriggerForPrefetch.isEnabled();
    }

    /**
     * Returns the maximum number of prefetches that can be triggered by touch down events within an
     * omnibox session.
     */
    public static int getMaxPrefetchesPerOmniboxSession() {
        return sTouchDownTriggerMaxPrefetchesPerSession.getValue();
    }

    /** Returns whether answer suggestions should be annotated with attached action chips. */
    public static boolean shouldShowAnswerActions() {
        return sOmniboxAnswerActions.isEnabled();
    }

    /** Returns whether answers with actions should be re-ordered to just above the keyboard */
    public static boolean shouldShowAnswerWithActionsAboveKeyboard() {
        return shouldShowAnswerActions() && sAnswerActionsShowAboveKeyboard.getValue();
    }

    /**
     * Returns whether answers with actions should be displayed if there are url suggestions
     * present.
     */
    public static boolean shouldShowAnswerWithActionsIfUrlsPresent() {
        return shouldShowAnswerActions() && sAnswerActionsShowIfUrlsPresent.getValue();
    }

    /** Returns whether answers with actions should be presented as a rich card */
    public static boolean shouldShowRichAnswerCard() {
        return shouldShowAnswerActions() && sAnswerActionsShowRichCard.getValue();
    }

    /**
     * Whether the appearance of the omnibox suggestions list should animated in sync with the soft
     * keyboard.
     */
    public static boolean shouldAnimateSuggestionsListAppearance() {
        return sAnimateSuggestionsListAppearance.isEnabled();
    }

    /** Indicate a low memory device for testing purposes. */
    public static void setIsLowMemoryDeviceForTesting(boolean isLowMemDevice) {
        sIsLowMemoryDevice = isLowMemDevice;
        ResettersForTesting.register(() -> sIsLowMemoryDevice = null);
    }

    /**
     * Returns whether the rich inline autocomplete URL should be shown.
     *
     * @param inputCount the count of characters user input.
     * @return Whether the rich inline autocomplete URL should be shown.
     */
    public static boolean shouldShowRichInlineAutocompleteUrl(int inputCount) {
        return sRichInlineAutocomplete.isEnabled()
                && sRichInlineShowFullUrl.getValue()
                && inputCount >= sRichInlineMinimumInputChars.getValue();
    }
}
