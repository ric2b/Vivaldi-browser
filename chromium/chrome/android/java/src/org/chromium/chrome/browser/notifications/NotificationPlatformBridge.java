// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.chromium.components.content_settings.PrefNames.NOTIFICATIONS_VIBRATE_ENABLED;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.StyleSpan;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.NotificationManagerCompat;
import androidx.preference.PreferenceFragmentCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebApkServiceClient;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.url.URI;
import org.chromium.webapk.lib.client.WebApkIdentityServiceClient;

import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

/**
 * Provides the ability for the NotificationPlatformBridgeAndroid to talk to the Android platform
 * notification system.
 *
 * This class should only be used on the UI thread.
 */
public class NotificationPlatformBridge {
    private static final String TAG = NotificationPlatformBridge.class.getSimpleName();

    // We always use the same integer id when showing and closing notifications. The notification
    // tag is always set, which is a safe and sufficient way of identifying a notification, so the
    // integer id is not needed anymore except it must not vary in an uncontrolled way.
    public static final int PLATFORM_ID = -1;

    // We always use the same request code for pending intents. We use other ways to force
    // uniqueness of pending intents when necessary.
    private static final int PENDING_INTENT_REQUEST_CODE = 0;

    private static final int[] EMPTY_VIBRATION_PATTERN = new int[0];

    // The duration after which the "provisionally unsubscribed" service notification is auto-closed
    // and the permission revocation commits.
    // TODO(crbug.com/41494393): Fine tune this duration, and possibly turn it off for A11Y users.
    private static final long PROVISIONAL_UNSUBSCRIBE_DURATION_MS = 10 * 1000;

    private static NotificationPlatformBridge sInstance;

    private static BaseNotificationManagerProxy sNotificationManagerOverride;

    private final long mNativeNotificationPlatformBridge;

    private final BaseNotificationManagerProxy mNotificationManager;

    private long mLastNotificationClickMs;

    // Set of origins that are showing the "provisionally unsubscribed" service notification, and
    // for which we will revoke the permission after a grace period of
    // `PROVISIONAL_UNSUBSCRIBE_DURATION_MS`, unless the user hits the `ACTION_UNDO_UNSUBSCRIBE`.
    private Set<String> mOriginsWithProvisionallyRevokedPermissions;

    // Set of `notificationId`s for which we need to expressly clear an Android-side dismiss timeout
    // if the `notificationId` is to be re-used for another notification, otherwise Android will
    // mercilessly kill the new notification.
    private Set<String> mNotificationIdsWithStaleTimeouts;

    private TrustedWebActivityClient mTwaClient;

    /** Encapsulates attributes that identify a notification and where it originates from. */
    private static class NotificationIdentifyingAttributes {
        public final String notificationId;
        public final @NotificationType int notificationType;
        public final String origin;
        public final String scopeUrl;
        public final String profileId;
        public final boolean incognito;
        public final String webApkPackage;

        public NotificationIdentifyingAttributes(
                String notificationId,
                @NotificationType int notificationType,
                String origin,
                String scopeUrl,
                String profileId,
                boolean incognito,
                String webApkPackage) {
            this.notificationId = notificationId;
            this.notificationType = notificationType;
            this.origin = origin;
            this.scopeUrl = scopeUrl;
            this.profileId = profileId;
            this.incognito = incognito;
            this.webApkPackage = webApkPackage;
        }
    }

    /**
     * Creates a new instance of the NotificationPlatformBridge.
     *
     * @param nativeNotificationPlatformBridge Instance of the NotificationPlatformBridgeAndroid
     *        class.
     */
    @CalledByNative
    private static NotificationPlatformBridge create(long nativeNotificationPlatformBridge) {
        if (sInstance != null) {
            throw new IllegalStateException(
                    "There must only be a single NotificationPlatformBridge.");
        }

        sInstance = new NotificationPlatformBridge(nativeNotificationPlatformBridge);
        return sInstance;
    }

    /**
     * Returns the current instance of the NotificationPlatformBridge.
     *
     * @return The instance of the NotificationPlatformBridge, if any.
     */
    @Nullable
    static NotificationPlatformBridge getInstanceForTests() {
        return sInstance;
    }

    /**
     * Overrides the notification manager which is to be used for displaying Notifications on the
     * Android framework. Should only be used for testing. Tests are expected to clean up after
     * themselves by setting this to NULL again.
     *
     * @param notificationManager The notification manager instance to use instead of the system's.
     */
    static void overrideNotificationManagerForTesting(
            BaseNotificationManagerProxy notificationManager) {
        sNotificationManagerOverride = notificationManager;
    }

    private NotificationPlatformBridge(long nativeNotificationPlatformBridge) {
        mNativeNotificationPlatformBridge = nativeNotificationPlatformBridge;
        Context context = ContextUtils.getApplicationContext();
        if (sNotificationManagerOverride != null) {
            mNotificationManager = sNotificationManagerOverride;
        } else {
            mNotificationManager = BaseNotificationManagerProxyFactory.create(context);
        }

        // This set will be reset to empty if the application process is killed and then restarted.
        // However, this is unlikely during the brief `PROVISIONAL_UNSUBSCRIBE_DURATION_MS` period.
        // Even if it happens, it is not catastrophic: The revocation will still happen as that is
        // wired up to the service notification getting closed. However, we won't suppress new
        // notifications anymore.
        mOriginsWithProvisionallyRevokedPermissions = new HashSet<String>();

        // This will also be reset on process kill, however, it will be non-empty only for periods
        // of O(k*100ms). If it does get wiped, we may cancel some notifications prematurely.
        mNotificationIdsWithStaleTimeouts = new HashSet<String>();
    }

    /**
     * Marks the current instance as being freed, allowing for a new NotificationPlatformBridge
     * object to be initialized.
     */
    @CalledByNative
    private void destroy() {
        assert sInstance == this;
        sInstance = null;
    }

