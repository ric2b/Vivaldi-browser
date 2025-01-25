// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGroupInfo;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab List. */
class TabListViewBinder {
    private static final int TAB_GROUP_ICON_COLOR_LEVEL = 1;

    /**
     * Main entrypoint for binding TabListView
     *
     * @param view The view to bind to.
     * @param model The model to bind.
     * @param viewType The view type to bind.
     */
    public static void bindTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        assert view instanceof ViewLookupCachingFrameLayout;
        @TabActionState Integer tabActionState = model.get(TabProperties.TAB_ACTION_STATE);
        if (tabActionState == null) {
            assert false : "TAB_ACTION_STATE must be set before initial bindTab call.";
            return;
        }

        ((TabListView) view).setTabActionState(tabActionState);
        bindListTab(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        if (tabActionState == TabActionState.CLOSABLE) {
            bindClosableListTab(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        } else if (tabActionState == TabActionState.SELECTABLE) {
            bindSelectableListTab(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        } else {
            assert false : "Unsupported TabActionState provided to bindTab.";
        }
    }

    // TODO(crbug.com/40107066): Merge with TabGridViewBinder for shared properties.
    private static void bindListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            ((TextView) view.findViewById(R.id.title)).setText(title);
        } else if (TabProperties.FAVICON_FETCHER == propertyKey) {
            final TabListFaviconProvider.TabFaviconFetcher fetcher =
                    model.get(TabProperties.FAVICON_FETCHER);
            if (fetcher == null) {
                setFavicon(view, null);
                return;
            }
            fetcher.fetch(
                    tabFavicon -> {
                        if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                        setFavicon(view, tabFavicon.getDefaultDrawable());
                    });
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            int selectedTabBackground =
                    model.get(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID);
            Resources res = view.getResources();
            Resources.Theme theme = view.getContext().getTheme();
            Drawable drawable =
                    new InsetDrawable(
                            ResourcesCompat.getDrawable(res, selectedTabBackground, theme),
                            (int) res.getDimension(R.dimen.tab_list_selected_inset_low_end));
            view.setForeground(model.get(TabProperties.IS_SELECTED) ? drawable : null);
        } else if (TabProperties.IS_INCOGNITO == propertyKey) {
            updateColors(
                    view,
                    model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED));
        } else if (TabProperties.URL_DOMAIN == propertyKey) {
            String domain = model.get(TabProperties.URL_DOMAIN);
            ((TextView) view.findViewById(R.id.description)).setText(domain);
        } else if (TabProperties.TAB_GROUP_COLOR_ID == propertyKey) {
            setTabGroupColorIcon(view, model);
        } else if (TabProperties.TAB_ACTION_BUTTON_LISTENER == propertyKey) {
            TabGridViewBinder.setNullableClickListener(
                    model.get(TabProperties.TAB_ACTION_BUTTON_LISTENER),
                    view.findViewById(R.id.end_button),
                    model);
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            TabGridViewBinder.setNullableClickListener(
                    model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            TabGridViewBinder.setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        }
    }

