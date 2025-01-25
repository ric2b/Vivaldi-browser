// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/certificate_manager/certificate_subpage_v2.js';
import 'chrome://certificate-manager/strings.m.js';

import {CertificateSource} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import type {CertificateSubpageV2Element, SubpageCertificateList} from 'chrome://resources/cr_components/certificate_manager/certificate_subpage_v2.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestCertificateManagerProxy} from './certificate_manager_v2_test_support.js';

suite('CertificateSubpageV2Test', () => {
  let certSubpage: CertificateSubpageV2Element;
  let testProxy: TestCertificateManagerProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesV2BrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certSubpage = document.createElement('certificate-subpage-v2');

    const subpageCertLists: SubpageCertificateList[] = [
      {
        headerText: 'CRS',
        hideExport: false,
        certSource: CertificateSource.kChromeRootStore,
      },
      {
        headerText: 'EnterpriseTrusted',
        hideExport: true,
        certSource: CertificateSource.kEnterpriseTrustedCerts,
      },
    ];

    certSubpage.subpageCertLists = subpageCertLists;
    certSubpage.subpageTitle = 'Subpage Title';
    document.body.appendChild(certSubpage);
  }

  test('element check', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
          },
        ],
      };
    });

    initializeElement();

    await microtasksFinished();
    assertEquals(
        2, testProxy.handler.getCallCount('getCertificates'),
        'getCertificates called incorrect amount of times');

    const lists =
        certSubpage.shadowRoot!.querySelectorAll('certificate-list-v2');
    assertEquals(2, lists.length, 'no lists displayed');
    assertEquals('CRS', lists[0]!.headerText, 'crs list header wrong');
    assertEquals(
        CertificateSource.kChromeRootStore, lists[0]!.certSource,
        'crs list certsource wrong');
    assertFalse(lists[0]!.hideExport, 'crs hideexport value incorrect');
    assertEquals(
        'EnterpriseTrusted', lists[1]!.headerText,
        'enterprisetrusted list header wrong');
    assertEquals(
        CertificateSource.kEnterpriseTrustedCerts, lists[1]!.certSource,
        'enterprisetrusted list certsource wrong');
    assertTrue(
        lists[1]!.hideExport, 'enterprisetrusted hideexport value incorrect');
  });
});
