/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @module */

"use strict";

/**
 * @fileOverview Handles notifications.
 */

const {Prefs} = require("prefs");

const {MILLIS_IN_MINUTE, MILLIS_IN_HOUR, MILLIS_IN_DAY} = require("./time");
const {Downloader, Downloadable} = require("./downloader");
const {compareVersions} = require("./versions");
const {analytics} = require("./analytics");

const INITIAL_DELAY = 1 * MILLIS_IN_MINUTE;
const CHECK_INTERVAL = 1 * MILLIS_IN_HOUR;
const EXPIRATION_INTERVAL = 1 * MILLIS_IN_DAY;

/**
 * The default locale for the localization of notification texts.
 * @type {string}
 * @default
 */
const DEFAULT_LOCALE = "en-US";

/**
 * Returns the numerical severity of a notification.
 * @param {object} notification The notification.
 * @returns {number} The numerical severity of the notification.
 */
function getNumericalSeverity(notification)
{
  switch (notification.type)
  {
    case "information":
      return 0;
    case "newtab":
    case "relentless":
      return 1;
    case "critical":
      return 2;
  }

  return 0;
}

/**
 * Saves notification data to preferences.
 * @param {string} [key] The key to save.
 */
function saveNotificationData(key = "notificationdata")
{
  // JSON values aren't saved unless they are assigned a different object.
  Prefs[key] = JSON.parse(JSON.stringify(Prefs[key]));
}

/**
 * Localizes a notification text.
 *
 * @param {object} translations The set of translations from which to select
 *   the localization.
 * @param {string} locale The locale string to use for the selection. If no
 *   localized version is found in the `translations` object for the given
 *   locale, the
 *   {@link module:notifications~DEFAULT_LOCALE default locale} is used.
 *
 * @returns {*} The localized text; `undefined` if no localized version is
 *   found in the `translations` object.
 */
function localize(translations, locale)
{
  if (locale in translations)
    return translations[locale];

  let languagePart = locale.substring(0, locale.indexOf("-"));
  if (languagePart && languagePart in translations)
    return translations[languagePart];

  return translations[DEFAULT_LOCALE];
}

function isValidNotification(notification)
{
  // Only notifications that are meant to be opened in a new tab must
  // contain at least one URL to open.
  if (notification.type == "newtab" &&
      !(notification.links && notification.links.length > 0))
    return false;

  return true;
}

/**
 * `{@link module:notifications.notifications}` implementation.
 */
class Notifications
{
  /**
   * @hideconstructor
   */
  constructor()
  {
    /**
     * Whether the fetching of notifications has been started.
     * @private
     */
    this._started = false;

    /**
     * Whether all ignorable notifications should be ignored.
     * @type {boolean}
     */
    this.ignored = false;

    /**
     * The locale for the localization of notification texts.
     * @type {string}
     * @default {@link module:notifications~DEFAULT_LOCALE}
     */
    this.locale = DEFAULT_LOCALE;

    /**
     * Listeners for notifications to be shown.
     * @type {Set.<function>}
     * @private
     */
    this._showListeners = new Set();

    /**
     * Local notifications added via
     * `{@link module:notifications~Notifications#addNotification
     *         addNotification()}`.
     * @type {Set.<object>}
     * @private
     */
    this._localNotifications = new Set();

    /**
     * The object providing actual downloading functionality.
     * @type {module:downloader.Downloader}
     * @private
     */
    this._downloader = new Downloader(this._getDownloadables.bind(this));

    this._downloader.onExpirationChange = this._onExpirationChange.bind(this);
    this._downloader.onDownloadSuccess = this._onDownloadSuccess.bind(this);
    this._downloader.onDownloadError = this._onDownloadError.bind(this);
  }

  /**
   * Starts fetching notifications.
   *
   * No notifications are fetched until this function has been called at least
   * once.
   */
  start()
  {
    if (this._started)
      return;

    Prefs.on("blocked_total", this._onBlockedTotal.bind(this));

    this._downloader.scheduleChecks(CHECK_INTERVAL, INITIAL_DELAY);
    this._started = true;
  }