    /**
     * Invoked by the NotificationService when a Notification intent has been received. There may
     * not be an active instance of the NotificationPlatformBridge at this time, so inform the
     * native side through a static method, initializing both ends if needed.
     *
     * @param intent The intent as received by the Notification service.
     * @return Whether the event could be handled by the native Notification bridge.
     */
    static boolean dispatchNotificationEvent(Intent intent) {
        if (sInstance == null) {
            NotificationPlatformBridgeJni.get().initializeNotificationPlatformBridge();
            if (sInstance == null) {
                Log.e(TAG, "Unable to initialize the native NotificationPlatformBridge.");
                return false;
            }
        }
        recordJobStartDelayUMA(intent);
        recordJobNativeStartupDuration(intent);

        NotificationIdentifyingAttributes attributes =
                new NotificationIdentifyingAttributes(
                        /* notificationId= */ intent.getStringExtra(
                                NotificationConstants.EXTRA_NOTIFICATION_ID),
                        /* notificationType= */ intent.getIntExtra(
                                NotificationConstants.EXTRA_NOTIFICATION_TYPE,
                                NotificationType.WEB_PERSISTENT),
                        /* origin= */ intent.getStringExtra(
                                NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN),
                        /* scopeUrl= */ Objects.requireNonNullElse(
                                intent.getStringExtra(
                                        NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE),
                                ""),
                        /* profileId= */ intent.getStringExtra(
                                NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID),
                        /* incognito= */ intent.getBooleanExtra(
                                NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO,
                                false),
                        /* webApkPackage= */ Objects.requireNonNullElse(
                                intent.getStringExtra(
                                        NotificationConstants
                                                .EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE),
                                ""));

        Log.i(
                TAG,
                String.format(
                        "Dispatching notification event to native: id=%s action=%s",
                        attributes.notificationId, intent.getAction()));

        if (NotificationConstants.ACTION_CLICK_NOTIFICATION.equals(intent.getAction())) {
            int actionIndex =
                    intent.getIntExtra(
                            NotificationConstants.EXTRA_NOTIFICATION_INFO_ACTION_INDEX, -1);
            sInstance.onNotificationClicked(attributes, actionIndex, getNotificationReply(intent));
            return true;
        } else if (NotificationConstants.ACTION_CLOSE_NOTIFICATION.equals(intent.getAction())) {
            // Notification deleteIntent is executed only "when the notification is explicitly
            // dismissed by the user, either with the 'Clear All' button or by swiping it away
            // individually" (though a third-party NotificationListenerService may also trigger it).
            sInstance.onNotificationClosed(attributes, /* byUser= */ true);
            return true;
        } else if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
            sInstance.onNotificationPreUnsubcribe(attributes);
            return true;
        } else if (NotificationConstants.ACTION_UNDO_UNSUBSCRIBE.equals(intent.getAction())) {
            sInstance.onNotificationUndoUnsubcribe(attributes);
            return true;
        } else if (NotificationConstants.ACTION_COMMIT_UNSUBSCRIBE.equals(intent.getAction())) {
            sInstance.onNotificationCommitUnsubscribe(attributes);
            return true;
        }

