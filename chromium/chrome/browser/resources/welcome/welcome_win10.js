// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'welcome-win10',

  properties: {
    // Determines if the combined variant should be displayed. The combined
    // variant includes instructions on how to pin Chrome to the taskbar.
    isCombined: Boolean,

    // Indicates if the accelerated flow is enabled.
    isAccelerated: Boolean,
  },

  receivePinnedState_: function(isPinnedToTaskbar) {
    // Allow overriding of the result via a query parameter.
    // TODO(pmonette): Remove these checks when they are no longer needed.
    const VARIANT_KEY = 'variant';
    const VARIANT_TYPE_MAP = {'defaultonly': false, 'combined': true};
    var params = new URLSearchParams(location.search);
    if (params.has(VARIANT_KEY) && params.get(VARIANT_KEY) in VARIANT_TYPE_MAP)
      this.isCombined = VARIANT_TYPE_MAP[params.get(VARIANT_KEY)];
    else
      this.isCombined = !isPinnedToTaskbar;

    // Show the module.
    this.style.opacity = 1;
  },

  ready: function() {
    this.isCombined = false;
    this.isAccelerated = loadTimeData.getBoolean('accelerated_flow_enabled');

    // The accelerated flow can be overridden with a query parameter.
    const FLOWTYPE_KEY = 'flowtype';
    const FLOW_TYPE_MAP = {'regular': false, 'accelerated': true};
    var params = new URLSearchParams(location.search);
    if (params.has(FLOWTYPE_KEY)) {
      if (params.get(FLOWTYPE_KEY) in FLOW_TYPE_MAP) {
        this.isAccelerated = FLOW_TYPE_MAP[params.get(FLOWTYPE_KEY)];
      } else {
        console.log(
            'Found invalid value for the \'flowtype\' parameter: %s',
            params.get(FLOWTYPE_KEY));
      }
    }

    // The accelerated flow contains fewer steps. To account for this, toggle
    // the 'accelerated' class which reduces the height of the steps section.
    this.classList.toggle('accelerated', this.isAccelerated);

    // Asynchronously check if Chrome is pinned to the taskbar.
    cr.sendWithPromise('getPinnedToTaskbarState')
        .then(this.receivePinnedState_.bind(this));
  },

  computeClasses: function(isCombined) {
    return isCombined ? 'section expandable expanded' : 'section';
  },

  onContinue: function() {
    chrome.send('handleContinue');
  },

  onOpenSettings: function() {
    chrome.send('handleSetDefaultBrowser');
  },

  onToggle: function() {
    if (!this.isCombined)
      return;
    var sections = this.shadowRoot.querySelectorAll('.section.expandable');
    sections.forEach(function(section) {
      var isExpanded = section.classList.toggle('expanded');
      section.querySelector('[role~="button"]')
          .setAttribute('aria-expanded', isExpanded);
    });
  }
});
