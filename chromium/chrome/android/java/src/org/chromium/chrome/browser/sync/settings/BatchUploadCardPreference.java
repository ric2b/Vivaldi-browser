// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Activity;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.HashMap;
import java.util.Set;

public class BatchUploadCardPreference extends Preference
        implements SyncService.SyncStateChangedListener, BatchUploadDialogCoordinator.Listener {
    private Activity mActivity;
    private Profile mProfile;
    private SyncService mSyncService;
    private HashMap<Integer, LocalDataDescription> mLocalDataDescriptionsMap;
    private ModalDialogManager mDialogManager;
    private SnackbarManager mSnackbarManager;
    private ReauthenticatorBridge mReauthenticatorBridge;

    public BatchUploadCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signin_settings_card_view);
    }

    /** Initialize the dependencies for the BatchUploadCardPreference and update the error card. */
    public void initialize(Activity activity, Profile profile, ModalDialogManager dialogManager) {
        mActivity = activity;
        mProfile = profile;
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        mDialogManager = dialogManager;
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }
        mReauthenticatorBridge =
                ReauthenticatorBridge.create(
                        mActivity, mProfile, DeviceAuthSource.SETTINGS_BATCH_UPLOAD);
        update();
    }

    public void hideBatchUploadCardAndUpdate() {
        // Temporarily hide, it will become visible again once getLocalDataDescriptions() completes,
        // which is triggered from update().
        setVisible(false);
        notifyChanged();
        update();
    }

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    @Override
    public void onDetached() {
        super.onDetached();
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }
        if (mReauthenticatorBridge != null) {
            mReauthenticatorBridge.destroy();
        }
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        holder.setDividerAllowedAbove(false);
        setupBatchUploadCardView(holder.findViewById(R.id.signin_settings_card));
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        update();
    }

    @Override
    public void onSaveInAccountDialogButtonClicked(Set<Integer> types, int itemsCount) {
        if (!types.contains(ModelType.PASSWORDS)) {
            uploadLocalDataAndShowSnackbar(types, itemsCount);
            return;
        }
        // Uploading passwords requires a reauthentication.
        mReauthenticatorBridge.reauthenticate(
                success -> {
                    if (success) {
                        uploadLocalDataAndShowSnackbar(types, itemsCount);
                    }
                });
    }

    private void uploadLocalDataAndShowSnackbar(Set<Integer> types, int itemsCount) {
        SyncServiceFactory.getForProfile(mProfile).triggerLocalDataMigration(types);
        String snackbarMessage =
                getContext()
                        .getResources()
                        .getQuantityString(
                                R.plurals.account_settings_bulk_upload_saved_snackbar_message,
                                itemsCount,
                                IdentityServicesProvider.get()
                                        .getIdentityManager(mProfile)
                                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                                        .getEmail());
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                snackbarMessage,
                                /* controller= */ null,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_SETTINGS_BATCH_UPLOAD)
                        .setSingleLine(false));
        hideBatchUploadCardAndUpdate();
    }

    private void update() {
        mSyncService.getLocalDataDescriptions(
                mReauthenticatorBridge.canUseAuthenticationWithBiometricOrScreenLock()
                        ? Set.of(ModelType.BOOKMARKS, ModelType.READING_LIST, ModelType.PASSWORDS)
                        : Set.of(ModelType.BOOKMARKS, ModelType.READING_LIST),
                localDataDescriptionsMap -> {
                    mLocalDataDescriptionsMap = localDataDescriptionsMap;
                    int sum =
                            mLocalDataDescriptionsMap.values().stream()
                                    .map(LocalDataDescription::itemCount)
                                    .reduce(0, Integer::sum);
                    if (sum == 0) {
                        setVisible(false);
                    } else {
                        setVisible(true);
                    }
                    notifyChanged();
                });
    }

    private void setupBatchUploadCardView(View card) {
        if (mLocalDataDescriptionsMap == null) {
            return;
        }

        Context context = getContext();

        Button button = (Button) card.findViewById(R.id.signin_settings_card_button);
        button.setText(R.string.account_settings_bulk_upload_section_save_button);
        button.setOnClickListener(
                v -> {
                    BatchUploadDialogCoordinator.show(
                            context, mProfile, mLocalDataDescriptionsMap, mDialogManager, this);
                });

        ImageView image = (ImageView) card.findViewById(R.id.signin_settings_card_icon);
        image.setImageDrawable(
                UiUtils.getTintedDrawable(
                        getContext(),
                        R.drawable.ic_cloud_upload_24dp,
                        R.color.default_icon_color_accent1_tint_list));

        int localPasswordsCount = 0;
        LocalDataDescription passwordsLocalDataDescription =
                mLocalDataDescriptionsMap.get(ModelType.PASSWORDS);
        if (passwordsLocalDataDescription != null) {
            localPasswordsCount = passwordsLocalDataDescription.itemCount();
        }

        int localItemsCountExcludingPasswords = 0;
        LocalDataDescription bookmarksLocalDataDescription =
                mLocalDataDescriptionsMap.get(ModelType.BOOKMARKS);
        if (bookmarksLocalDataDescription != null) {
            localItemsCountExcludingPasswords += bookmarksLocalDataDescription.itemCount();
        }

        LocalDataDescription readingListLocalDataDescription =
                mLocalDataDescriptionsMap.get(ModelType.READING_LIST);
        if (readingListLocalDataDescription != null) {
            localItemsCountExcludingPasswords += readingListLocalDataDescription.itemCount();
        }

        TextView text = (TextView) card.findViewById(R.id.signin_settings_card_description);
        // TODO(b/354686035): Handle accounts with non-displayable email address.
        CoreAccountInfo accountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(mProfile)
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (localItemsCountExcludingPasswords == 0) {
            text.setText(
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .account_settings_bulk_upload_section_description_password,
                                    localPasswordsCount,
                                    localPasswordsCount,
                                    accountInfo.getEmail()));
        } else if (localPasswordsCount == 0) {
            text.setText(
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .account_settings_bulk_upload_section_description_other,
                                    localItemsCountExcludingPasswords,
                                    localItemsCountExcludingPasswords,
                                    accountInfo.getEmail()));
        } else {
            text.setText(
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .account_settings_bulk_upload_section_description_password_and_other,
                                    localPasswordsCount,
                                    localPasswordsCount,
                                    accountInfo.getEmail()));
        }
    }
}
