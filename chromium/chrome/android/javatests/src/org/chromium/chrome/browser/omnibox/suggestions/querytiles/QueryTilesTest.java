// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_QUERY_TILES;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

import java.util.List;

/** Tests of the Query Tiles. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class QueryTilesTest {
    private static final int QUERY_TILE_CAROUSEL_MATCH_POSITION = 1;

    public final @Rule ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;
    private @Mock AutocompleteController mController;

    private ChromeTabbedActivity mActivity;
    private AutocompleteController.OnSuggestionsReceivedListener mListener;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() throws Exception {
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mAutocompleteControllerJniMock);
        doReturn(mController).when(mAutocompleteControllerJniMock).getForProfile(any());
        doAnswer(
                        inv -> {
                            mListener = inv.getArgument(0);
                            return null;
                        })
                .when(mController)
                .addOnSuggestionsReceivedListener(any());

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mActivity = mActivityTestRule.getActivity();
        OmniboxFeatures.QUERY_TILES_SHOW_AS_CAROUSEL.setForTesting(true);

        mOmnibox = new OmniboxTestUtils(mActivity);
        setUpSuggestionsToShow();
    }

    @After
    public void tearDown() {
        mOmnibox.clearFocus();
    }

    /** Initialize a small set of suggestions with QueryTiles, and focus the Omnibox. */
    private void setUpSuggestionsToShow() {
        var search =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Search")
                        .build();

        var queryTile =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION)
                        .setGroupId(1);

        var acResult =
                AutocompleteResult.fromCache(
                        List.of(
                                search,
                                queryTile.setDisplayText("News").build(),
                                queryTile.setDisplayText("Music").build(),
                                queryTile.setDisplayText("Movies").build(),
                                queryTile.setDisplayText("Sports").build(),
                                queryTile.setDisplayText("Fun").build(),
                                search),
                        GroupsInfo.newBuilder().putGroupConfigs(1, SECTION_QUERY_TILES).build());

        mOmnibox.requestFocus();
        verify(mController).startZeroSuggest(anyString(), any(), anyInt(), anyString());
        ThreadUtils.runOnUiThreadBlocking(() -> mListener.onSuggestionsReceived(acResult, true));
        mOmnibox.checkSuggestionsShown();
    }

    @Test
    @SmallTest
    public void queryTilesCarouselIsShowing() throws Exception {
        var carousel = mOmnibox.findSuggestionWithType(OmniboxSuggestionUiType.QUERY_TILES);
        assertNotNull(carousel);
        assertTrue(carousel.view instanceof BaseCarouselSuggestionView);
        assertEquals(QUERY_TILE_CAROUSEL_MATCH_POSITION, carousel.index);
    }
}