  /**
   * Yields a `{@link module:downloader.Downloadable Downloadable}` instance
   * for the notifications download.
   * @yields {module:downloader.Downloadable}
   * @private
   */
  *_getDownloadables()
  {
    let {notificationurl: url, notificationdata} = Prefs;

    let downloadable = new Downloadable(url);

    if (analytics.isTrusted(url))
      downloadable.firstVersion = analytics.getFirstVersion();

    if (typeof notificationdata.lastError == "number")
      downloadable.lastError = notificationdata.lastError;
    if (typeof notificationdata.lastCheck == "number")
      downloadable.lastCheck = notificationdata.lastCheck;

    if (typeof notificationdata.data == "object" &&
        "version" in notificationdata.data)
      downloadable.lastVersion = notificationdata.data.version;

    if (typeof notificationdata.softExpiration == "number")
      downloadable.softExpiration = notificationdata.softExpiration;
    if (typeof notificationdata.hardExpiration == "number")
      downloadable.hardExpiration = notificationdata.hardExpiration;
    if (typeof notificationdata.downloadCount == "number")
      downloadable.downloadCount = notificationdata.downloadCount;

    yield downloadable;
  }

  _onExpirationChange(downloadable)
  {
    Prefs.notificationdata.lastCheck = downloadable.lastCheck;
    Prefs.notificationdata.softExpiration = downloadable.softExpiration;
    Prefs.notificationdata.hardExpiration = downloadable.hardExpiration;

    saveNotificationData();
  }

  _onDownloadSuccess(downloadable, responseText, errorCallback,
                     redirectCallback)
  {
    try
    {
      let data = JSON.parse(responseText);

      if (typeof data.version == "string" &&
          analytics.isTrusted(downloadable.redirectURL || downloadable.url))
        analytics.recordVersion(data.version);

      let notifications = [];
      for (let notification of data.notifications)
      {
        if (!isValidNotification(notification))
        {
          console.warn(`Invalid notification with ID "${notification.id}"`);
          continue;
        }

        if ("severity" in notification)
        {
          if (!("type" in notification))
            notification.type = notification.severity;
          delete notification.severity;
        }

        notifications.push(notification);
      }
      data.notifications = notifications;

      Prefs.notificationdata.data = data;
    }
    catch (error)
    {
      console.warn(error);
      errorCallback("synchronize_invalid_data");
      return;
    }

    Prefs.notificationdata.lastError = 0;
    Prefs.notificationdata.downloadStatus = "synchronize_ok";
    [
      Prefs.notificationdata.softExpiration,
      Prefs.notificationdata.hardExpiration
    ] = this._downloader.processExpirationInterval(EXPIRATION_INTERVAL);
    Prefs.notificationdata.downloadCount = downloadable.downloadCount;

    saveNotificationData();

    this.showNext();
  }

  _onDownloadError(downloadable, downloadURL, error, responseStatus,
                   redirectCallback)
  {
    Prefs.notificationdata.lastError = Date.now();
    Prefs.notificationdata.downloadStatus = error;

    saveNotificationData();
  }

  _onBlockedTotal()
  {
    this.showNext();
  }

  /**
   * Adds a listener for notifications to be shown.
   * @param {function} listener Listener to be invoked when a notification is
   *   to be shown.
   */
  addShowListener(listener)
  {
    this._showListeners.add(listener);
  }

  /**
   * Removes the supplied listener.
   * @param {function} listener Listener that was added via
   *   `{@link module:notifications~Notifications#addShowListener
   *           addShowListener()}`.
   */
  removeShowListener(listener)
  {
    this._showListeners.delete(listener);
  }

  /**
   * Returns whether user preferences indicate that notifications
   * should be ignored.
   * @return {boolean}
   */
  shouldIgnoreNotifications()
  {
    return this.ignored ||
      Prefs.notifications_ignoredcategories.includes("*");
  }

  /**
   * Returns all local and remote notifications.
   * @returns {Array.<object>} All local and remote notifications.
   * @private
   */
  _getNotifications()
  {
    let remoteNotifications = [];
    if (typeof Prefs.notificationdata.data == "object" &&
        Prefs.notificationdata.data.notifications instanceof Array)
      remoteNotifications = Prefs.notificationdata.data.notifications;

    return [...this._localNotifications, ...remoteNotifications];
  }

