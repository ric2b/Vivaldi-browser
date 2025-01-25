// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.page.PageStation;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends PageStation {
    public ViewSpec ICON = ViewSpec.viewSpec(withId(R.id.new_tab_incognito_icon));

    public ViewSpec GONE_INCOGNITO_TEXT = ViewSpec.viewSpec(withText("You’ve gone Incognito"));

    protected <T extends IncognitoNewTabPageStation> IncognitoNewTabPageStation(
            Builder<T> builder) {
        super(builder.withIncognito(true));
    }

    public static Builder<IncognitoNewTabPageStation> newBuilder() {
        return new Builder<>(IncognitoNewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        elements.declareView(ICON);
        elements.declareView(GONE_INCOGNITO_TEXT);
        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedSupplier));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public IncognitoNewTabPageAppMenuFacility openAppMenu() {
        return enterFacilitySync(new IncognitoNewTabPageAppMenuFacility(), MENU_BUTTON::click);
    }
}
