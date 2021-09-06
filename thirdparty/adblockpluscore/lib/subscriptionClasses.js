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
 * @fileOverview Definition of Subscription class and its subclasses.
 */

const {recommendations} = require("./recommendations");
const {isActiveFilter} = require("./filterClasses");
const {filterNotifier} = require("./filterNotifier");
const {Downloader} = require("./downloader");

/**
 * Subscription types by URL.
 *
 * @type {Map.<string, string>}
 */
let typesByURL = new Map(
  (function*()
  {
    for (let {type, url} of recommendations())
      yield [url, type];
  })()
);

let Subscription =
/**
 * The `Subscription` class represents a filter subscription.
 * @abstract
 */
exports.Subscription = class Subscription
{
  /**
   * Creates a `Subscription` object.
   * @param {string} url The URL of the subscription.
   * @param {string} [title] The title of the subscription.
   * @private
   */
  constructor(url, title)
  {
    /**
     * The URL of the subscription.
     * @type {string}
     * @see module:subscriptionClasses.Subscription#url
     * @private
     */
    this._url = url;

    /**
     * Whether the URL of the subscription is a valid subscription URL.
     * @type {boolean}
     * @private
     * @see module:subscriptionClasses.Subscription#valid
     */
    this._urlValid = Subscription.isValidURL(url);

    /**
     * The type of the subscription.
     * @type {?string}
     * @default <code>null</code>
     * @private
     * @see module:subscriptionClasses.Subscription#type
     */
    this._type = null;

    /**
     * Filter text contained in the subscription.
     * @type {Array.<string>}
     * @private
     */
    this._filterText = [];

    /**
     * A searchable index of filter text in the subscription.
     * @type {Set.<string>}
     * @private
     */
    this._filterTextIndex = new Set();

    /**
     * The title of the subscription.
     * @type {?string}
     * @default <code>null</code>
     * @private
     * @see module:subscriptionClasses.Subscription#title
     */
    this._title = null;

    if (title)
      this._title = title;

    /**
     * Whether the title of the subscription is non-editable.
     * @type {boolean}
     * @default <code>false</code>
     * @private
     * @see module:subscriptionClasses.Subscription#fixedTitle
     */
    this._fixedTitle = false;

    /**
     * Whether the subscription is disabled.
     * @type {boolean}
     * @default <code>false</code>
     * @private
     * @see module:subscriptionClasses.Subscription#disabled
     */
    this._disabled = false;

    Subscription.knownSubscriptions.set(url, this);
  }

  /**
   * The URL of the subscription.
   * @type {string}
   */
  get url()
  {
    return this._url;
  }

  /**
   * Whether the subscription is valid.
   * @type {boolean}
   * @package
   */
  get valid()
  {
    // This should return a value based on Subscription#_urlValid after
    // https://gitlab.com/eyeo/adblockplus/abpui/adblockplusui/-/issues/753
    return true;
  }

  /**
   * The type of the subscription.
   * @type {?string}
   * @default <code>null</code>
   */
  get type()
  {
    return this._type;
  }

  /**
   * The title of the subscription.
   * @type {string}
   */
  get title()
  {
    return this._title;
  }

  set title(value)
  {
    if (value != this._title)
    {
      let oldValue = this._title;
      this._title = value;
      filterNotifier.emit("subscription.title", this, value, oldValue);
    }
  }

  /**
   * Whether the title of the subscription is non-editable.
   * @type {boolean}
   * @default <code>false</code>
   */
  get fixedTitle()
  {
    return this._fixedTitle;
  }

  set fixedTitle(value)
  {
    if (value != this._fixedTitle)
    {
      let oldValue = this._fixedTitle;
      this._fixedTitle = value;
      filterNotifier.emit("subscription.fixedTitle", this, value, oldValue);
    }
  }

  /**
   * Whether the subscription is disabled.
   * @type {boolean}
   * @default <code>false</code>
   */
  get disabled()
  {
    return this._disabled;
  }

  set disabled(value)
  {
    if (value != this._disabled)
    {
      let oldValue = this._disabled;
      this._disabled = value;
      filterNotifier.emit("subscription.disabled", this, value, oldValue);
    }
  }

  /**
   * The number of filters in the subscription.
   * @type {number}
   * @default <code>0</code>
   */
  get filterCount()
  {
    return this._filterText.length;
  }

  /**
   * Returns an iterator that yields the text for each filter in the
   * subscription.
   * @returns {Iterator.<string>}
   */
  filterText()
  {
    return this._filterText[Symbol.iterator]();
  }

  /**
   * Checks whether the subscription has the given filter text.
   * @param {string} filterText The filter text.
   * @returns {boolean} Whether the subscription has the filter text.
   * @package
   */
  hasFilterText(filterText)
  {
    return this._filterTextIndex.has(filterText);
  }

  /**
   * Returns the filter text at the given `0`-based index.
   * @param {number} index The `0`-based index. If the index is out of bounds,
   *   the return value is `null`.
   * @returns {?module:filterClasses.Filter} The filter text.
   */
  filterTextAt(index)
  {
    return this._filterText[index] || null;
  }

  /**
   * Returns the `0`-based index of the given filter.
   *
   * @param {module:filterClasses.Filter} filter The filter.
   * @param {number} [fromIndex] The `0`-based index from which to start the
   *   search.
   *
   * @returns {number} The `0`-based index at which the filter is found. If the
   *   filter is not found in the subscription, the return value is `-1`.
   */
  findFilterIndex(filter, fromIndex = 0)
  {
    return this._filterText.indexOf(filter.text, fromIndex);
  }

  /**
   * Removes all filters from the subscription.
   */
  clearFilters()
  {
    this._filterText = [];
    this._filterTextIndex.clear();
  }

  /**
   * Adds a filter to the subscription.
   * @param {Filter} filter The filter.
   */
  addFilter(filter)
  {
    this._filterText.push(filter.text);
    this._filterTextIndex.add(filter.text);
  }

  /**
   * Inserts a filter into the subscription at the given `0`-based index.
   *
   * @param {module:filterClasses.Filter} filter The filter.
   * @param {number} index The `0`-based index. If the index is out of bounds,
   *   the filter is inserted at the beginning or at the end accordingly.
   */
  insertFilterAt(filter, index)
  {
    this._filterText.splice(index, 0, filter.text);
    this._filterTextIndex.add(filter.text);
  }

  /**
   * Deletes a filter from the subscription at the given `0`-based index.
   * @param {number} index The `0`-based index. If the index is out of bounds,
   *   no filter is deleted.
   */
  deleteFilterAt(index)
  {
    // Ignore index if out of bounds on the negative side, for consistency.
    if (index < 0)
      return;

    let [filterText] = this._filterText.splice(index, 1);
    if (!this._filterText.includes(filterText))
      this._filterTextIndex.delete(filterText);
  }

  /**
   * Updates the filter text of the subscription.
   * @param {Array.<string>} filterText The new filter text.
   * @returns {{added: Array.<string>, removed: Array.<string>}} An object
   *   containing two lists of the text of added and removed filters
   *   respectively.
   * @package
   */
  updateFilterText(filterText)
  {
    let added = [];
    let removed = [];

    if (this._filterText.length == 0)
    {
      added = [...filterText];
    }
    else if (filterText.length > 0)
    {
      for (let text of filterText)
      {
        if (!this._filterTextIndex.has(text))
          added.push(text);
      }
    }

    this._filterTextIndex = new Set(filterText);

    if (filterText.length == 0)
    {
      removed = [...this._filterText];
    }
    else if (this._filterText.length > 0)
    {
      for (let text of this._filterText)
      {
        if (!this._filterTextIndex.has(text))
          removed.push(text);
      }
    }

    this._filterText = [...filterText];

    return {added, removed};
  }

  /**
   * Serializes the subscription for writing out on disk.
   * @yields {string}
   * @package
   */
  *serialize()
  {
    let {url, _title, _fixedTitle, _disabled} = this;

    yield "[Subscription]";
    yield "url=" + url;

    if (_title)
      yield "title=" + _title;
    if (_fixedTitle)
      yield "fixedTitle=true";
    if (_disabled)
      yield "disabled=true";
  }

  /**
   * Serializes the subscription's filter text for writing out on disk.
   * @yields {string}
   * @package
   */
  *serializeFilters()
  {
    let {_filterText} = this;

    yield "[Subscription filters]";

    for (let text of _filterText)
      yield text.replace(/\[/g, "\\[");
  }

  /**
   * Returns a string representing the subscription.
   * @returns {string}
   */
  toString()
  {
    return [...this.serialize()].join("\n");
  }
};

/**
 * Cache for known filter subscriptions that maps subscription URLs to
 * subscription objects.
 * @type {Map.<string, module:subscriptionClasses.Subscription>}
 * @package
 */
exports.Subscription.knownSubscriptions = new Map();

/**
 * Returns the subscription object for a subscription URL.
 *
 * Every subscription URL maps to its own unique object. If no such object
 * exists, a new one is created internally; otherwise the existing object is
 * used.
 *
 * @param {string} url The subscription URL.
 *
 * @returns {module:subscriptionClasses.Subscription} A subscription object.
 */
exports.Subscription.fromURL = function(url)
{
  let subscription = Subscription.knownSubscriptions.get(url);
  if (subscription)
    return subscription;

  if (url[0] != "~")
  {
    subscription = new DownloadableSubscription(url, null);

    let type = typesByURL.get(url);
    if (typeof type != "undefined")
      subscription._type = type;

    return subscription;
  }

  return new SpecialSubscription(url);
};

/**
 * Deserializes a subscription.
 * @param {object} obj A map of serialized properties and their values.
 * @returns {module:subscriptionClasses.Subscription} A subscription object.
 * @package
 */
exports.Subscription.fromObject = function(obj)
{
  let result;
  if (obj.url[0] != "~")
  {
    // URL is valid - this is a downloadable subscription
    result = new DownloadableSubscription(obj.url, obj.title);
    if ("downloadStatus" in obj)
      result._downloadStatus = obj.downloadStatus;
    if ("lastSuccess" in obj)
      result.lastSuccess = parseInt(obj.lastSuccess, 10) || 0;
    if ("lastCheck" in obj)
      result._lastCheck = parseInt(obj.lastCheck, 10) || 0;
    if ("expires" in obj)
      result.expires = parseInt(obj.expires, 10) || 0;
    if ("softExpiration" in obj)
      result.softExpiration = parseInt(obj.softExpiration, 10) || 0;
    if ("errors" in obj)
      result._errors = parseInt(obj.errors, 10) || 0;
    if ("version" in obj)
      result.version = parseInt(obj.version, 10) || 0;
    if ("requiredVersion" in obj)
      result.requiredVersion = obj.requiredVersion;
    if ("homepage" in obj)
      result._homepage = obj.homepage;
    if ("lastDownload" in obj)
      result._lastDownload = parseInt(obj.lastDownload, 10) || 0;
    if ("downloadCount" in obj)
      result.downloadCount = parseInt(obj.downloadCount, 10) || 0;

    let type = typesByURL.get(obj.url);
    if (typeof type != "undefined")
      result._type = type;
  }
  else
  {
    result = new SpecialSubscription(obj.url, obj.title);
    if ("defaults" in obj)
      result.defaults = obj.defaults.split(" ");
  }
  if ("fixedTitle" in obj)
    result._fixedTitle = (obj.fixedTitle == "true");
  if ("disabled" in obj)
    result._disabled = (obj.disabled == "true");

  return result;
};

/**
 * Checks whether a URL is a valid subscription URL.
 * @param {string} url The URL.
 * @returns {boolean} Whether the URL is a valid subscription URL.
 */
exports.Subscription.isValidURL = function isValidURL(url)
{
  return url.startsWith("~user~") || Downloader.isValidURL(url);
};

let SpecialSubscription =
/**
 * The `SpecialSubscription` class represents a special filter subscription.
 *
 * This type of subscription is used for keeping user-defined filters.
 */
exports.SpecialSubscription = class SpecialSubscription extends Subscription
{
  /**
   * Creates a `SpecialSubscription` object.
   * @param {string} url The URL of the subscription.
   * @param {string} [title] The title of the subscription.
   * @private
   */
  constructor(url, title)
  {
    super(url, title);

    /**
     * Filter types that should be added to this subscription by default.
     *
     * Entries should correspond to keys in
     * `{@link module:subscriptionClasses.SpecialSubscription.defaultsMap}`.
     *
     * @type {?Array.<string>}
     *
     * @package
     */
    this.defaults = null;
  }

  /**
   * Checks whether the given filter should be added to this subscription by
   * default.
   * @param {Filter} filter The filter.
   * @returns {boolean} Whether the filter should be added to this subscription
   *   by default.
   * @package
   */
  isDefaultFor(filter)
  {
    if (this.defaults && this.defaults.length)
    {
      for (let type of this.defaults)
      {
        if (SpecialSubscription.defaultsMap.get(type).includes(filter.type))
          return true;
        if (!isActiveFilter(filter) && type == "blocking")
          return true;
      }
    }

    return false;
  }

  /**
   * Serializes the subscription for writing out on disk.
   * @yields {string}
   * @package
   */
  *serialize()
  {
    let {defaults, _lastDownload} = this;

    yield* super.serialize();

    if (defaults)
    {
      yield "defaults=" +
            defaults.map(
              // remap for the stored format.
              type => type == "allowing" ? "whitelist" : type
            ).filter(
              type => SpecialSubscription.defaultsMap.has(type)
            ).join(" ");
    }
    if (_lastDownload)
      yield "lastDownload=" + _lastDownload;
  }
};

/**
 * A map of filter types.
 * @type {Map.<string, Array.<string>>}
 * @package
 */
exports.SpecialSubscription.defaultsMap = new Map([
  ["allowing", ["allowing"]],
  // deprecated terminology
  ["whitelist", ["allowing"]],
  ["blocking", ["blocking"]],
  ["elemhide", ["elemhide", "elemhideexception", "elemhideemulation"]]
]);

/**
 * Creates a new special subscription.
 * @param {string} [title] The title of the subscription.
 * @returns {module:subscriptionClasses.SpecialSubscription} A new special
 *   subscription.
 * @package
 */
exports.SpecialSubscription.create = function(title)
{
  let url;
  do
    url = "~user~" + Math.round(Math.random() * 1000000);
  while (Subscription.knownSubscriptions.has(url));
  return new SpecialSubscription(url, title);
};

/**
 * Creates a new special subscription and adds the given filter to it.
 *
 * Once created, the subscription acts as the default for all filters of the
 * {@link module:filterClasses.Filter#type type}.
 *
 * @param {module:filterClasses.Filter} filter The filter.
 *
 * @returns {module:subscriptionClasses.SpecialSubscription} A new special
 *   subscription.
 *
 * @package
 */
exports.SpecialSubscription.createForFilter = function(filter)
{
  let subscription = SpecialSubscription.create();
  subscription.addFilter(filter);
  for (let [type, mappedTypes] of SpecialSubscription.defaultsMap)
  {
    if (mappedTypes.includes(filter.type))
      subscription.defaults = [type];
  }
  if (!subscription.defaults)
    subscription.defaults = ["blocking"];
  return subscription;
};

let RegularSubscription =
/**
 * The `RegularSubscription` class represents a regular filter subscription.
 * @abstract
 */
exports.RegularSubscription = class RegularSubscription extends Subscription
{
  /**
   * Creates a `RegularSubscription` object.
   * @param {string} url The URL of the subscription.
   * @param {string} [title] The title of the subscription.
   * @private
   */
  constructor(url, title)
  {
    super(url, title || url);

    /**
     * The homepage of the subscription.
     * @type {?string}
     * @default <code>null</code>
     * @private
     * @see module:subscriptionClasses.RegularSubscription#homepage
     */
    this._homepage = null;

    /**
     * The last time the subscription was downloaded, in seconds since the
     * beginning of the Unix epoch.
     * @type {number}
     * @default <code>0</code>
     * @private
     * @see module:subscriptionClasses.RegularSubscription#lastDownload
     */
    this._lastDownload = 0;
  }

  /**
   * The homepage of the subscription.
   * @type {?string}
   * @default <code>null</code>
   */
  get homepage()
  {
    return this._homepage;
  }

  set homepage(value)
  {
    if (value != this._homepage)
    {
      let oldValue = this._homepage;
      this._homepage = value;
      filterNotifier.emit("subscription.homepage", this, value, oldValue);
    }
  }

  /**
   * The last time the subscription was downloaded, in seconds since the
   * beginning of the Unix epoch.
   * @type {number}
   * @default <code>0</code>
   */
  get lastDownload()
  {
    return this._lastDownload;
  }

  set lastDownload(value)
  {
    if (value != this._lastDownload)
    {
      let oldValue = this._lastDownload;
      this._lastDownload = value;
      filterNotifier.emit("subscription.lastDownload", this, value, oldValue);
    }
  }

  /**
   * Serializes the subscription for writing out on disk.
   * @yields {string}
   * @package
   */
  *serialize()
  {
    let {_homepage, _lastDownload} = this;

    yield* super.serialize();

    if (_homepage)
      yield "homepage=" + _homepage;
    if (_lastDownload)
      yield "lastDownload=" + _lastDownload;
  }
};

let DownloadableSubscription =
/**
 * The `DownloadableSubscription` class represents a regular filter
 * subscription that is downloaded by Adblock Plus.
 */
exports.DownloadableSubscription =
class DownloadableSubscription extends RegularSubscription
{
  /**
   * Creates a `DownloadableSubscription` object.
   * @param {string} url The URL of the subscription.
   * @param {string} [title] The title of the subscription.
   * @private
   */
  constructor(url, title)
  {
    super(url, title);

    /**
     * The status of the last download.
     * @type {?string}
     * @default <code>null</code>
     * @private
     * @see module:subscriptionClasses.DownloadableSubscription#downloadStatus
     */
    this._downloadStatus = null;

    /**
     * The last time the subscription was considered for an update, in seconds
     * since the beginning of the Unix epoch.
     * @type {number}
     * @default <code>0</code>
     * @private
     * @see module:subscriptionClasses.DownloadableSubscription#lastCheck
     */
    this._lastCheck = 0;

    /**
     * The number of download failures since the last successful download.
     * @type {number}
     * @default <code>0</code>
     * @private
     * @see module:subscriptionClasses.DownloadableSubscription#errors
     */
    this._errors = 0;

    /**
     * The last time the subscription was successfully downloaded, in seconds
     * since the beginning of the Unix epoch.
     * @type {number}
     * @default <code>0</code>
     */
    this.lastSuccess = 0;

    /**
     * The hard expiration time of the subscription, in seconds since the
     * beginning of the Unix epoch.
     * @type {number}
     * @default <code>0</code>
     */
    this.expires = 0;

    /**
     * The soft expiration time of the subscription, in seconds since the
     * beginning of the Unix epoch.
     * @type {number}
     * @default <code>0</code>
     */
    this.softExpiration = 0;

    /**
     * The version of the subscription data that was retrieved on last
     * successful download.
     * @type {number}
     * @default <code>0</code>
     */
    this.version = 0;

    /**
     * The minimal Adblock Plus version required for the subscription.
     * @type {?string}
     * @default <code>null</code>
     */
    this.requiredVersion = null;

    /**
     * The number of times the subscription has been downloaded.
     * @type {number}
     * @default <code>0</code>
     */
    this.downloadCount = 0;
  }

  /**
   * The status of the last download.
   *
   * This is a message ID like `synchronize_ok` or
   * `synchronize_connection_error`.
   *
   * @type {?string}
   * @default <code>null</code>
   */
  get downloadStatus()
  {
    return this._downloadStatus;
  }

  set downloadStatus(value)
  {
    let oldValue = this._downloadStatus;
    this._downloadStatus = value;
    filterNotifier.emit("subscription.downloadStatus", this, value, oldValue);
  }

  /**
   * The last time the subscription was considered for an update, in seconds
   * since the beginning of the Unix epoch.
   *
   * This is used to increase the soft expiration time if the user doesn't use
   * Adblock Plus for some time.
   *
   * @type {number}
   * @default <code>0</code>
   */
  get lastCheck()
  {
    return this._lastCheck;
  }

  set lastCheck(value)
  {
    if (value != this._lastCheck)
    {
      let oldValue = this._lastCheck;
      this._lastCheck = value;
      filterNotifier.emit("subscription.lastCheck", this, value, oldValue);
    }
  }

  /**
   * The number of download failures since the last successful download.
   * @type {number}
   * @default <code>0</code>
   */
  get errors()
  {
    return this._errors;
  }

  set errors(value)
  {
    if (value != this._errors)
    {
      let oldValue = this._errors;
      this._errors = value;
      filterNotifier.emit("subscription.errors", this, value, oldValue);
    }
  }

  /**
   * Serializes the subscription for writing out on disk.
   * @yields {string}
   * @package
   */
  *serialize()
  {
    let {downloadStatus, lastSuccess, lastCheck, expires,
         softExpiration, errors, version, requiredVersion,
         downloadCount} = this;

    yield* super.serialize();

    if (downloadStatus)
      yield "downloadStatus=" + downloadStatus;
    if (lastSuccess)
      yield "lastSuccess=" + lastSuccess;
    if (lastCheck)
      yield "lastCheck=" + lastCheck;
    if (expires)
      yield "expires=" + expires;
    if (softExpiration)
      yield "softExpiration=" + softExpiration;
    if (errors)
      yield "errors=" + errors;
    if (version)
      yield "version=" + version;
    if (requiredVersion)
      yield "requiredVersion=" + requiredVersion;
    if (downloadCount)
      yield "downloadCount=" + downloadCount;
  }
};
