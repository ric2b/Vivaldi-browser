// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LoadingScreen', () => {
  let app: ReadAnythingElement;
  let emptyState: SpEmptyStateElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    app.showLoading();
    emptyState = document.querySelector<SpEmptyStateElement>('sp-empty-state')!;
  });

  test('shows spinner', () => {
    const spinner = 'throbber';
    assertStringContains(emptyState.darkImagePath, spinner);
    assertStringContains(emptyState.imagePath, spinner);
  });

  test('with flag clears read aloud state', () => {
    app.speechPlayingState = {
      isSpeechActive: true,
      isSpeechTreeInitialized: true,
      isAudioCurrentlyPlaying: true,
    };

    app.showLoading();

    assertFalse(app.speechPlayingState.isSpeechActive);
    assertFalse(app.speechPlayingState.isSpeechTreeInitialized);
    assertFalse(app.speechPlayingState.isAudioCurrentlyPlaying);
  });

  test('selection on loading screen does nothing', async () => {
    const range = new Range();
    range.setStartBefore(emptyState);
    range.setEndAfter(emptyState);
    const selection = document.getSelection();
    assertTrue(!!selection);
    selection.removeAllRanges();
    selection.addRange(range);

    await flushTasks();

    assertEquals('', document.getSelection()?.toString());
  });
});
