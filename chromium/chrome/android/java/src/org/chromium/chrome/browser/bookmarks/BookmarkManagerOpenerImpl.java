// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

// Vivaldi
import static org.vivaldi.browser.bookmarks.VivaldiBookmarkAddEditFolderActivity.INTENT_IS_ADD_MODE;
import static org.vivaldi.browser.bookmarks.VivaldiBookmarkAddEditFolderActivity.INTENT_BOOKMARK_ID;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkType;

import org.vivaldi.browser.bookmarks.VivaldiBookmarkAddEditFolderActivity;
import org.vivaldi.browser.bookmarks.VivaldiBookmarkFolderSelectActivity;
import org.vivaldi.browser.common.VivaldiBookmarkUtils;
import org.vivaldi.browser.panels.PanelUtils;
// End Vivaldi

@NullMarked
public class BookmarkManagerOpenerImpl implements BookmarkManagerOpener {

    // TODO(crbug.com/): Figure out what dependencies can be injected here.
    public BookmarkManagerOpenerImpl() {}

    @Override
    public void showBookmarkManager(
            Activity activity, Profile profile, @Nullable BookmarkId folderId) {
        ThreadUtils.assertOnUiThread();
        String url = getFirstUrlToLoad(folderId);
        boolean isIncognito = profile.isOffTheRecord();

        if (BuildConfig.IS_VIVALDI) {
            PanelUtils.showPanel(activity, url, false);
            return;
        } // End Vivaldi

        if (ChromeSharedPreferences.getInstance()
                .contains(ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL)) {
            RecordUserAction.record("MobileBookmarkManagerReopenBookmarksInSameSession");
        }

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            showBookmarkManagerOnTablet(activity, activity.getComponentName(), url, isIncognito);
        } else {
            showBookmarkManagerOnPhone(activity, url, profile);
        }
    }

    @Override
    public void startEditActivity(Context context, Profile profile, BookmarkId bookmarkId) {
        RecordUserAction.record("MobileBookmarkManagerEditBookmark");

        if (BuildConfig.IS_VIVALDI) {
            VivaldiBookmarkUtils.startEditActivity(context, bookmarkId, null, false);
            return;
        } // End Vivaldi

        Intent intent = getEditActivityIntent(context, profile, bookmarkId);
        if (context instanceof BookmarkActivity bookmarkActivity) {
            bookmarkActivity.startActivityForResult(
                    intent, BookmarkActivity.EDIT_BOOKMARK_REQUEST_CODE);
        } else {
            context.startActivity(intent);
        }
    }

    @Override
    public void startFolderPickerActivity(
            Context context, Profile profile, BookmarkId... bookmarkIds) {
        Intent intent = new Intent(context, BookmarkFolderPickerActivity.class);
        intent.putStringArrayListExtra(
                BookmarkFolderPickerActivity.INTENT_BOOKMARK_IDS,
                BookmarkUtils.bookmarkIdsToStringList(bookmarkIds));
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        context.startActivity(intent);
    }

    @Override
    public void finishActivityOnPhone(Context context) {
        if (context instanceof BookmarkActivity bookmarkActivity) {
            bookmarkActivity.finish();
        }
    }

    @Override
    public String getLastUsedUrl() {
        return ChromeSharedPreferences.getInstance()
                .readString(
                        ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, UrlConstants.BOOKMARKS_URL);
    }

    private void showBookmarkManagerOnPhone(Activity activity, String url, Profile profile) {
        Intent intent =
                new Intent(
                        activity == null ? ContextUtils.getApplicationContext() : activity,
                        BookmarkActivity.class);
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        intent.setData(Uri.parse(url));
        if (activity != null) {
            // Start from an existing activity.
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            activity.startActivity(intent);
        } else {
            // Start a new task.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(intent);
        }
    }

    private void showBookmarkManagerOnTablet(
            Context context, @Nullable ComponentName componentName, String url,
            boolean isIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, context.getApplicationContext().getPackageName());
        IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_CHROME_UI);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (ChromeFeatureList.sAndroidNativePagesInNewTab.isEnabled()
                && ChromeFeatureList.sAndroidNativePagesInNewTabBookmarksEnabled.getValue()) {
            intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, isIncognito);
        }

        if (componentName != null) {
            ActivityUtils.setNonAliasedComponentForMainBrowsingActivity(intent, componentName);
        } else {
            // If the bookmark manager is shown in a tab on a phone (rather than in a separate
            // activity) the component name may be null. Send the intent through
            // ChromeLauncherActivity instead to avoid crashing. See crbug.com/615012.
            intent.setClass(context.getApplicationContext(), ChromeLauncherActivity.class);
        }

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    // Returns the first URL to load.
    private String getFirstUrlToLoad(@Nullable BookmarkId folderId) {
        String url;
        if (folderId == null) {
            // Load most recently visited bookmark folder.
            url = getLastUsedUrl();
        } else {
            // Load a specific folder.
            url = BookmarkUiState.createFolderUrl(folderId).toString();
        }

        return TextUtils.isEmpty(url) ? UrlConstants.BOOKMARKS_URL : url;
    }

    private Intent getEditActivityIntent(Context context, Profile profile, BookmarkId bookmarkId) {
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        return intent;
    }

    // Vivaldi
    @Override
    public void startVivaldiFolderSelectActivity(Context context, BookmarkId... bookmarks) {
        assert bookmarks.length > 0;
        Intent intent = VivaldiBookmarkFolderSelectActivity.createIntent(
                context, /*createFolder=*/false, bookmarks);
        context.startActivity(intent);
    }

    @Override
    public void startVivaldiEditFolderActivity(
            Context context, Profile profile, BookmarkId idToEdit) {
        Intent intent = new Intent(context, VivaldiBookmarkAddEditFolderActivity.class);
        intent.putExtra(INTENT_IS_ADD_MODE, false);
        intent.putExtra(INTENT_BOOKMARK_ID, idToEdit.toString());
        context.startActivity(intent);
    }

    @Override
    public void closePanel(Context context) {
        if (context instanceof ChromeTabbedActivity) {
            PanelUtils.closePanel(context);
        }
    }

    @Override
    public boolean isPanelOpen(Context context) {
        if (context instanceof ChromeTabbedActivity tabbedActivity) {
            return PanelUtils.isPanelOpen(tabbedActivity);
        }
        return false;
    }

    @Override
    public void vivaldiAddToReadingList(Context context) {
        ChromeTabbedActivity activity = null;
        if (context instanceof ChromeTabbedActivity) activity = (ChromeTabbedActivity) context;
        if (activity == null) return;
        Tab currentTab = activity.getActivityTab();
        if (currentTab != null && BookmarkUtils.isReadingListSupported(currentTab.getUrl())) {
            activity.vivaldiAddToReadingList(currentTab.getUrl(), currentTab.getTitle());
        }
    }

    @Override
    public boolean getAddToReadingListButtonVisibility(Context context) {
        ChromeTabbedActivity activity = null;
        if (context instanceof ChromeTabbedActivity) activity = (ChromeTabbedActivity) context;
        if (activity == null) return false;
        Tab currentTab = activity.getActivityTab();
        if (currentTab == null) return false;
        if (!BookmarkUtils.isReadingListSupported(currentTab.getUrl())) return false;
        BookmarkModel model =
                BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
        BookmarkId existingBookmark = model.getUserBookmarkIdForTab(currentTab);
        if (existingBookmark == null) return true;
        return existingBookmark.getType() != BookmarkType.READING_LIST;
    }
    // End Vivaldi
}