  /**
   * Determines which notification is to be shown next.
   * @returns {?object} The notification to be shown, or `null` if there is
   *   none.
   * @private
   */
  _getNextToShow()
  {
    let notifications = this._getNotifications();
    if (notifications.length == 0)
      return null;

    const {addonName, addonVersion, application, applicationVersion,
           platform, platformVersion} = require("info");

    let targetChecks = {
      extension: v => v == addonName,
      extensionMinVersion:
        v => compareVersions(addonVersion, v) >= 0,
      extensionMaxVersion:
        v => compareVersions(addonVersion, v) <= 0,
      application: v => v == application,
      applicationMinVersion:
        v => compareVersions(applicationVersion, v) >= 0,
      applicationMaxVersion:
        v => compareVersions(applicationVersion, v) <= 0,
      platform: v => v == platform,
      platformMinVersion:
        v => compareVersions(platformVersion, v) >= 0,
      platformMaxVersion:
        v => compareVersions(platformVersion, v) <= 0,
      blockedTotalMin: v => Prefs.show_statsinpopup &&
        Prefs.blocked_total >= v,
      blockedTotalMax: v => Prefs.show_statsinpopup &&
        Prefs.blocked_total <= v,
      locales: v => v.includes(this.locale)
    };

    let notificationToShow = null;
    for (let notification of notifications)
    {
      if (notification.type != "critical")
      {
        let shown;
        if (typeof Prefs.notificationdata.shown == "object")
          shown = Prefs.notificationdata.shown[notification.id];

        if (typeof shown != "undefined")
        {
          if (typeof notification.interval == "number")
          {
            if (shown + notification.interval > Date.now())
              continue;
          }
          else if (shown)
          {
            continue;
          }
        }

        if (notification.type != "relentless" &&
            this.shouldIgnoreNotifications())
          continue;
      }

      if (notification.targets instanceof Array)
      {
        let match = false;

        for (let target of notification.targets)
        {
          let keys = Object.keys(target);
          if (keys.every(
            key => Object.prototype.hasOwnProperty.call(targetChecks, key) &&
                   targetChecks[key](target[key]))
          )
          {
            match = true;
            break;
          }
        }

        if (!match)
          continue;
      }

      if (!notificationToShow || getNumericalSeverity(notification) >
                                 getNumericalSeverity(notificationToShow))
        notificationToShow = notification;
    }

    return notificationToShow;
  }

  /**
   * Shows a notification once.
   * @param {object} notification The notification to show.
   * @param {object} [options]
   * @param {boolean} [options.ignorable=false] If true, user preference will
   *   be considered to determine whether notification should be shown.
   */
  showNotification(notification, options = {})
  {
    if (options.ignorable && this.shouldIgnoreNotifications())
      return;

    for (let showListener of this._showListeners)
      showListener(notification);
  }

  /**
   * Invokes the listeners added via
   * `{@link module:notifications~Notifications#addShowListener
   *         addShowListener()}` with the next notification to be shown.
   */
  showNext()
  {
    let notification = this._getNextToShow();
    if (notification)
    {
      for (let showListener of this._showListeners)
        showListener(notification);
    }
  }

  /**
   * Marks a notification as shown.
   * @param {string} id The ID of the notification to be marked as shown.
   */
  markAsShown(id)
  {
    let notifications = this._getNotifications();

    // Ignore if there's no notification with the given ID.
    if (!notifications.some(notification => notification.id == id))
      return;

    let now = Date.now();
    let data = Prefs.notificationdata;

    if (data.shown instanceof Array)
    {
      let newShown = {};
      for (let oldId of data.shown)
        newShown[oldId] = now;
      data.shown = newShown;
    }

    if (typeof data.shown != "object")
      data.shown = {};

    data.shown[id] = now;

    saveNotificationData();
  }

  /**
   * Localizes the texts of the given notification based on the
   * {@link module:notifications~Notifications#locale locale}.
   * @param {object} notification The notification.
   * @returns {object} The localized texts.
   */
  getLocalizedTexts(notification)
  {
    let textKeys = ["title", "message"];
    let localizedTexts = {};

    for (let key of textKeys)
    {
      if (key in notification)
      {
        if (typeof notification[key] == "string")
          localizedTexts[key] = notification[key];
        else
          localizedTexts[key] = localize(notification[key], this.locale);
      }
    }

    return localizedTexts;
  }

  /**
   * Adds a local notification.
   * @param {object} notification The notification to add.
   */
  addNotification(notification)
  {
    if (!isValidNotification(notification))
      throw new Error(`Invalid notification with ID "${notification.id}"`);

    this._localNotifications.add(notification);
  }

  /**
   * Removes an existing local notification.
   * @param {object} notification The notification to remove.
   */
  removeNotification(notification)
  {
    this._localNotifications.delete(notification);
  }

  /**
   * Toggles whether notifications of a specific category should be ignored.
   * @param {string} category The notification category identifier.
   * @param {boolean} [forceValue] Whether to force the specified value.
   */
  toggleIgnoreCategory(category, forceValue)
  {
    let categories = Prefs.notifications_ignoredcategories;
    let index = categories.indexOf(category);
    if (index == -1 && forceValue != false)
      categories.push(category);
    else if (index != -1 && forceValue != true)
      categories.splice(index, 1);

    saveNotificationData("notifications_ignoredcategories");
  }
}

/**
 * Regularly fetches notifications and decides which notification to show.
 * @type {module:notifications~Notifications}
 */
exports.notifications = new Notifications();
