// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
// <if expr="_google_chrome">
import './internal/icons.html.js';
// </if>
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
// <if expr="_google_chrome">
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
// </if>
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import './strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import type {MojomData} from './data.js';
import type {PageHandlerInterface} from './downloads.mojom-webui.js';
import {DangerType, SafeBrowsingState, State, TailoredWarningType} from './downloads.mojom-webui.js';
import {IconLoaderImpl} from './icon_loader.js';
import {getTemplate} from './item.html.js';

export interface DownloadsItemElement {
  $: {
    'controlled-by': HTMLElement,
    'file-icon': HTMLImageElement,
    'file-link': HTMLAnchorElement,
    'referrer-url': HTMLAnchorElement,
    'url': HTMLAnchorElement,
  };
}

const DownloadsItemElementBase = I18nMixin(FocusRowMixin(PolymerElement));

/**
 * The UI pattern for displaying a download. Computed from DangerType and other
 * properties of the download and user's profile.
 */
enum DisplayType {
  NORMAL,
  DANGEROUS,
  SUSPICIOUS,
  UNVERIFIED,
  INSECURE,
  ERROR,
}

export class DownloadsItemElement extends DownloadsItemElementBase {
  static get is() {
    return 'downloads-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: Object,

      completelyOnDisk_: {
        computed: 'computeCompletelyOnDisk_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      shouldLinkFilename_: {
        computed: 'computeShouldLinkFilename_(' +
            'data.dangerType, completelyOnDisk_)',
        type: Boolean,
        value: true,
      },

      hasShowInFolderLink_: {
        computed: 'computeHasShowInFolderLink_(' +
            'data.state, data.fileExternallyRemoved, data.dangerType)',
        type: Boolean,
        value: true,
      },

      controlledBy_: {
        computed: 'computeControlledBy_(data.byExtId, data.byExtName)',
        type: String,
        value: '',
      },

      iconAriaLabel_: {
        type: String,
        computed: 'computeIconAriaLabel_(displayType_)',
      },

      isActive_: {
        computed: 'computeIsActive_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      isDangerous_: {
        computed: 'computeIsDangerous_(data.state)',
        type: Boolean,
        value: false,
      },

      isMalware_: {
        computed: 'computeIsMalware_(isDangerous_, data.dangerType)',
        type: Boolean,
        value: false,
      },

      isReviewable_: {
        computed: 'computeIsReviewable_(data.isReviewable)',
        type: Boolean,
        value: false,
      },

      isInProgress_: {
        computed: 'computeIsInProgress_(data.state)',
        type: Boolean,
        value: false,
      },

      pauseOrResumeText_: {
        computed: 'computePauseOrResumeText_(isInProgress_, data.resume)',
        type: String,
      },

      showCancel_: {
        computed: 'computeShowCancel_(data.state)',
        type: Boolean,
        value: false,
      },

      showProgress_: {
        computed: 'computeShowProgress_(showCancel_, data.percent)',
        type: Boolean,
        value: false,
      },

      showDeepScan_: {
        computed: 'computeShowDeepScan_(data.state)',
        type: Boolean,
        value: false,
      },

      showOpenAnyway_: {
        computed: 'computeShowOpenAnyway_(data.dangerType)',
        type: Boolean,
        value: false,
      },

      displayType_: {
        computed: 'computeDisplayType_(data.isInsecure, data.state,' +
            'data.dangerType, data.safeBrowsingState,' +
            'data.hasSafeBrowsingVerdict)',
        type: DisplayType,
        value: DisplayType.NORMAL,
      },

      // <if expr="_google_chrome">
      showEsbPromotion: {
        type: Boolean,
        value: false,
      },
      // </if>

      useFileIcon_: Boolean,

      showReferrerUrl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showReferrerUrl'),
      },
    };
  }

  static get observers() {
    return [
      // TODO(dbeam): this gets called way more when I observe data.byExtId
      // and data.byExtName directly. Why?
      'observeControlledBy_(controlledBy_)',
      'observeDisplayType_(displayType_, isDangerous_, data.*)',
      'restoreFocusAfterCancelIfNeeded_(data)',
    ];
  }

  data: MojomData;
  // <if expr="_google_chrome">
  showEsbPromotion: boolean;
  // </if>
  private mojoHandler_: PageHandlerInterface|null = null;
  private controlledBy_: string;
  private iconAriaLabel_: string;
  private isActive_: boolean;
  private isDangerous_: boolean;
  private isReviewable_: boolean;
  private isInProgress_: boolean;
  private pauseOrResumeText_: string;
  private showCancel_: boolean;
  private showProgress_: boolean;
  private showDeepScan_: boolean;
  private showOpenAnyway_: boolean;
  private useFileIcon_: boolean;
  private restoreFocusAfterCancel_: boolean = false;
  private displayType_: DisplayType;
  private completelyOnDisk_: boolean;
  override overrideCustomEquivalent: boolean;

  constructor() {
    super();

    /** Used by FocusRowMixin. */
    this.overrideCustomEquivalent = true;
  }

  override ready() {
    super.ready();

    this.setAttribute('role', 'row');
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
  }

  /** Overrides FocusRowMixin. */
  override getCustomEquivalent(sampleElement: HTMLElement): HTMLElement|null {
    if (sampleElement.getAttribute('focus-type') === 'cancel') {
      return this.shadowRoot!.querySelector('[focus-type="retry"]');
    }
    if (sampleElement.getAttribute('focus-type') === 'retry') {
      return this.shadowRoot!.querySelector('[focus-type="pauseOrResume"]');
    }
    return null;
  }

  getFileIcon(): HTMLImageElement {
    return this.$['file-icon'];
  }

  getMoreActionsButton(): CrIconButtonElement|null {
    const button =
        this.shadowRoot!.querySelector<CrIconButtonElement>('#more-actions');
    return button || null;
  }

  getMoreActionsMenu(): CrActionMenuElement {
    const menu = this.shadowRoot!.querySelector<CrActionMenuElement>(
        '#more-actions-menu');
    assert(!!menu);
    return menu;
  }

  /**
   * @return A JS string of the display URL.
   */
  private getDisplayUrlStr_(displayUrl: String16): string {
    return mojoString16ToString(displayUrl);
  }

  private computeClass_(): string {
    const classes = [];

    if (this.isActive_) {
      classes.push('is-active');
    }

    if (this.isDangerous_) {
      classes.push('dangerous');
    }

    if (this.showProgress_) {
      classes.push('show-progress');
    }

    return classes.join(' ');
  }

  private computeCompletelyOnDisk_(): boolean {
    if (this.data.fileExternallyRemoved) {
      return false;
    }
    switch (this.data.state) {
      case State.kComplete:
        return true;
      case State.kInProgress:
      case State.kCancelled:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeShouldLinkFilename_(): boolean {
    if (this.data === undefined) {
      return false;
    }

    if (!this.completelyOnDisk_) {
      return false;
    }

    switch (this.data.dangerType) {
      case DangerType.kDeepScannedFailed:
        return false;
      case DangerType.kNoApplicableDangerType:
      case DangerType.kDangerousFile:
      case DangerType.kDangerousUrl:
      case DangerType.kDangerousContent:
      case DangerType.kCookieTheft:
      case DangerType.kUncommonContent:
      case DangerType.kDangerousHost:
      case DangerType.kPotentiallyUnwanted:
      case DangerType.kAsyncScanning:
      case DangerType.kAsyncLocalPasswordScanning:
      case DangerType.kBlockedPasswordProtected:
      case DangerType.kBlockedTooLarge:
      case DangerType.kSensitiveContentWarning:
      case DangerType.kSensitiveContentBlock:
      case DangerType.kDeepScannedSafe:
      case DangerType.kDeepScannedOpenedDangerous:
      case DangerType.kBlockedScanFailed:
        return true;
      default:
        assertNotReached('Unhandled DangerType encountered');
    }
  }

  private computeHasShowInFolderLink_(): boolean {
    if (this.data === undefined) {
      return false;
    }

    if (!this.computeCompletelyOnDisk_()) {
      return false;
    }

    switch (this.data.dangerType) {
      case DangerType.kDeepScannedFailed:
        return false;
      case DangerType.kNoApplicableDangerType:
      case DangerType.kDangerousFile:
      case DangerType.kDangerousUrl:
      case DangerType.kDangerousContent:
      case DangerType.kCookieTheft:
      case DangerType.kUncommonContent:
      case DangerType.kDangerousHost:
      case DangerType.kPotentiallyUnwanted:
      case DangerType.kAsyncScanning:
      case DangerType.kAsyncLocalPasswordScanning:
      case DangerType.kBlockedPasswordProtected:
      case DangerType.kBlockedTooLarge:
      case DangerType.kSensitiveContentWarning:
      case DangerType.kSensitiveContentBlock:
      case DangerType.kDeepScannedSafe:
      case DangerType.kDeepScannedOpenedDangerous:
      case DangerType.kBlockedScanFailed:
        return true;
      default:
        assertNotReached('Unhandled DangerType encountered');
    }
  }

  private computeControlledBy_(): string {
    if (!this.data.byExtId || !this.data.byExtName) {
      return '';
    }

    const url = `chrome://extensions/?id=${this.data.byExtId}`;
    const name = this.data.byExtName;
    return loadTimeData.getStringF('controlledByUrl', url, htmlEscape(name));
  }

  private computeDate_(): string {
    assert(typeof this.data.hideDate === 'boolean');
    if (this.data.hideDate) {
      return '';
    }
    return this.data.sinceString || this.data.dateString;
  }

  private computeDescriptionVisible_(): boolean {
    return this.computeDescription_() !== '';
  }

  private computeSecondLineVisible_(): boolean {
    if (!this.data) {
      return false;
    }
    switch (this.data.state) {
      case State.kAsyncScanning:
        return true;
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private isSuspiciousEnterpriseApVerdict_(
      requestsApVerdicts: boolean, dangerType: DangerType): boolean {
    switch (dangerType) {
      case DangerType.kUncommonContent:
        return requestsApVerdicts;
      case DangerType.kSensitiveContentWarning:
        return true;
      case DangerType.kNoApplicableDangerType:
      case DangerType.kDangerousFile:
      case DangerType.kDangerousUrl:
      case DangerType.kDangerousContent:
      case DangerType.kCookieTheft:
      case DangerType.kDangerousHost:
      case DangerType.kPotentiallyUnwanted:
      case DangerType.kAsyncScanning:
      case DangerType.kAsyncLocalPasswordScanning:
      case DangerType.kBlockedPasswordProtected:
      case DangerType.kBlockedTooLarge:
      case DangerType.kSensitiveContentBlock:
      case DangerType.kDeepScannedFailed:
      case DangerType.kDeepScannedSafe:
      case DangerType.kDeepScannedOpenedDangerous:
      case DangerType.kBlockedScanFailed:
        return false;
      default:
        assertNotReached('Unhandled DangerType encountered');
    }
  }

  private computeDisplayType_(): DisplayType {
    // Most downloads are normal. If we don't have data, don't assume danger.
    if (!this.data) {
      return DisplayType.NORMAL;
    }

    if (this.data.isInsecure) {
      return DisplayType.INSECURE;
    }

    switch (this.data.state) {
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return DisplayType.SUSPICIOUS;
      case State.kInsecure:
        return DisplayType.INSECURE;
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
        break;
      default:
        assertNotReached('Unhandled State encountered');
    }

    // Enterprise AP verdicts.
    if (this.isSuspiciousEnterpriseApVerdict_(
            loadTimeData.getBoolean('requestsApVerdicts'),
            this.data.dangerType)) {
      return DisplayType.SUSPICIOUS;
    }

    switch (this.data.dangerType) {
      // Mimics logic in download_ui_model.cc for downloads with danger_type
      // DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE.
      case DangerType.kDangerousFile:
        return this.data.hasSafeBrowsingVerdict ? DisplayType.SUSPICIOUS :
                                                  DisplayType.UNVERIFIED;
      case DangerType.kDangerousUrl:
      case DangerType.kDangerousContent:
      case DangerType.kCookieTheft:
      case DangerType.kDangerousHost:
      case DangerType.kPotentiallyUnwanted:
      case DangerType.kDeepScannedOpenedDangerous:
        return DisplayType.DANGEROUS;
      case DangerType.kUncommonContent:
      case DangerType.kDeepScannedFailed:
        return DisplayType.SUSPICIOUS;
      case DangerType.kNoApplicableDangerType:
      case DangerType.kAsyncScanning:
      case DangerType.kAsyncLocalPasswordScanning:
      case DangerType.kSensitiveContentWarning:
      case DangerType.kDeepScannedSafe:
      case DangerType.kBlockedScanFailed:
        return DisplayType.NORMAL;
      case DangerType.kBlockedPasswordProtected:
      case DangerType.kBlockedTooLarge:
      case DangerType.kSensitiveContentBlock:
        return DisplayType.ERROR;
      default:
        assertNotReached('Unhandled DangerType encountered');
    }
  }

  private computeDeepScanControlText_(): string {
    switch (this.data.state) {
      case State.kPromptForScanning:
        return loadTimeData.getString('controlDeepScan');
      case State.kPromptForLocalPasswordScanning:
        return loadTimeData.getString('controlLocalPasswordScan');
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kAsyncScanning:
        return '';
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeSaveDangerousLabel_(): string {
    switch (this.displayType_) {
      case DisplayType.DANGEROUS:
        return this.i18n('controlKeepDangerous');
      case DisplayType.SUSPICIOUS:
        return this.i18n('controlKeepSuspicious');
      case DisplayType.UNVERIFIED:
        return this.i18n('controlKeepUnverified');
      case DisplayType.INSECURE:
        return this.i18n('controlKeepInsecure');
      case DisplayType.NORMAL:
      case DisplayType.ERROR:
        return '';
      default:
        assertNotReached('Unhandled DisplayType encountered');
    }
  }

  private computeDescription_(): string {
    if (!this.data) {
      return '';
    }

    const data = this.data;

    switch (data.state) {
      case State.kComplete:
        switch (data.dangerType) {
          case DangerType.kDeepScannedOpenedDangerous:
            return loadTimeData.getString('deepScannedOpenedDangerousDesc');
          case DangerType.kDeepScannedFailed:
            return loadTimeData.getString('deepScannedFailedDesc');
          case DangerType.kNoApplicableDangerType:
          case DangerType.kDangerousFile:
          case DangerType.kDangerousUrl:
          case DangerType.kDangerousContent:
          case DangerType.kCookieTheft:
          case DangerType.kUncommonContent:
          case DangerType.kDangerousHost:
          case DangerType.kPotentiallyUnwanted:
          case DangerType.kAsyncScanning:
          case DangerType.kAsyncLocalPasswordScanning:
          case DangerType.kBlockedPasswordProtected:
          case DangerType.kBlockedTooLarge:
          case DangerType.kSensitiveContentWarning:
          case DangerType.kSensitiveContentBlock:
          case DangerType.kDeepScannedSafe:
          case DangerType.kBlockedScanFailed:
            return '';
          default:
            assertNotReached('Unhandled DangerType encountered');
        }
      case State.kInsecure:
        return loadTimeData.getString('insecureDownloadDesc');
      case State.kDangerous:
        switch (data.dangerType) {
          case DangerType.kNoApplicableDangerType:
            return '';
          case DangerType.kDangerousFile:
            return data.safeBrowsingState ===
                    SafeBrowsingState.kNoSafeBrowsing ?
                loadTimeData.getString('noSafeBrowsingDesc') :
                loadTimeData.getString('dangerFileDesc');
          case DangerType.kDangerousUrl:
          case DangerType.kDangerousContent:
          case DangerType.kDangerousHost:
            return loadTimeData.getString('dangerDownloadDesc');
          case DangerType.kCookieTheft:
            switch (data.tailoredWarningType) {
              case TailoredWarningType.kCookieTheft:
                return loadTimeData.getString('dangerDownloadCookieTheft');
              case TailoredWarningType.kCookieTheftWithAccountInfo:
                return data.accountEmail ?
                    loadTimeData.getStringF(
                        'dangerDownloadCookieTheftAndAccountDesc',
                        data.accountEmail) :
                    loadTimeData.getString('dangerDownloadCookieTheft');
              case TailoredWarningType.kSuspiciousArchive:
              case TailoredWarningType.kNoApplicableTailoredWarningType:
                return loadTimeData.getString('dangerDownloadDesc');
              default:
                assertNotReached('Unhandled TailoredWarningType encountered');
            }
          case DangerType.kUncommonContent:
            switch (data.tailoredWarningType) {
              case TailoredWarningType.kSuspiciousArchive:
                return loadTimeData.getString(
                    'dangerUncommonSuspiciousArchiveDesc');
              case TailoredWarningType.kCookieTheft:
              case TailoredWarningType.kCookieTheftWithAccountInfo:
              case TailoredWarningType.kNoApplicableTailoredWarningType:
                return loadTimeData.getString('dangerUncommonDesc');
              default:
                assertNotReached('Unhandled TailoredWarningType encountered');
            }
          case DangerType.kPotentiallyUnwanted:
            return loadTimeData.getString('dangerSettingsDesc');
          case DangerType.kAsyncScanning:
          case DangerType.kAsyncLocalPasswordScanning:
          case DangerType.kBlockedPasswordProtected:
          case DangerType.kBlockedTooLarge:
            return '';
          case DangerType.kSensitiveContentWarning:
            return loadTimeData.getString('sensitiveContentWarningDesc');
          case DangerType.kSensitiveContentBlock:
          case DangerType.kDeepScannedFailed:
          case DangerType.kDeepScannedSafe:
          case DangerType.kDeepScannedOpenedDangerous:
          case DangerType.kBlockedScanFailed:
            return '';
          default:
            assertNotReached('Unhandled DangerType encountered');
        }
      case State.kAsyncScanning:
        return loadTimeData.getString('asyncScanningDownloadDesc');
      case State.kPromptForScanning:
        return loadTimeData.getString('promptForScanningDesc');
      case State.kPromptForLocalPasswordScanning:
        return loadTimeData.getString('promptForLocalPasswordScanningDesc');
      case State.kInProgress:
      case State.kPaused:  // Fallthrough.
        return data.progressStatusText;
      case State.kInterrupted:
        switch (data.dangerType) {
          case DangerType.kNoApplicableDangerType:
          case DangerType.kDangerousFile:
          case DangerType.kDangerousUrl:
          case DangerType.kDangerousContent:
          case DangerType.kDangerousHost:
          case DangerType.kCookieTheft:
          case DangerType.kUncommonContent:
          case DangerType.kPotentiallyUnwanted:
          case DangerType.kAsyncScanning:
          case DangerType.kAsyncLocalPasswordScanning:
            return '';
          case DangerType.kBlockedPasswordProtected:
            return loadTimeData.getString('blockedPasswordProtectedDesc');
          case DangerType.kBlockedTooLarge:
            return loadTimeData.getString('blockedTooLargeDesc');
          case DangerType.kSensitiveContentWarning:
            return '';
          case DangerType.kSensitiveContentBlock:
            return loadTimeData.getString('sensitiveContentBlockedDesc');
          case DangerType.kDeepScannedFailed:
          case DangerType.kDeepScannedSafe:
          case DangerType.kDeepScannedOpenedDangerous:
          case DangerType.kBlockedScanFailed:
            return '';
          default:
            assertNotReached('Unhandled DangerType encountered');
        }
      case State.kCancelled:
        return '';
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeIconAriaHidden_(): string {
    return (this.iconAriaLabel_ === '').toString();
  }

  private computeIconAriaLabel_(): string {
    switch (this.displayType_) {
      case DisplayType.DANGEROUS:
        return this.i18n('accessibleLabelDangerous');
      case DisplayType.INSECURE:
        return this.i18n('accessibleLabelInsecure');
      case DisplayType.UNVERIFIED:
        return this.i18n('accessibleLabelUnverified');
      case DisplayType.SUSPICIOUS:
        return this.i18n('accessibleLabelSuspicious');
      case DisplayType.NORMAL:
      case DisplayType.ERROR:
        return '';
      default:
        assertNotReached('Unhandled DisplayType encountered');
    }
  }

  private iconAndDescriptionColor_(): string {
    switch (this.displayType_) {
      case DisplayType.DANGEROUS:
      case DisplayType.ERROR:
        return 'red';
      case DisplayType.INSECURE:
      case DisplayType.UNVERIFIED:
      case DisplayType.SUSPICIOUS:
        return 'grey';
      case DisplayType.NORMAL:
        return '';
      default:
        assertNotReached('Unhandled DisplayType encountered');
    }
  }

  private computeIcon_(): string {
    if (this.data) {
      switch (this.displayType_) {
        case DisplayType.DANGEROUS:
          return 'downloads:dangerous';
        case DisplayType.INSECURE:
        case DisplayType.UNVERIFIED:
        case DisplayType.SUSPICIOUS:
          return 'cr:warning';
        case DisplayType.ERROR:
          return 'cr:error';
        case DisplayType.NORMAL:
          break;
        default:
          assertNotReached('Unhandled DisplayType encountered');
      }

      assert(this.displayType_ === DisplayType.NORMAL);
      const dangerType = this.data.dangerType as DangerType;
      if (this.isSuspiciousEnterpriseApVerdict_(
              loadTimeData.getBoolean('requestsApVerdicts'), dangerType)) {
        return 'cr:warning';
      }

      switch (dangerType) {
        case DangerType.kDeepScannedFailed:
          return 'cr:info';
        case DangerType.kSensitiveContentBlock:
        case DangerType.kBlockedTooLarge:
        case DangerType.kBlockedPasswordProtected:
          return 'cr:error';
        case DangerType.kNoApplicableDangerType:
        case DangerType.kDangerousFile:
        case DangerType.kDangerousUrl:
        case DangerType.kDangerousContent:
        case DangerType.kCookieTheft:
        case DangerType.kUncommonContent:
        case DangerType.kDangerousHost:
        case DangerType.kPotentiallyUnwanted:
        case DangerType.kAsyncScanning:
        case DangerType.kAsyncLocalPasswordScanning:
        case DangerType.kSensitiveContentWarning:
        case DangerType.kDeepScannedSafe:
        case DangerType.kDeepScannedOpenedDangerous:
        case DangerType.kBlockedScanFailed:
          break;
        default:
          assertNotReached('Unhandled DangerType encountered');
      }

      switch (this.data.state) {
        case State.kAsyncScanning:
        case State.kPromptForScanning:
        case State.kPromptForLocalPasswordScanning:
          return 'cr:warning';
        case State.kInProgress:
        case State.kCancelled:
        case State.kComplete:
        case State.kPaused:
        case State.kDangerous:
        case State.kInterrupted:
        case State.kInsecure:
          break;
        default:
          assertNotReached('Unhandled State encountered');
      }
    }
    if (this.isDangerous_) {
      return 'downloads:dangerous';
    }
    if (!this.useFileIcon_) {
      return 'cr:insert-drive-file';
    }
    return '';
  }

  private computeIconColor_(): string {
    if (this.data) {
      return this.iconAndDescriptionColor_();
    }
    if (this.isDangerous_) {
      return 'red';
    }
    if (!this.useFileIcon_) {
      return 'light-grey';
    }
    return '';
  }

  private computeIsActive_(): boolean {
    if (this.data.fileExternallyRemoved) {
      return false;
    }
    switch (this.data.state) {
      case State.kCancelled:
      case State.kInterrupted:
        return false;
      case State.kInProgress:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInsecure:
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return true;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeIsDangerous_(): boolean {
    switch (this.data.state) {
      case State.kDangerous:
      case State.kInsecure:
        return true;
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kInterrupted:
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeIsInProgress_(): boolean {
    switch (this.data.state) {
      case State.kInProgress:
        return true;
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeIsMalware_(): boolean {
    if (!this.isDangerous_) {
      return false;
    }

    switch (this.data.dangerType) {
      case DangerType.kDangerousUrl:
      case DangerType.kDangerousContent:
      case DangerType.kCookieTheft:
      case DangerType.kDangerousHost:
      case DangerType.kPotentiallyUnwanted:
        return true;
      case DangerType.kNoApplicableDangerType:
      case DangerType.kDangerousFile:
      case DangerType.kUncommonContent:
      case DangerType.kAsyncScanning:
      case DangerType.kAsyncLocalPasswordScanning:
      case DangerType.kBlockedPasswordProtected:
      case DangerType.kBlockedTooLarge:
      case DangerType.kSensitiveContentWarning:
      case DangerType.kSensitiveContentBlock:
      case DangerType.kDeepScannedFailed:
      case DangerType.kDeepScannedSafe:
      case DangerType.kDeepScannedOpenedDangerous:
      case DangerType.kBlockedScanFailed:
        return false;
      default:
        assertNotReached('Unhandled DangerType encountered');
    }
  }

  private computeIsReviewable_(): boolean {
    return this.data.isReviewable;
  }

  private computePauseOrResumeText_(): string {
    if (this.data === undefined) {
      return '';
    }

    if (this.isInProgress_) {
      return loadTimeData.getString('controlPause');
    }
    if (this.data.resume) {
      return loadTimeData.getString('controlResume');
    }
    return '';
  }

  private computeShowRemove_(): boolean {
    const canDelete = loadTimeData.getBoolean('allowDeletingHistory');
    const hideRemove = this.isDangerous_ || this.showCancel_ || !canDelete;
    return !hideRemove;
  }

  private computeRemoveStyle_(): string {
    return this.computeShowRemove_() ? '' : 'visibility: hidden';
  }

  private computeShowControlsForDangerous_(): boolean {
    return !this.isReviewable_ && this.isDangerous_;
  }

  private computeShowCancel_(): boolean {
    if (!this.data) {
      return false;
    }
    switch (this.data.state) {
      case State.kInProgress:
      case State.kPaused:
        return true;
      case State.kCancelled:
      case State.kComplete:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeShowProgress_(): boolean {
    if (!this.data) {
      return false;
    }
    switch (this.data.state) {
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
        return this.showCancel_ && this.data.percent >= -1;
      case State.kAsyncScanning:
        return true;
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeShowDeepScan_(): boolean {
    switch (this.data.state) {
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return true;
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kAsyncScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private computeShowOpenAnyway_(): boolean {
    switch (this.data.dangerType) {
      case DangerType.kDeepScannedFailed:
        return true;
      case DangerType.kNoApplicableDangerType:
      case DangerType.kDangerousFile:
      case DangerType.kDangerousUrl:
      case DangerType.kDangerousContent:
      case DangerType.kCookieTheft:
      case DangerType.kUncommonContent:
      case DangerType.kDangerousHost:
      case DangerType.kPotentiallyUnwanted:
      case DangerType.kAsyncScanning:
      case DangerType.kAsyncLocalPasswordScanning:
      case DangerType.kBlockedPasswordProtected:
      case DangerType.kBlockedTooLarge:
      case DangerType.kSensitiveContentWarning:
      case DangerType.kSensitiveContentBlock:
      case DangerType.kDeepScannedSafe:
      case DangerType.kDeepScannedOpenedDangerous:
      case DangerType.kBlockedScanFailed:
        return false;
      default:
        assertNotReached('Unhandled DangerType encountered');
    }
  }

  private computeShowActionMenu_(): boolean {
    if (!this.data) {
      return false;
    }
    // If any of these actions are available, the action menu must be shown
    // because they don't have corresponding "quick actions".
    return !!this.pauseOrResumeText_ ||             // pause-or-resume
        this.computeShowControlsForDangerous_() ||  // save-dangerous
        this.data.retry ||                          // retry
        this.showDeepScan_ ||    // deep-scan and bypass-deep-scan
        this.showCancel_ ||      // cancel
        this.showOpenAnyway_ ||  // open-anyway
        this.isReviewable_;      // review-dangerous
  }

  private computeShowQuickRemove_(): boolean {
    return this.isReviewable_ || this.computeShowRemove_() ||
        this.computeShowControlsForDangerous_();
  }

  private computeShowQuickShow_(): boolean {
    // Only show the quick "show in folder" button if the full action menu
    // is hidden. If the action menu is shown, hide the quick "show in folder"
    // button to save space since this action will have an entry in the menu
    // anyway.
    return this.computeHasShowInFolderLink_() && !this.computeShowActionMenu_();
  }

  private computeTag_(): string {
    switch (this.data.state) {
      case State.kCancelled:
        return loadTimeData.getString('statusCancelled');
      case State.kInterrupted:
        return this.data.lastReasonText;
      case State.kComplete:
        return this.data.fileExternallyRemoved ?
            loadTimeData.getString('statusRemoved') :
            '';
      case State.kInProgress:
      case State.kPaused:
      case State.kDangerous:
      case State.kInsecure:
      case State.kAsyncScanning:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return '';
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private isIndeterminate_(): boolean {
    if (this.data.percent === -1) {
      return true;
    }
    switch (this.data.state) {
      case State.kAsyncScanning:
        return true;
      case State.kInProgress:
      case State.kCancelled:
      case State.kComplete:
      case State.kPaused:
      case State.kDangerous:
      case State.kInterrupted:
      case State.kInsecure:
      case State.kPromptForScanning:
      case State.kPromptForLocalPasswordScanning:
        return false;
      default:
        assertNotReached('Unhandled State encountered');
    }
  }

  private observeControlledBy_() {
    this.$['controlled-by'].innerHTML = sanitizeInnerHtml(this.controlledBy_);
    if (this.controlledBy_) {
      const link = this.shadowRoot!.querySelector('#controlled-by a');
      link!.setAttribute('focus-row-control', '');
      link!.setAttribute('focus-type', 'controlledBy');
    }
  }

  private shouldShowReferrerUrl_(): boolean {
    return loadTimeData.getBoolean('showReferrerUrl') &&
        this.data.displayReferrerUrl.data.length > 0;
  }

  getReferrerUrlAnchorElement(): HTMLAnchorElement|null {
    return this.$['referrer-url'].querySelector('a') || null;
  }

  private observeDisplayType_() {
    const removeFileUrlLinks = () => {
      this.$.url.removeAttribute('href');
      this.$['file-link'].removeAttribute('href');
    };

    const updateReferrerUrlLinkHref = (hrefValue?: string) => {
      const referrerUrlLink = this.getReferrerUrlAnchorElement();
      if (!referrerUrlLink) {
        // No <a> tag, nothing to do.
        return;
      }
      if (!hrefValue) {
        referrerUrlLink.removeAttribute('href');
        return;
      }
      referrerUrlLink.setAttribute('href', hrefValue);
      referrerUrlLink.setAttribute('focus-row-control', '');
      referrerUrlLink.setAttribute('focus-type', 'referrerUrl');
      referrerUrlLink.setAttribute('target', '_blank');
      referrerUrlLink.setAttribute('rel', 'noopener');
    };

    if (!this.data) {
      return;
    }

    // "else" case already handled by `shouldShowReferrerUrl_`.
    if (this.data.displayReferrerUrl.data.length > 0) {
      const referrerLine = loadTimeData.getStringF(
          'referrerLine', mojoString16ToString(this.data.displayReferrerUrl));
      this.$['referrer-url'].innerHTML = sanitizeInnerHtml(referrerLine);
    }

    // Returns whether to use the file icon, and additionally clears file url
    // links if necessary.
    const mayUseFileIcon = () => {
      const use = this.displayType_ === DisplayType.NORMAL;
      if (!use) {
        removeFileUrlLinks();
        updateReferrerUrlLinkHref();
      }
      return use;
    };

    this.useFileIcon_ = mayUseFileIcon();
    if (!this.useFileIcon_) {
      return;
    }

    // The file is not dangerous. Link the url if supplied.
    if (this.data.url) {
      this.$.url.href = this.data.url.url;
    } else {
      removeFileUrlLinks();
    }

    // The file is not dangerous. Link the referrer_url if supplied.
    if (this.data.referrerUrl) {
      updateReferrerUrlLinkHref(this.data.referrerUrl.url);
    } else {
      updateReferrerUrlLinkHref();
    }

    const path = this.data.filePath;
    IconLoaderImpl.getInstance()
        .loadIcon(this.$['file-icon'], path)
        .then(success => {
          if (path === this.data.filePath &&
              this.data.state !== State.kAsyncScanning) {
            // Check again if we may use the file icon, to avoid a race between
            // loading the icon and determining the proper danger type.
            this.useFileIcon_ = mayUseFileIcon() && success;
          }
        });
  }

  // <if expr="_google_chrome">
  private onEsbPromotionClick_() {
    assert(!!this.mojoHandler_);
    this.mojoHandler_.openEsbSettings();
  }
  // </if>

  private onMoreActionsClick_() {
    const button = this.getMoreActionsButton();
    // The menu button is not always shown, but if this handler is invoked, then
    // it must be.
    assert(!!button);
    this.getMoreActionsMenu().showAt(button);
  }

  // Handles the "x" remove button which can be different actions depending on
  // the state of the download.
  private onQuickRemoveClick_(e: Event) {
    if (this.isReviewable_ || this.computeShowControlsForDangerous_()) {
      this.onDiscardDangerousClick_(e);
      return;
    }
    assert(this.computeShowRemove_());
    this.onRemoveClick_(e);
  }

  private onCancelClick_() {
    this.restoreFocusAfterCancel_ = true;
    assert(!!this.mojoHandler_);
    this.mojoHandler_.cancel(this.data.id);
    getAnnouncerInstance().announce(
        loadTimeData.getString('screenreaderCanceled'));
    this.getMoreActionsMenu().close();
  }

  private onDiscardDangerousClick_(e: Event) {
    assert(!!this.mojoHandler_);
    this.mojoHandler_.discardDangerous(this.data.id);
    this.displayRemovedToast_(/*canUndo=*/ false, e);
    this.getMoreActionsMenu().close();
  }

  private onOpenNowClick_() {
    this.mojoHandler_!.openDuringScanningRequiringGesture(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private onDeepScanClick_() {
    this.mojoHandler_!.deepScan(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private onBypassDeepScanClick_() {
    this.mojoHandler_!.bypassDeepScanRequiringGesture(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private onReviewDangerousClick_() {
    this.mojoHandler_!.reviewDangerousRequiringGesture(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private onOpenAnywayClick_() {
    this.mojoHandler_!.openFileRequiringGesture(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private onDragStart_(e: Event) {
    e.preventDefault();
    this.mojoHandler_!.drag(this.data.id);
  }

  private onFileLinkClick_(e: Event) {
    e.preventDefault();
    this.mojoHandler_!.openFileRequiringGesture(this.data.id);
  }

  private onUrlClick_() {
    if (!this.data.url) {
      return;
    }
    chrome.send(
        'metricsHandler:recordAction', ['Downloads_OpenUrlOfDownloadedItem']);
  }

  private doPause_() {
    assert(!!this.mojoHandler_);
    this.mojoHandler_.pause(this.data.id);
    getAnnouncerInstance().announce(
        loadTimeData.getString('screenreaderPaused'));
  }

  private doResume_() {
    assert(!!this.mojoHandler_);
    this.mojoHandler_.resume(this.data.id);
    getAnnouncerInstance().announce(
        loadTimeData.getString('screenreaderResumed'));
  }

  private onPauseOrResumeClick_() {
    if (this.isInProgress_) {
      this.doPause_();
    } else {
      this.doResume_();
    }
    this.getMoreActionsMenu().close();
  }

  private displayRemovedToast_(canUndo: boolean, e: Event) {
    const templateStringId =
        (this.displayType_ === DisplayType.NORMAL && this.completelyOnDisk_) ?
        'toastDeletedFromHistoryStillOnDevice' :
        'toastDeletedFromHistory';
    const pieces =
        loadTimeData.getSubstitutedStringPieces(
            loadTimeData.getString(templateStringId), this.data.fileName) as
        unknown as Array<{collapsible: boolean, value: string, arg?: string}>;

    pieces.forEach(p => {
      // Make the file name collapsible.
      p.collapsible = !!p.arg;
    });
    getToastManager().showForStringPieces(pieces, /*hideSlotted=*/ !canUndo);

    // Stop propagating a click to the document to remove toast.
    e.stopPropagation();
    e.preventDefault();
  }

  private onRemoveClick_(e: Event) {
    assert(!!this.mojoHandler_);
    this.mojoHandler_.remove(this.data.id);
    const canUndo = !this.data.isDangerous && !this.data.isInsecure;
    this.displayRemovedToast_(canUndo, e);
    this.getMoreActionsMenu().close();
  }

  private onRetryClick_() {
    this.mojoHandler_!.retryDownload(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private notifySaveDangerousClick_() {
    this.dispatchEvent(new CustomEvent('save-dangerous-click', {
      bubbles: true,
      composed: true,
      detail: {id: this.data.id},
    }));
  }

  private onSaveDangerousClick_() {
    this.getMoreActionsMenu().close();

    if (this.displayType_ === DisplayType.DANGEROUS) {
      this.notifySaveDangerousClick_();
      return;
    }

    // "Suspicious" types which show up in grey can be validated directly.
    // This maps each such display type to its applicable screenreader
    // announcement string id.
    const SAVED_FROM_PAGE_TYPES_ANNOUNCEMENTS = new Map([
      [DisplayType.SUSPICIOUS, 'screenreaderSavedSuspicious'],
      [DisplayType.UNVERIFIED, 'screenreaderSavedUnverified'],
      [DisplayType.INSECURE, 'screenreaderSavedInsecure'],
    ]);
    assert(SAVED_FROM_PAGE_TYPES_ANNOUNCEMENTS.has(this.displayType_));
    assert(!!this.mojoHandler_);
    this.mojoHandler_.saveSuspiciousRequiringGesture(this.data.id);
    const announcement = loadTimeData.getString(
        SAVED_FROM_PAGE_TYPES_ANNOUNCEMENTS.get(this.displayType_) as string);
    getAnnouncerInstance().announce(announcement);
  }

  private onShowClick_() {
    this.mojoHandler_!.show(this.data.id);
    this.getMoreActionsMenu().close();
  }

  private restoreFocusAfterCancelIfNeeded_() {
    if (!this.restoreFocusAfterCancel_) {
      return;
    }
    this.restoreFocusAfterCancel_ = false;
    setTimeout(() => {
      const element = this.getFocusRow().getFirstFocusable('retry');
      if (element) {
        (element as HTMLElement).focus();
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'downloads-item': DownloadsItemElement;
  }
}

customElements.define(DownloadsItemElement.is, DownloadsItemElement);