        Log.e(TAG, "Unrecognized Notification action: " + intent.getAction());
        return false;
    }

    private static void recordJobStartDelayUMA(Intent intent) {
        if (intent.hasExtra(NotificationConstants.EXTRA_JOB_SCHEDULED_TIME_MS)
                && intent.hasExtra(NotificationConstants.EXTRA_JOB_STARTED_TIME_MS)) {
            long duration =
                    intent.getLongExtra(NotificationConstants.EXTRA_JOB_STARTED_TIME_MS, -1)
                            - intent.getLongExtra(
                                    NotificationConstants.EXTRA_JOB_SCHEDULED_TIME_MS, -1);
            if (duration < 0) return; // Possible if device rebooted before job started.
            RecordHistogram.recordMediumTimesHistogram(
                    "Notifications.Android.JobStartDelay", duration);
            if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
                RecordHistogram.recordMediumTimesHistogram(
                        "Notifications.Android.JobStartDelay.PreUnsubscribe", duration);
            }
        }
    }

    private static void recordJobNativeStartupDuration(Intent intent) {
        if (intent.hasExtra(NotificationConstants.EXTRA_JOB_STARTED_TIME_MS)) {
            long duration =
                    SystemClock.elapsedRealtime()
                            - intent.getLongExtra(
                                    NotificationConstants.EXTRA_JOB_STARTED_TIME_MS, -1);
            RecordHistogram.recordMediumTimesHistogram(
                    "Notifications.Android.JobNativeStartupDuration", duration);
            if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
                RecordHistogram.recordMediumTimesHistogram(
                        "Notifications.Android.JobNativeStartupDuration.PreUnsubscribe", duration);
            }
        }
    }

    static @Nullable String getNotificationReply(Intent intent) {
        if (intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_REPLY) != null) {
            // If the notification click went through the job scheduler, we will have set
            // the reply as a standard string extra.
            return intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_REPLY);
        }
        Bundle remoteInputResults = RemoteInput.getResultsFromIntent(intent);
        if (remoteInputResults != null) {
            CharSequence reply =
                    remoteInputResults.getCharSequence(NotificationConstants.KEY_TEXT_REPLY);
            if (reply != null) {
                return reply.toString();
            }
        }
        return null;
    }

    /**
     * Launches the notifications preferences screen. If the received intent indicates it came
     * from the gear button on a flipped notification, this launches the site specific preferences
     * screen.
     *
     * @param incomingIntent The received intent.
     */
    public static void launchNotificationPreferences(Intent incomingIntent) {
        // This method handles an intent fired by the Android system. There is no guarantee that the
        // native library is loaded at this point. The native library is needed for the preferences
        // activity, and it loads the library, but there are some native calls even before that
        // activity is started: from RecordUserAction.record and (indirectly) from
        // UrlFormatter.formatUrlForSecurityDisplay.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        // Use the application context because it lives longer. When using the given context, it
        // may be stopped before the preferences intent is handled.
        Context applicationContext = ContextUtils.getApplicationContext();

        // If we can read an origin from the intent, use it to open the settings screen for that
        // origin.
        String origin = getOriginFromIntent(incomingIntent);
        boolean launchSingleWebsitePreferences = origin != null;

        Bundle fragmentArguments;
        if (launchSingleWebsitePreferences) {
            // Record that the user has clicked on the [Site Settings] button.
            RecordUserAction.record("Notifications.ShowSiteSettings");

            // All preferences for a specific origin.
            fragmentArguments = SingleWebsiteSettings.createFragmentArgsForSite(origin);
        } else {
            // Notification preferences for all origins.
            fragmentArguments = new Bundle();
            fragmentArguments.putString(
                    SingleCategorySettings.EXTRA_CATEGORY,
                    SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.NOTIFICATIONS));
            fragmentArguments.putString(
                    SingleCategorySettings.EXTRA_TITLE,
                    applicationContext
                            .getResources()
                            .getString(R.string.push_notifications_permission_title));
        }

        Class<? extends PreferenceFragmentCompat> fragment =
                launchSingleWebsitePreferences
                        ? SingleWebsiteSettings.class
                        : SingleCategorySettings.class;
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(applicationContext, fragment, fragmentArguments);
    }

    /**
     * Returns a bogus Uri used to make each intent unique according to Intent#filterEquals.
     * Without this, the pending intents derived from the intent may be reused, because extras are
     * not taken into account for the filterEquals comparison.
     *
     * @param notificationId The id of the notification.
     * @param origin The origin to whom the notification belongs.
     * @param actionIndex The zero-based index of the action button, or -1 if not applicable.
     */
    private Uri makeIntentData(String notificationId, String origin, int actionIndex) {
        return Uri.parse(origin).buildUpon().fragment(notificationId + "," + actionIndex).build();
    }

    /**
     * Returns the PendingIntent for completing |action| on the notification identified by the data
     * in the other parameters.
     *
     * <p>All parameters set here should also be set in {@link
     * NotificationJobService#getJobExtrasFromIntent(Intent)}.
     *
     * @param attributes Attributes identifying the notification and its source.
     * @param action The action this pending intent will represent.
     * @param actionIndex The zero-based index of the action button, or -1 if not applicable.
     * @param mutable Whether the pending intent is mutable, see {@link
     *     PendingIntent#FLAG_IMMUTABLE}.
     */
    private PendingIntentProvider makePendingIntent(
            NotificationIdentifyingAttributes attributes,
            String action,
            int actionIndex,
            boolean mutable) {
        Context context = ContextUtils.getApplicationContext();
        Uri intentData = makeIntentData(attributes.notificationId, attributes.origin, actionIndex);
        boolean useServiceIntent =
                NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(action)
                        && NotificationIntentInterceptor
                                .shouldUseServiceIntentForPreUnsubscribeAction();
        Intent intent = new Intent(action, intentData);
        intent.setClass(
                context,
                useServiceIntent
                        ? NotificationService.class
                        : NotificationServiceImpl.Receiver.class);

        // Make sure to update NotificationJobService.getJobExtrasFromIntent() when changing any
        // of the extras included with the |intent|.
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_ID, attributes.notificationId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_TYPE, attributes.notificationType);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN, attributes.origin);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE, attributes.scopeUrl);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID, attributes.profileId);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO,
                attributes.incognito);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE,
                attributes.webApkPackage);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ACTION_INDEX, actionIndex);

        // This flag ensures the broadcast is delivered with foreground priority. It also means the
        // receiver gets a shorter timeout interval before it may be killed, but this is ok because
        // we schedule a job to handle the intent in NotificationService.Receiver.
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);

        if (useServiceIntent) {
            return PendingIntentProvider.getService(
                    context,
                    PENDING_INTENT_REQUEST_CODE,
                    intent,
                    PendingIntent.FLAG_UPDATE_CURRENT,
                    mutable);
        }

        return PendingIntentProvider.getBroadcast(
                context,
                PENDING_INTENT_REQUEST_CODE,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT,
                mutable);
    }

    /**
     * Attempts to extract an origin from the tag extras in the given intent.
     *
     * There are two tags that are relevant, either or none of them may be set, but not both:
     *   1. Notification.EXTRA_CHANNEL_ID - set by Android on the 'Additional settings in the app'
     *      button intent from individual channel settings screens in Android O.
     *   2. NotificationConstants.EXTRA_NOTIFICATION_TAG - set by us on browser UI that should
     *     launch specific site settings, e.g. the web notifications Site Settings button.
     *
     * See {@link SiteChannelsManager#createChannelId} and {@link SiteChannelsManager#toSiteOrigin}
     * for how we convert origins to and from channel ids.
     *
     * @param intent The incoming intent.
     * @return The origin string. Returns null if there was no relevant tag extra in the given
     * intent, or if a relevant notification tag value did not match the expected format.
     */
    private static @Nullable String getOriginFromIntent(Intent intent) {
        String originFromChannelId =
                getOriginFromChannelId(intent.getStringExtra(Notification.EXTRA_CHANNEL_ID));
        return originFromChannelId != null
                ? originFromChannelId
                : getOriginFromNotificationTag(
                        intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_TAG));
    }

    /**
     * Gets origin from the notification tag.
     * If the user touched the settings cog on a flipped notification originating from this
     * class, there will be a notification tag extra in a specific format. From the tag we can
     * read the origin of the notification.
     *
     * @param tag The notification tag to extract origin from.
     * @return The origin string. Return null if there was no tag extra in the given notification
     * tag, or if the notification tag didn't match the expected format.
     */
    public static @Nullable String getOriginFromNotificationTag(@Nullable String tag) {
        if (tag == null
                || !tag.startsWith(
                        NotificationConstants.PERSISTENT_NOTIFICATION_TAG_PREFIX
                                + NotificationConstants.NOTIFICATION_TAG_SEPARATOR)) {
            return null;
        }

        // This code parses the notification id that was generated in notification_id_generator.cc
        // TODO(crbug.com/41364310): Extract this to a separate class.
        String[] parts = tag.split(NotificationConstants.NOTIFICATION_TAG_SEPARATOR);
        assert parts.length >= 3;
        try {
            URI uri = new URI(parts[1]);
            if (uri.getHost() != null) return parts[1];
        } catch (URISyntaxException e) {
            Log.e(TAG, "Expected to find a valid url in the notification tag extra.", e);
            return null;
        }
        return null;
    }

    @Nullable
    @VisibleForTesting
    static String getOriginFromChannelId(@Nullable String channelId) {
        if (channelId == null || !SiteChannelsManager.isValidSiteChannelId(channelId)) {
            return null;
        }
        return SiteChannelsManager.toSiteOrigin(channelId);
    }

    /**
     * Generates the notification defaults from vibrationPattern's size and silent.
     *
     * Use the system's default ringtone, vibration and indicator lights unless the notification
     * has been marked as being silent.
     * If a vibration pattern is set, the notification should use the provided pattern
     * rather than defaulting to the system settings.
     *
     * @param vibrationPatternLength Vibration pattern's size for the Notification.
     * @param silent Whether the default sound, vibration and lights should be suppressed.
     * @param vibrateEnabled Whether vibration is enabled in preferences.
     * @return The generated notification's default value.
     */
    @VisibleForTesting
    static int makeDefaults(int vibrationPatternLength, boolean silent, boolean vibrateEnabled) {
        assert !silent || vibrationPatternLength == 0;

        if (silent) return 0;

        int defaults = Notification.DEFAULT_ALL;
        if (vibrationPatternLength > 0 || !vibrateEnabled) {
            defaults &= ~Notification.DEFAULT_VIBRATE;
        }
        return defaults;
    }

    /**
     * Generates the vibration pattern used in Android notification.
     *
     * Android takes a long array where the first entry indicates the number of milliseconds to wait
     * prior to starting the vibration, whereas Chrome follows the syntax of the Web Vibration API.
     *
     * @param vibrationPattern Vibration pattern following the Web Vibration API syntax.
     * @return Vibration pattern following the Android syntax.
     */
    @VisibleForTesting
    static long[] makeVibrationPattern(int[] vibrationPattern) {
        long[] pattern = new long[vibrationPattern.length + 1];
        for (int i = 0; i < vibrationPattern.length; ++i) {
            pattern[i + 1] = vibrationPattern[i];
        }
        return pattern;
    }

    /**
     * Displays a notification with the given details.
     *
     * @param notificationId The id of the notification.
     * @param origin Full text of the origin, including the protocol, owning this notification.
     * @param scopeUrl The scope of the service worker registered by the site where the notification
     *     comes from.
     * @param profileId Id of the profile that showed the notification.
     * @param profile The profile that showed the notification.
     * @param title Title to be displayed in the notification.
     * @param body Message to be displayed in the notification. Will be trimmed to one line of text
     *     by the Android notification system.
     * @param image Content image to be prominently displayed when the notification is expanded.
     * @param icon Icon to be displayed in the notification. Valid Bitmap icons will be scaled to
     *     the platforms, whereas a default icon will be generated for invalid Bitmaps.
     * @param badge An image to represent the notification in the status bar. It is also displayed
     *     inside the notification.
     * @param vibrationPattern Vibration pattern following the Web Vibration syntax.
     * @param timestamp The timestamp of the event for which the notification is being shown.
     * @param renotify Whether the sound, vibration, and lights should be replayed if the
     *     notification is replacing another notification.
     * @param silent Whether the default sound, vibration and lights should be suppressed.
     * @param actions Action buttons to display alongside the notification.
     * @see <a href="https://developer.android.com/reference/android/app/Notification.html">Android
     *     Notification API</a>
     */
    @CalledByNative
    private void displayNotification(
            @JniType("std::string") final String notificationId,
            @NotificationType final int notificationType,
            @JniType("std::string") final String origin,
            @JniType("std::string") final String scopeUrl,
            @JniType("std::string") final String profileId,
            final Profile profile,
            @JniType("std::u16string") final String title,
            @JniType("std::u16string") final String body,
            @JniType("SkBitmap") final Bitmap image,
            @JniType("SkBitmap") final Bitmap icon,
            @JniType("SkBitmap") final Bitmap badge,
            @JniType("std::vector<int32_t>") final int[] vibrationPattern,
            final long timestamp,
            final boolean renotify,
            final boolean silent,
            final ActionInfo[] actions) {
        final boolean vibrateEnabled =
                UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                        .getBoolean(NOTIFICATIONS_VIBRATE_ENABLED);
        final boolean incognito = profile.isOffTheRecord();
        // TODO(peter): by-pass this check for non-Web Notification types.
        getWebApkPackage(scopeUrl)
                .then(
                        (Callback<String>)
                                (webApkPackage) ->
                                        displayNotificationInternal(
                                                new NotificationIdentifyingAttributes(
                                                        notificationId,
                                                        notificationType,
                                                        origin,
                                                        scopeUrl,
                                                        profileId,
                                                        incognito,
                                                        webApkPackage),
                                                profile,
                                                vibrateEnabled,
                                                title,
                                                body,
                                                image,
                                                icon,
                                                badge,
                                                vibrationPattern,
                                                timestamp,
                                                renotify,
                                                silent,
                                                actions));
    }

    private Promise<String> getWebApkPackage(String scopeUrl) {
        String webApkPackage =
                WebApkValidator.queryFirstWebApkPackage(
                        ContextUtils.getApplicationContext(), scopeUrl);
        if (webApkPackage == null) return Promise.fulfilled("");
        Promise<String> promise = new Promise<>();
        ChromeWebApkHost.checkChromeBacksWebApkAsync(
                webApkPackage,
                (doesBrowserBackWebApk, browserPackageName) ->
                        promise.fulfill(doesBrowserBackWebApk ? webApkPackage : ""));
        return promise;
    }

    /** Called after querying whether the browser backs the given WebAPK. */
    private void displayNotificationInternal(
            NotificationIdentifyingAttributes identifyingAttributes,
            Profile profile,
            boolean vibrateEnabled,
            String title,
            String body,
            Bitmap image,
            Bitmap icon,
            Bitmap badge,
            int[] vibrationPattern,
            long timestamp,
            boolean renotify,
            boolean silent,
            ActionInfo[] actions) {
        NotificationPlatformBridgeJni.get()
                .storeCachedWebApkPackageForNotificationId(
                        mNativeNotificationPlatformBridge,
                        NotificationPlatformBridge.this,
                        identifyingAttributes.notificationId,
                        identifyingAttributes.webApkPackage);
        // Record whether it's known whether notifications can be shown to the user at all.
        NotificationSystemStatusUtil.recordAppNotificationStatusHistogram();

        NotificationBuilderBase notificationBuilder =
                prepareNotificationBuilder(
                        identifyingAttributes,
                        vibrateEnabled,
                        title,
                        body,
                        image,
                        icon,
                        badge,
                        vibrationPattern,
                        timestamp,
                        renotify,
                        silent,
                        actions);

        notificationBuilder.setContentIntent(
                makePendingIntent(
                        identifyingAttributes,
                        NotificationConstants.ACTION_CLICK_NOTIFICATION,
                        /* actionIndex= */ -1,
                        /* mutable= */ false));

        notificationBuilder.setDeleteIntent(
                makePendingIntent(
                        identifyingAttributes,
                        NotificationConstants.ACTION_CLOSE_NOTIFICATION,
                        /* actionIndex= */ -1,
                        /* mutable= */ false));

        // Delegate notification to WebAPK.
        if (!identifyingAttributes.webApkPackage.isEmpty()) {
            WebApkServiceClient.getInstance()
                    .notifyNotification(
                            identifyingAttributes.origin,
                            identifyingAttributes.webApkPackage,
                            notificationBuilder,
                            identifyingAttributes.notificationId,
                            PLATFORM_ID);
            return;
        }

        // Delegate notification to TWA.
        Uri scopeUri = Uri.parse(identifyingAttributes.scopeUrl);
        if (getTwaClient().twaExistsForScope(scopeUri)) {
            getTwaClient()
                    .notifyNotification(
                            scopeUri,
                            identifyingAttributes.notificationId,
                            PLATFORM_ID,
                            notificationBuilder,
                            NotificationUmaTracker.getInstance());
            return;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.NOTIFICATION_ONE_TAP_UNSUBSCRIBE)
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                && identifyingAttributes.notificationType == NotificationType.WEB_PERSISTENT) {
            appendUnsubscribeButton(notificationBuilder, identifyingAttributes);
        } else {
            appendSiteSettingsButton(
                    notificationBuilder,
                    identifyingAttributes.notificationId,
                    identifyingAttributes.origin,
                    actions);
        }

        NotificationWrapper notification =
                buildNotificationWrapper(notificationBuilder, identifyingAttributes.notificationId);

        // Either display the notification right away; or, if this kind of notification is currently
        // under suspension, store the notification's resources back into the NotificationDatabase.
        // Once the suspension is over, displayNotification() will be called again.
        storeNotificationResourcesIfSuspended(identifyingAttributes, profile, notification)
                .then(
                        (suspended) -> {
                            if (suspended) {
                                return;
                            }

                            // Display notification as Chrome.
                            // Android may throw an exception on
                            // INotificationManager.enqueueNotificationWithTag,
                            // see crbug.com/1077027.
                            try {
                                mNotificationManager.notify(notification);
                                NotificationUmaTracker.getInstance()
                                        .onNotificationShown(
                                                NotificationUmaTracker.SystemNotificationType.SITES,
                                                notification.getNotification());
                            } catch (RuntimeException e) {
                                Log.e(
                                        TAG,
                                        "Failed to send notification, the IPC message might be"
                                                + " corrupted.");
                            }
                        });

        // If Chrome has no app-level notifications permission, check if an origin-level permission
        // should be revoked.
        // Notifications permission is not allowed for incognito profile.
        if (!identifyingAttributes.origin.isEmpty() && !identifyingAttributes.incognito) {
            NotificationManagerCompat manager =
                    NotificationManagerCompat.from(ContextUtils.getApplicationContext());
            PushMessagingServiceBridge.getInstance()
                    .verify(
                            identifyingAttributes.origin,
                            identifyingAttributes.profileId,
                            manager.areNotificationsEnabled());
        }
    }

    private Promise<Boolean> storeNotificationResourcesIfSuspended(
            NotificationIdentifyingAttributes identifyingAttributes,
            Profile profile,
            NotificationWrapper notification) {
        if (identifyingAttributes.notificationType != NotificationType.WEB_PERSISTENT) {
            return Promise.fulfilled(false);
        }

        if (mOriginsWithProvisionallyRevokedPermissions.contains(identifyingAttributes.origin)) {
            return Promise.fulfilled(true);
        }

        if (!UsageStatsService.isEnabled()) {
            return Promise.fulfilled(false);
        }

        // Only native calls into this here code, so the native process must be running, which is
        // important if we end up lazily constructing `UsageStatsService` here, which uses JNI.
        assert BrowserStartupController.getInstance().isFullBrowserStarted();
        return UsageStatsService.getForProfile(profile)
                .getSuspensionTracker()
                .storeNotificationResourcesIfSuspended(notification);
    }

    private NotificationBuilderBase prepareNotificationBuilder(
            NotificationIdentifyingAttributes identifyingAttributes,
            boolean vibrateEnabled,
            String title,
            String body,
            Bitmap image,
            Bitmap icon,
            Bitmap badge,
            int[] vibrationPattern,
            long timestamp,
            boolean renotify,
            boolean silent,
            ActionInfo[] actions) {
        Context context = ContextUtils.getApplicationContext();

        final boolean hasImage = image != null;
        final boolean forWebApk = !identifyingAttributes.webApkPackage.isEmpty();
        final String origin = identifyingAttributes.origin;
        NotificationBuilderBase notificationBuilder =
                new StandardNotificationBuilder(context)
                        .setTitle(title)
                        .setBody(body)
                        .setImage(image)
                        .setLargeIcon(icon)
                        .setSmallIconId(R.drawable.ic_chrome)
                        .setStatusBarIcon(badge)
                        .setSmallIconForContent(badge)
                        .setTicker(createTickerText(title, body))
                        .setTimestamp(timestamp)
                        .setRenotify(renotify)
                        .setOrigin(
                                UrlFormatter.formatUrlForSecurityDisplay(
                                        origin, SchemeDisplay.OMIT_HTTP_AND_HTTPS));

        if (shouldSetChannelId(forWebApk)) {
            // TODO(crbug.com/40544272): Channel ID should be retrieved from cache in native and
            // passed through to here with other notification parameters.
            String channelId = SiteChannelsManager.getInstance().getChannelIdForOrigin(origin);
            notificationBuilder.setChannelId(channelId);
        }

        // Work around the following bug in Android: if setTimeoutAfter() is called on a Builder,
        // then the corresponding notification shown, then cancelled, and then later another
        // notification is shown with the same ID/tag without specify any timeout, the new
        // notification will still "inherit" the original timeout. There is no way to specify "no
        // timeout" other than specifying a sufficiently long timeout instead (e.g. one week).
        //
        // TODO(crbug.com/41494406): Find a more elegant solution to this problem.
        if (mNotificationIdsWithStaleTimeouts.contains(identifyingAttributes.notificationId)) {
            notificationBuilder.setTimeoutAfter(/* ms= */ 1000 * 3600 * 24 * 7);
            mNotificationIdsWithStaleTimeouts.remove(identifyingAttributes.notificationId);
        }

        for (int actionIndex = 0; actionIndex < actions.length; actionIndex++) {
            ActionInfo action = actions[actionIndex];
            boolean mutable = (action.type == NotificationActionType.TEXT);
            PendingIntentProvider intent =
                    makePendingIntent(
                            identifyingAttributes,
                            NotificationConstants.ACTION_CLICK_NOTIFICATION,
                            actionIndex,
                            mutable);
            // Don't show action button icons when there's an image, as then action buttons go on
            // the same row as the Site Settings button, so icons wouldn't leave room for text.
            Bitmap actionIcon = hasImage ? null : action.icon;
            if (action.type == NotificationActionType.TEXT) {
                notificationBuilder.addTextAction(
                        actionIcon, action.title, intent, action.placeholder);
            } else {
                notificationBuilder.addButtonAction(actionIcon, action.title, intent);
            }
        }

        // The Android framework applies a fallback vibration pattern for the sound when the device
        // is in vibrate mode, there is no custom pattern, and the vibration default has been
        // disabled. To truly prevent vibration, provide a custom empty pattern.
        if (!vibrateEnabled) {
            vibrationPattern = EMPTY_VIBRATION_PATTERN;
        }
        notificationBuilder.setDefaults(
                makeDefaults(vibrationPattern.length, silent, vibrateEnabled));
        notificationBuilder.setVibrate(makeVibrationPattern(vibrationPattern));
        notificationBuilder.setSilent(silent);

        return notificationBuilder;
    }

    /**
     * Displays a service notification informing the user that they have unsubscribed from
     * notifications from a given site.
     *
     * <p>To implement undo in simple terms, the permission will not yet actually be revoked while
     * this notification is showing. Instead, the permission is revoked when this notification is
     * OK'ed, dismissed, or times out.
     */
    private void displayProvisionallyUnsubscribedNotification(
            NotificationIdentifyingAttributes identifyingAttributes) {
        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        NotificationBuilderBase notificationBuilder =
                prepareNotificationBuilder(
                        identifyingAttributes,
                        /* vibrateEnabled= */ false,
                        res.getString(R.string.notification_provisionally_unsubscribed_title),
                        res.getString(
                                R.string.notification_provisionally_unsubscribed_body,
                                UrlFormatter.formatUrlForSecurityDisplay(
                                        identifyingAttributes.origin,
                                        SchemeDisplay.OMIT_HTTP_AND_HTTPS)),
                        /* image= */ null,
                        /* icon= */ null,
                        /* badge= */ null,
                        /* vibrationPattern= */ null,
                        /* timestamp= */ -1,
                        /* renotify= */ false,
                        /* silent= */ true,
                        /* actions= */ new ActionInfo[] {});

        if (shouldSetChannelId(/* forWebApk= */ false)) {
            String channelId =
                    SiteChannelsManager.getInstance()
                            .getChannelIdForOrigin(identifyingAttributes.origin);
            notificationBuilder.setChannelId(channelId);
        }

        // TODO(crbug.com/41494407): We are setting quite a few uncommon attributes here, consider
        // just not using NotificationBuilderBase.
        notificationBuilder.setSuppressShowingLargeIcon(true);
        notificationBuilder.setTimeoutAfter(PROVISIONAL_UNSUBSCRIBE_DURATION_MS);

        notificationBuilder.setDeleteIntent(
                makePendingIntent(
                        identifyingAttributes,
                        NotificationConstants.ACTION_COMMIT_UNSUBSCRIBE,
                        /* actionIndex= */ -1,
                        /* mutable= */ false),
                NotificationUmaTracker.ActionType.COMMIT_UNSUBSCRIBE_IMPLICIT);

        addProvisionallyUnsubscribedNotificationAction(
                notificationBuilder,
                identifyingAttributes,
                NotificationConstants.ACTION_UNDO_UNSUBSCRIBE,
                NotificationUmaTracker.ActionType.UNDO_UNSUBSCRIBE,
                res.getString(R.string.notification_undo_unsubscribe_button));

        addProvisionallyUnsubscribedNotificationAction(
                notificationBuilder,
                identifyingAttributes,
                NotificationConstants.ACTION_COMMIT_UNSUBSCRIBE,
                NotificationUmaTracker.ActionType.COMMIT_UNSUBSCRIBE_EXPLICIT,
                res.getString(R.string.notification_commit_unsubscribe_button));

        NotificationWrapper notification =
                buildNotificationWrapper(notificationBuilder, identifyingAttributes.notificationId);
        mNotificationManager.notify(notification);
    }

    private void appendSiteSettingsButton(
            NotificationBuilderBase notificationBuilder,
            String notificationId,
            String origin,
            ActionInfo[] actions) {
        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        // TODO(peter): Generalize the NotificationPlatformBridge sufficiently to not need
        // to care about the individual notification types.
        // Set up a pending intent for going to the settings screen for |origin|.
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent settingsIntent =
                settingsLauncher.createSettingsActivityIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(origin));
        settingsIntent.setData(makeIntentData(notificationId, origin, /* actionIndex= */ -1));
        PendingIntentProvider settingsIntentProvider =
                PendingIntentProvider.getActivity(
                        context,
                        PENDING_INTENT_REQUEST_CODE,
                        settingsIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT);

        // If action buttons are displayed, there isn't room for the full Site Settings button
        // label and icon, so abbreviate it. This has the unfortunate side-effect of
        // unnecessarily abbreviating it on Android Wear also (crbug.com/576656). If custom
        // layouts are enabled, the label and icon provided here only affect Android Wear, so
        // don't abbreviate them.
        boolean abbreviateSiteSettings = actions.length > 0;
        int settingsIconId = abbreviateSiteSettings ? 0 : R.drawable.settings_cog;
        CharSequence settingsTitle =
                abbreviateSiteSettings
                        ? res.getString(R.string.notification_site_settings_button)
                        : res.getString(R.string.page_info_site_settings_button);
        // If the settings button is displayed together with the other buttons it has to be the
        // last one, so add it after the other actions.
        notificationBuilder.addSettingsAction(
                settingsIconId,
                settingsTitle,
                settingsIntentProvider,
                NotificationUmaTracker.ActionType.SETTINGS);
    }

    private void appendUnsubscribeButton(
            NotificationBuilderBase notificationBuilder,
            NotificationIdentifyingAttributes identifyingAttributes) {
        PendingIntentProvider unsubscribeIntentProvider =
                makePendingIntent(
                        identifyingAttributes,
                        NotificationConstants.ACTION_PRE_UNSUBSCRIBE,
                        /* actionIndex= */ -1,
                        false);

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        // TODO(crbug.com/41492613): Double check if this icon is actually used on any Android
        // versions and/or flavors.
        notificationBuilder.addSettingsAction(
                /* iconId= */ 0,
                res.getString(R.string.notification_unsubscribe_button),
                unsubscribeIntentProvider,
                NotificationUmaTracker.ActionType.PRE_UNSUBSCRIBE);
    }

    private void addProvisionallyUnsubscribedNotificationAction(
            NotificationBuilderBase notificationBuilder,
            NotificationIdentifyingAttributes identifyingAttributes,
            String action,
            @NotificationUmaTracker.ActionType int umaActionType,
            CharSequence actionLabel) {
        PendingIntentProvider intentProvider =
                makePendingIntent(identifyingAttributes, action, /* actionIndex= */ -1, false);
        notificationBuilder.addSettingsAction(
                /* iconId= */ 0, actionLabel, intentProvider, umaActionType);
    }

    private NotificationWrapper buildNotificationWrapper(
            NotificationBuilderBase notificationBuilder, String notificationId) {
        return notificationBuilder.build(
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        /* notificationTag= */ notificationId,
                        /* notificationId= */ PLATFORM_ID));
    }

    /** Returns whether to set a channel id when building a notification. */
    private boolean shouldSetChannelId(boolean forWebApk) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !forWebApk;
    }

    /**
     * Creates the ticker text for a notification having |title| and |body|. The notification's
     * title will be printed in bold, followed by the text of the body.
     *
     * @param title Title of the notification.
     * @param body Textual contents of the notification.
     * @return A character sequence containing the ticker's text.
     */
    private CharSequence createTickerText(String title, String body) {
        SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();

        spannableStringBuilder.append(title);
        spannableStringBuilder.append("\n");
        spannableStringBuilder.append(body);

        // Mark the title of the notification as being bold.
        spannableStringBuilder.setSpan(
                new StyleSpan(android.graphics.Typeface.BOLD),
                0,
                title.length(),
                Spannable.SPAN_INCLUSIVE_INCLUSIVE);

        return spannableStringBuilder;
    }

    /**
     * Returns whether a notification has been clicked in the last 5 seconds.
     * Used for Startup.BringToForegroundReason UMA histogram.
     */
    public static boolean wasNotificationRecentlyClicked() {
        if (sInstance == null) return false;
        long now = System.currentTimeMillis();
        return now - sInstance.mLastNotificationClickMs < 5 * 1000;
    }

    /**
     * Closes the notification associated with the given parameters.
     *
     * @param notificationId The id of the notification.
     * @param scopeUrl The scope of the service worker registered by the site where the notification
     *                 comes from.
     * @param hasQueriedWebApkPackage Whether has done the query of is there a WebAPK can handle
     *                                this notification.
     * @param webApkPackage The package of the WebAPK associated with the notification.
     *                      Empty if the notification is not associated with a WebAPK.
     */
    @CalledByNative
    private void closeNotification(
            final String notificationId,
            String scopeUrl,
            boolean hasQueriedWebApkPackage,
            String webApkPackage) {
        if (!hasQueriedWebApkPackage) {
            final String webApkPackageFound =
                    WebApkValidator.queryFirstWebApkPackage(
                            ContextUtils.getApplicationContext(), scopeUrl);
            if (webApkPackageFound != null) {
                WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback callback =
                        new WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback() {
                            @Override
                            public void onChecked(
                                    boolean doesBrowserBackWebApk, String backingBrowser) {
                                closeNotificationInternal(
                                        notificationId,
                                        doesBrowserBackWebApk ? webApkPackageFound : null,
                                        scopeUrl);
                            }
                        };
                ChromeWebApkHost.checkChromeBacksWebApkAsync(webApkPackageFound, callback);
                return;
            }
        }
        closeNotificationInternal(notificationId, webApkPackage, scopeUrl);
    }

    /** Called after querying whether the browser backs the given WebAPK. */
    private void closeNotificationInternal(
            String notificationId, String webApkPackage, String scopeUrl) {
        if (!TextUtils.isEmpty(webApkPackage)) {
            WebApkServiceClient.getInstance()
                    .cancelNotification(webApkPackage, notificationId, PLATFORM_ID);
            return;
        }

        if (getTwaClient().twaExistsForScope(Uri.parse(scopeUrl))) {
            getTwaClient().cancelNotification(Uri.parse(scopeUrl), notificationId, PLATFORM_ID);

            // There's an edge case where a notification was displayed by Chrome, a Trusted Web
            // Activity is then installed and run then the notification is cancelled by javascript.
            // Chrome will attempt to close the notification through the TWA client and not itself.
            // Since NotificationManager#cancel is safe to call if the requested notification
            // isn't being shown, we just call that as well to ensure notifications are cleared.
        }

        // The "provisionally unsubscribed" service notification re-uses the tag of the organic
        // notification it has replaced. Do not let this service notification be canceled. The
        // organic notification is at this point already deleted from the NotificationDatabase in
        // response to it being closed by the developer. If the user clicks `UNDO_UNSUBSCRIBE`, we
        // will not restore the cancelled notification.
        String origin = getOriginFromNotificationTag(notificationId);
        if (origin != null && mOriginsWithProvisionallyRevokedPermissions.contains(origin)) {
            return;
        }

        mNotificationManager.cancel(notificationId, PLATFORM_ID);
    }

    /**
     * Calls NotificationPlatformBridgeAndroid::OnNotificationClicked in native code to indicate
     * that the notification with the given parameters has been clicked on.
     *
     * @param identifyingAttributes Common attributes identifying a notification and its source.
     * @param actionIndex The index of the action button that was clicked, or -1 if not applicable.
     * @param reply User reply to a text action on the notification. Null if the user did not click
     *     on a text action or if inline replies are not supported.
     */
    private void onNotificationClicked(
            NotificationIdentifyingAttributes identifyingAttributes,
            int actionIndex,
            @Nullable String reply) {
        // After the user taps the `PRE_UNSUBSCRIBE` action on a notification, we need to complete
        // native startup before we can replace the tapped notification with the "provisionally
        // unsubscribed" service notification. In this time window, the user might have tapped the
        // content or a developer-provided action button. Given the strong indication the user may
        // want to stop getting these notifications, resolve this conflict by silently discarding
        // the action.
        if (identifyingAttributes.origin != null
                && mOriginsWithProvisionallyRevokedPermissions.contains(
                        identifyingAttributes.origin)) {
            return;
        }

        mLastNotificationClickMs = System.currentTimeMillis();
        NotificationPlatformBridgeJni.get()
                .onNotificationClicked(
                        mNativeNotificationPlatformBridge,
                        NotificationPlatformBridge.this,
                        identifyingAttributes.notificationId,
                        identifyingAttributes.notificationType,
                        identifyingAttributes.origin,
                        identifyingAttributes.scopeUrl,
                        identifyingAttributes.profileId,
                        identifyingAttributes.incognito,
                        identifyingAttributes.webApkPackage,
                        actionIndex,
                        reply);
    }

    /**
     * Calls NotificationPlatformBridgeAndroid::OnNotificationClosed in native code to indicate that
     * the notification with the given parameters has been closed.
     *
     * @param identifyingAttributes Common attributes identifying a notification and its source.
     * @param byUser Whether the notification was closed by a user gesture.
     */
    private void onNotificationClosed(
            NotificationIdentifyingAttributes identifyingAttributes, boolean byUser) {
        NotificationPlatformBridgeJni.get()
                .onNotificationClosed(
                        mNativeNotificationPlatformBridge,
                        NotificationPlatformBridge.this,
                        identifyingAttributes.notificationId,
                        identifyingAttributes.notificationType,
                        identifyingAttributes.origin,
                        identifyingAttributes.profileId,
                        identifyingAttributes.incognito,
                        byUser);
    }

    /**
     * Called when the user clicks the `ACTION_PRE_UNSUBSCRIBE` button.
     *
     * <p>Replaces the clicked notification with a "provisionally unsubscribed" service
     * notification. While that is showing, all new notifications from the origin are suspended, but
     * the permission is only revoked once it is dismissed/okay'ed/timed out.
     *
     * @param identifyingAttributes Common attributes identifying a notification and its source.
     */
    private void onNotificationPreUnsubcribe(
            NotificationIdentifyingAttributes identifyingAttributes) {
        // The user might tap on the PRE_UNSUBSCRIBE action multiple times in case we need to do a
        // native startup and it takes long. Record how often this happens and ignore duplicate
        // unsubscribe actions.
        boolean duplicatePreUnsubscribe =
                mOriginsWithProvisionallyRevokedPermissions.contains(identifyingAttributes.origin);
        NotificationUmaTracker.getInstance()
                .recordIsDuplicatePreUnsubscribe(duplicatePreUnsubscribe);
        if (duplicatePreUnsubscribe) {
            return;
        }

        // TODO(crbug.com/41494401): Verify if we can/need to use the correct profile here.
        NotificationSuspender suspender =
                new NotificationSuspender(
                        ProfileManager.getLastUsedRegularProfile(),
                        ContextUtils.getApplicationContext(),
                        mNotificationManager);
        mOriginsWithProvisionallyRevokedPermissions.add(identifyingAttributes.origin);
        suspender.storeNotificationResourcesFromOrigins(
                Collections.singletonList(Uri.parse(identifyingAttributes.origin)),
                (notificationIdsToCancel) -> {
                    NotificationUmaTracker.getInstance()
                            .recordSuspendedNotificationCountOnUnsubscribe(
                                    notificationIdsToCancel.size());

                    displayProvisionallyUnsubscribedNotification(identifyingAttributes);
                    notificationIdsToCancel.remove(identifyingAttributes.notificationId);
                    suspender.cancelNotificationsWithIds(notificationIdsToCancel);
                });
    }

    /**
     * Called when the user clicks the `ACTION_UNDO_UNSUBSCRIBE` button on the "provisionally
     * unsubscribed" service notification.
     *
     * <p>Restores the clicked notification and all other notifications from that origin by
     * unsuspending them.
     *
     * @param identifyingAttributes Common attributes identifying a notification and its source.
     */
    private void onNotificationUndoUnsubcribe(
            NotificationIdentifyingAttributes identifyingAttributes) {
        // Manually canceling the "provisionally unsubscribed" notification is needed in case the
        // website closed the web notification that we replaced with it.
        mNotificationManager.cancel(identifyingAttributes.notificationId, PLATFORM_ID);
        mOriginsWithProvisionallyRevokedPermissions.remove(identifyingAttributes.origin);
        mNotificationIdsWithStaleTimeouts.add(identifyingAttributes.notificationId);

        // TODO(crbug.com/41494401): Verify if we can/need to use the correct profile here.
        NotificationSuspender suspender =
                new NotificationSuspender(ProfileManager.getLastUsedRegularProfile());
        suspender.unsuspendNotificationsFromOrigins(
                Collections.singletonList(Uri.parse(identifyingAttributes.origin)));
    }

    /**
     * Called when the user clicks the `ACTION_COMMIT_UNSUBSCRIBE` button, expressly dismisses the
     * "provisionally unsubscribed" service notification, or if the service notification times out.
     *
     * <p>Handles "unsubscribing", which in practice means resetting the permission for the origin,
     * which will delete the notification channel, issue an FCM unsubscribe request, and cancel all
     * notification, including the "Provisionally unsubscribed" service notification.
     *
     * @param identifyingAttributes Common attributes identifying a notification and its source.
     */
    private void onNotificationCommitUnsubscribe(
            NotificationIdentifyingAttributes identifyingAttributes) {
        NotificationPlatformBridgeJni.get()
                .onNotificationDisablePermission(
                        mNativeNotificationPlatformBridge,
                        NotificationPlatformBridge.this,
                        identifyingAttributes.notificationId,
                        identifyingAttributes.notificationType,
                        identifyingAttributes.origin,
                        identifyingAttributes.profileId,
                        identifyingAttributes.incognito);
        mOriginsWithProvisionallyRevokedPermissions.remove(identifyingAttributes.origin);
    }

    private TrustedWebActivityClient getTwaClient() {
        if (mTwaClient == null) {
            mTwaClient = ChromeApplicationImpl.getComponent().resolveTrustedWebActivityClient();
        }
        return mTwaClient;
    }

    @NativeMethods
    interface Natives {
        void initializeNotificationPlatformBridge();

        void onNotificationClicked(
                long nativeNotificationPlatformBridgeAndroid,
                NotificationPlatformBridge caller,
                @JniType("std::string") String notificationId,
                @NotificationType int notificationType,
                @JniType("std::string") String origin,
                @JniType("std::string") String scopeUrl,
                @JniType("std::string") String profileId,
                boolean incognito,
                @JniType("std::string") String webApkPackage,
                int actionIndex,
                String reply);

        void onNotificationClosed(
                long nativeNotificationPlatformBridgeAndroid,
                NotificationPlatformBridge caller,
                @JniType("std::string") String notificationId,
                @NotificationType int notificationType,
                @JniType("std::string") String origin,
                @JniType("std::string") String profileId,
                boolean incognito,
                boolean byUser);

        void onNotificationDisablePermission(
                long nativeNotificationPlatformBridgeAndroid,
                NotificationPlatformBridge caller,
                @JniType("std::string") String notificationId,
                @NotificationType int notificationType,
                @JniType("std::string") String origin,
                @JniType("std::string") String profileId,
                boolean incognito);

        void storeCachedWebApkPackageForNotificationId(
                long nativeNotificationPlatformBridgeAndroid,
                NotificationPlatformBridge caller,
                @JniType("std::string") String notificationId,
                @JniType("std::string") String webApkPackage);
    }
}
