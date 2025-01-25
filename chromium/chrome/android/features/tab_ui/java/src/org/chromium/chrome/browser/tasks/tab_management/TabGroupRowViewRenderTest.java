// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.core.util.Pair;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.time.Clock;

/** Render tests for {@link TabGroupRowView}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabGroupRowViewRenderTest {

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(1)
                    .build();

    private Activity mActivity;
    private TabGroupRowView mTabGroupRowView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(this::setUpOnUi);
    }

    private void setUpOnUi() {
        mTabGroupRowView = inflateAndAttach(mActivity, R.layout.tab_group_row);
    }

    private <T extends View> T inflateAndAttach(Context context, @LayoutRes int layoutRes) {
        FrameLayout contentView = new FrameLayout(mActivity);
        contentView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mActivity.setContentView(contentView);

        LayoutInflater inflater = LayoutInflater.from(context);
        inflater.inflate(layoutRes, contentView);
        assert contentView.getChildCount() == 1;
        return (T) contentView.getChildAt(0);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithVeryLongTitle() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
                    builder.with(
                            ASYNC_FAVICON_TOP_LEFT,
                            callback -> callback.onResult(new ColorDrawable(Color.RED)));
                    builder.with(TabGroupRowProperties.COLOR_INDEX, TabGroupColorId.GREY);
                    builder.with(
                            TITLE_DATA,
                            new Pair<>(
                                    "VeryLongTitleThatGetsTruncatedOrSplitOverMultipleLines", 1));
                    builder.with(CREATION_MILLIS, Clock.systemUTC().millis());
                    mPropertyModel = builder.build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mTabGroupRowView, new TabGroupRowViewBinder());
                });
        mRenderTestRule.render(mTabGroupRowView, "long_title");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithVariousFaviconCounts() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
                    builder.with(
                            ASYNC_FAVICON_TOP_LEFT,
                            callback -> callback.onResult(new ColorDrawable(Color.RED)));
                    builder.with(
                            ASYNC_FAVICON_TOP_RIGHT,
                            callback -> callback.onResult(new ColorDrawable(Color.GREEN)));
                    builder.with(
                            ASYNC_FAVICON_BOTTOM_LEFT,
                            callback -> callback.onResult(new ColorDrawable(Color.BLUE)));
                    builder.with(PLUS_COUNT, 2);
                    builder.with(TabGroupRowProperties.COLOR_INDEX, TabGroupColorId.GREY);
                    builder.with(TITLE_DATA, new Pair<>("Title", 1));
                    builder.with(CREATION_MILLIS, Clock.systemUTC().millis());
                    mPropertyModel = builder.build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mTabGroupRowView, new TabGroupRowViewBinder());
                });
        mRenderTestRule.render(mTabGroupRowView, "five");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(
                            ASYNC_FAVICON_BOTTOM_RIGHT,
                            callback -> callback.onResult(new ColorDrawable(Color.BLACK)));
                    mPropertyModel.set(PLUS_COUNT, 0);
                });
        mRenderTestRule.render(mTabGroupRowView, "four");

        ThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(ASYNC_FAVICON_BOTTOM_RIGHT, null));
        mRenderTestRule.render(mTabGroupRowView, "three");

        ThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(ASYNC_FAVICON_BOTTOM_LEFT, null));
        mRenderTestRule.render(mTabGroupRowView, "two");

        ThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(ASYNC_FAVICON_TOP_RIGHT, null));
        mRenderTestRule.render(mTabGroupRowView, "one");
    }
}