    /**
     * Bind a closable tab to view.
     *
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    private static void bindClosableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        if (TabProperties.IS_INCOGNITO == propertyKey) {
            ImageView closeButton = view.findViewById(R.id.end_button);
            ImageViewCompat.setImageTintList(
                    closeButton,
                    TabUiThemeProvider.getActionButtonTintList(
                            view.getContext(),
                            model.get(TabProperties.IS_INCOGNITO),
                            /* isSelected= */ false));
        } else if (TabProperties.ACTION_BUTTON_DESCRIPTION_STRING == propertyKey) {
            view.findViewById(R.id.end_button)
                    .setContentDescription(
                            model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING));
        } else if (TabProperties.TAB_GROUP_INFO == propertyKey
                || TabProperties.TAB_ID == propertyKey) {
            @Nullable TabGroupInfo tabGroupInfo = model.get(TabProperties.TAB_GROUP_INFO);
            ImageView actionButton = view.findViewById(R.id.end_button);
            Resources res = view.getResources();

            // Only change the drawable if the property key in question is for tab groups.
            if (TabProperties.TAB_GROUP_INFO == propertyKey) {
                if (tabGroupInfo.getIsTabGroup()) {
                    actionButton.setImageDrawable(
                            ResourcesCompat.getDrawable(
                                    res,
                                    R.drawable.ic_more_vert_24dp,
                                    view.getContext().getTheme()));
                } else {
                    int closeButtonSize =
                            (int) res.getDimension(R.dimen.tab_grid_close_button_size);
                    Bitmap bitmap = BitmapFactory.decodeResource(res, R.drawable.btn_close);
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true);
                    actionButton.setImageBitmap(bitmap);
                }
            }
        }
    }

    /**
     * Bind color updates.
     *
     * @param view The root view of the item (either Selectable/ClosableTabListView).
     * @param isIncognito Whether the model is in incognito mode.
     * @param isSelected Whether the item is selected.
     */
    private static void updateColors(ViewGroup view, boolean isIncognito, boolean isSelected) {
        // TODO(crbug.com/40272756): isSelected is ignored as the selected row is only outlined not
        // colored so it should use the unselected color. This will be addressed in a fixit.

        // Shared by both classes, from tab_list_card_item.
        View contentView = view.findViewById(R.id.content_view);
        contentView.getBackground().mutate();
        final @ColorInt int backgroundColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        view.getContext(), isIncognito, /* isSelected= */ false);
        ViewCompat.setBackgroundTintList(contentView, ColorStateList.valueOf(backgroundColor));

        final @ColorInt int textColor =
                TabUiThemeUtils.getTitleTextColor(
                        view.getContext(), isIncognito, /* isSelected= */ false);
        TextView titleView = view.findViewById(R.id.title);
        TextView descriptionView = view.findViewById(R.id.description);
        titleView.setTextColor(textColor);
        descriptionView.setTextColor(textColor);

        ImageView faviconView = view.findViewById(R.id.start_icon);
        if (faviconView.getBackground() == null) {
            faviconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        }
        faviconView.getBackground().mutate();
        final @ColorInt int faviconBackgroundColor =
                TabUiThemeUtils.getMiniThumbnailPlaceholderColor(
                        view.getContext(), isIncognito, /* isSelected= */ false);
        ViewCompat.setBackgroundTintList(
                faviconView, ColorStateList.valueOf(faviconBackgroundColor));
    }

    /**
     * Bind a selectable tab to view.
     *
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    private static void bindSelectableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        final int tabId = model.get(TabProperties.TAB_ID);
        final int defaultLevel = view.getResources().getInteger(R.integer.list_item_level_default);
        final int selectedLevel =
                view.getResources().getInteger(R.integer.list_item_level_selected);
        TabListView tabListView = (TabListView) view;

        if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            tabListView.setSelectionDelegate(model.get(TabProperties.TAB_SELECTION_DELEGATE));
            tabListView.setItem(tabId);
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            ImageView actionButton = view.findViewById(R.id.end_button);
            actionButton.getBackground().setLevel(isSelected ? selectedLevel : defaultLevel);
            DrawableCompat.setTintList(
                    actionButton.getBackground().mutate(),
                    isSelected
                            ? model.get(
                                    TabProperties.SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND)
                            : model.get(TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND));

            // The check should be invisible if not selected.
            actionButton.getDrawable().setAlpha(isSelected ? 255 : 0);
            ImageViewCompat.setImageTintList(
                    actionButton,
                    isSelected ? model.get(TabProperties.CHECKED_DRAWABLE_STATE_LIST) : null);
            if (isSelected) ((AnimatedVectorDrawableCompat) actionButton.getDrawable()).start();
        }
    }

    private static void setFavicon(View view, Drawable favicon) {
        ImageView faviconView = view.findViewById(R.id.start_icon);
        faviconView.setImageDrawable(favicon);
    }

    private static void setTabGroupColorIcon(ViewGroup view, PropertyModel model) {
        ImageView colorIconView = view.findViewById(R.id.icon);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            colorIconView.setVisibility(View.VISIBLE);

            // If the tab is a single tab item, a tab that is part of a group but shown in the
            // TabGridDialogView list representation, or an invalid case, do not set/show.
            if (model.get(TabProperties.TAB_GROUP_COLOR_ID)
                    == TabGroupColorUtils.INVALID_COLOR_ID) {
                colorIconView.setVisibility(View.GONE);
                return;
            }

            Context context = view.getContext();
            final @ColorInt int color =
                    ColorPickerUtils.getTabGroupColorPickerItemColor(
                            context,
                            model.get(TabProperties.TAB_GROUP_COLOR_ID),
                            model.get(TabProperties.IS_INCOGNITO));

            // If the icon already exists, just apply the color to the existing drawable.
            LayerDrawable bgDrawable = (LayerDrawable) colorIconView.getBackground();
            if (bgDrawable == null) {
                LayerDrawable tabGroupColorIcon =
                        (LayerDrawable)
                                ResourcesCompat.getDrawable(
                                        context.getResources(),
                                        R.drawable.tab_group_color_icon,
                                        context.getTheme());
                ((GradientDrawable) tabGroupColorIcon.getDrawable(TAB_GROUP_ICON_COLOR_LEVEL))
                        .setColor(color);
                colorIconView.setBackground(tabGroupColorIcon);
            } else {
                bgDrawable.mutate();
                ((GradientDrawable) bgDrawable.getDrawable(TAB_GROUP_ICON_COLOR_LEVEL))
                        .setColor(color);
            }

        } else {
            colorIconView.setVisibility(View.GONE);
        }
    }
}
