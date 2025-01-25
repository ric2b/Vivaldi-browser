// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrCollapseElement, CrExpandButtonElement, SettingsGetMostChromePageElement} from 'chrome://settings/lazy_load.js';
import {GetTheMostOutOfChromeUserAction} from 'chrome://settings/lazy_load.js';
import type {SettingsRoutes} from 'chrome://settings/settings.js';
import {HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, resetRouterForTesting, Router, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';


/** @fileoverview Suite of tests for get_most_chrome_page. */
suite('GetMostChromePage', function() {
  let routes: SettingsRoutes;
  let hatsBrowserProxy: TestHatsBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let testElement: SettingsGetMostChromePageElement;

  setup(function() {
    hatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(hatsBrowserProxy);

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    loadTimeData.overrideValues({showGetTheMostOutOfChromeSection: true});
    resetRouterForTesting();
    routes = Router.getInstance().getRoutes();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-get-most-chrome-page');
    Router.getInstance().navigateTo(routes.GET_MOST_CHROME);
    document.body.appendChild(testElement);
    flush();
  });

  test('Basic', async function() {
    const rows = [
      {
        id: '#first',
        action: GetTheMostOutOfChromeUserAction.FIRST_SECTION_EXPANDED,
      },
      {
        id: '#second',
        action: GetTheMostOutOfChromeUserAction.SECOND_SECTION_EXPANDED,
      },
      {
        id: '#third',
        action: GetTheMostOutOfChromeUserAction.THIRD_SECTION_EXPANDED,
      },
    ];
    for (const row of rows) {
      const crExpandButton =
          testElement.shadowRoot!.querySelector<CrExpandButtonElement>(row.id);
      assertTrue(!!crExpandButton);
      const crCollapse = crExpandButton.nextElementSibling as CrCollapseElement;
      assertTrue(!!crCollapse);

      metricsBrowserProxy.reset();

      assertFalse(crCollapse.opened);
      crExpandButton.click();

      const userAction = await metricsBrowserProxy.whenCalled('recordAction');
      assertEquals(row.action, userAction);

      await crExpandButton.updateComplete;
      assertTrue(crCollapse.opened);
    }
  });

  test('HatsSurveyRequested', async function() {
    const result =
        await hatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_GET_MOST_CHROME, result);
  });

  test('LinksAriaDescriptions', function() {
    const links = testElement.shadowRoot!.querySelectorAll('a');
    assertGT(links.length, 0, 'page should contain links');
    links.forEach(
        link => assertEquals(
            loadTimeData.getString('opensInNewTab'),
            link.getAttribute('aria-description'),
            'the link should indicate that it will be opened in a new tab'));
  });
});
