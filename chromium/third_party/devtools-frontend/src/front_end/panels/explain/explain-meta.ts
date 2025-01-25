// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Root from '../../core/root/root.js';
import * as Console from '../../panels/console/console.js';
import * as UI from '../../ui/legacy/legacy.js';

const UIStrings = {
  /**
   *@description Message to offer insights for a console error message
   */
  explainThisError: 'Understand this error',
  /**
   *@description Message to offer insights for a console warning message
   */
  explainThisWarning: 'Understand this warning',
  /**
   *@description Message to offer insights for a console message
   */
  explainThisMessage: 'Understand this message',
  /**
   * @description The setting title to enable the console insights feature via
   * the settings tab.
   */
  enableConsoleInsights: 'Understand console messages with AI',
  /**
   * @description Message shown to the user if the DevTools locale is not
   * supported.
   */
  wrongLocale: 'To use this feature, update your Language preference in DevTools Settings to English',
  /**
   * @description Message shown to the user if the age check is not successful.
   */
  ageRestricted: 'This feature is only available to users who are 18 years of age or older',
  /**
   * @description Message shown to the user if the user's region is not
   * supported.
   */
  geoRestricted: 'This feature is unavailable in your region',
  /**
   * @description Message shown to the user if the enterprise policy does
   * not allow this feature.
   */
  policyRestricted: 'Your organization turned off this feature. Contact your administrators for more information.',
};
const str_ = i18n.i18n.registerUIStrings('panels/explain/explain-meta.ts', UIStrings);
const i18nLazyString = i18n.i18n.getLazilyComputedLocalizedString.bind(undefined, str_);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

const setting = 'console-insights-enabled';

const actions = [
  {
    actionId: 'explain.console-message.hover',
    title: i18nLazyString(UIStrings.explainThisMessage),
    contextTypes(): [typeof Console.ConsoleViewMessage.ConsoleViewMessage] {
      return [Console.ConsoleViewMessage.ConsoleViewMessage];
    },
  },
  {
    actionId: 'explain.console-message.context.error',
    title: i18nLazyString(UIStrings.explainThisError),
    contextTypes(): [] {
      return [];
    },
  },
  {
    actionId: 'explain.console-message.context.warning',
    title: i18nLazyString(UIStrings.explainThisWarning),
    contextTypes(): [] {
      return [];
    },
  },
  {
    actionId: 'explain.console-message.context.other',
    title: i18nLazyString(UIStrings.explainThisMessage),
    contextTypes(): [] {
      return [];
    },
  },
];

function isLocaleRestricted(): boolean {
  const devtoolsLocale = i18n.DevToolsLocale.DevToolsLocale.instance();
  return !devtoolsLocale.locale.startsWith('en-');
}

function isAgeRestricted(config?: Root.Runtime.HostConfig): boolean {
  return config?.aidaAvailability?.blockedByAge === true;
}

function isGeoRestricted(config?: Root.Runtime.HostConfig): boolean {
  return config?.aidaAvailability?.blockedByGeo === true;
}

function isPolicyRestricted(config?: Root.Runtime.HostConfig): boolean {
  return config?.aidaAvailability?.blockedByEnterprisePolicy === true;
}

function isFeatureEnabled(config?: Root.Runtime.HostConfig): boolean {
  return (config?.aidaAvailability?.enabled && config?.devToolsConsoleInsights?.enabled) === true;
}

Common.Settings.registerSettingExtension({
  // TODO(crbug.com/350668580) SettingCategory.NONE once experiment GEN_AI_SETTINGS_PANEL is enabled
  category: Common.Settings.SettingCategory.CONSOLE,
  settingName: setting,
  settingType: Common.Settings.SettingType.BOOLEAN,
  title: i18nLazyString(UIStrings.enableConsoleInsights),
  defaultValue: true,
  // TODO(crbug.com/350668580) set to false once experiment GEN_AI_SETTINGS_PANEL is enabled
  reloadRequired: true,
  condition: config => isFeatureEnabled(config),
  disabledCondition: config => {
    if (isLocaleRestricted()) {
      return {disabled: true, reason: i18nString(UIStrings.wrongLocale)};
    }
    if (isAgeRestricted(config)) {
      return {disabled: true, reason: i18nString(UIStrings.ageRestricted)};
    }
    if (isGeoRestricted(config)) {
      return {disabled: true, reason: i18nString(UIStrings.geoRestricted)};
    }
    if (isPolicyRestricted(config)) {
      return {disabled: true, reason: i18nString(UIStrings.policyRestricted)};
    }
    return {disabled: false};
  },
});

function getConsoleInsightsEnabledSetting(): Common.Settings.Setting<unknown>|undefined {
  try {
    return Common.Settings.moduleSetting('console-insights-enabled');
  } catch {
    return;
  }
}

for (const action of actions) {
  UI.ActionRegistration.registerActionExtension({
    ...action,
    category: UI.ActionRegistration.ActionCategory.CONSOLE,
    async loadActionDelegate() {
      const Explain = await import('./explain.js');
      return new Explain.ActionDelegate();
    },
    condition: config => {
      if (Root.Runtime.experiments.isEnabled(Root.Runtime.ExperimentName.GEN_AI_SETTINGS_PANEL)) {
        return isFeatureEnabled(config) && !isPolicyRestricted(config);
      }
      const consoleInsightsSetting = getConsoleInsightsEnabledSetting();
      return (consoleInsightsSetting?.getIfNotDisabled() === true) && isFeatureEnabled(config) &&
          !isAgeRestricted(config) && !isGeoRestricted(config) && !isLocaleRestricted() && !isPolicyRestricted(config);
    },
  });
}
