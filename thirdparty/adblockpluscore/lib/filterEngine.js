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

const {parseURL} = require("./url");
const {Filter} = require("./filterClasses");
const {defaultMatcher} = require("./matcher");
const {elemHide} = require("./elemHide");
const {elemHideEmulation} = require("./elemHideEmulation");
const {elemHideExceptions} = require("./elemHideExceptions");
const {snippets} = require("./snippets");

/**
 * Selects the appropriate module for the given filter type.
 *
 * @param {string} type The type of a filter. This must be one of
 *   `"blocking"`, `"allowing"`, `"elemhide"`, `"elemhideemulation"`,
 *   `"elemhideexception"`, and `"snippet"`; otherwise the function returns
 *   `null`.
 *
 * @returns {?object} The appropriate module for the given filter type.
 */
function selectModule(type)
{
  switch (type)
  {
    case "blocking":
    case "allowing":
      return defaultMatcher;
    case "elemhide":
      return elemHide;
    case "elemhideemulation":
      return elemHideEmulation;
    case "elemhideexception":
      return elemHideExceptions;
    case "snippet":
      return snippets;
  }

  return null;
}

/**
 * `{@link module:filterEngine.filterEngine filterEngine}` implementation.
 */
class FilterEngine
{
  /**
   * Creates the `{@link module:filterEngine.filterEngine filterEngine}`
   * object.
   * @private
   */
  constructor()
  {
    /**
     * The promise returned by
     * `{@link module:filterEngine~FilterEngine#initialize}`.
     * @type {?Promise}
     * @private
     */
    this._initializationPromise = null;
  }

  /**
   * Initializes the `{@link module:filterEngine.filterEngine filterEngine}`
   * object with the given filters or filters loaded from disk.
   *
   * @param {Iterable.<string>} [filterSource] An iterable object that yields
   *   the text of the filters with which to initialize the
   *   `{@link module:filterEngine.filterEngine filterEngine}` object. If
   *   omitted, filters are loaded from disk.
   *
   * @returns {Promise} A promise that is fulfilled when the initialization is
   *   complete.
   *
   * @public
   */
  initialize(filterSource)
  {
    if (!this._initializationPromise)
    {
      if (typeof filterSource == "undefined")
      {
        // Note: filterListener.js must be loaded conditionally here in local
        // scope in order for the engine to be able to work in standalone mode
        // with no disk and network I/O.
        const {filterListener} = require("./filterListener");
        this._initializationPromise = filterListener.initialize(this);
      }
      else
      {
        this._initializationPromise = Promise.resolve().then(() =>
        {
          for (let text of filterSource)
            this.add(Filter.fromText(Filter.normalize(text)));
        });
      }
    }

    return this._initializationPromise;
  }

  /**
   * Adds a new filter.
   * @param {module:filterClasses.ActiveFilter} filter
   * @package
   */
  add(filter)
  {
    let module = selectModule(filter.type);
    if (module)
      module.add(filter);
  }

  /**
   * Removes an existing filter.
   * @param {module:filterClasses.ActiveFilter} filter
   * @package
   */
  remove(filter)
  {
    let module = selectModule(filter.type);
    if (module)
      module.remove(filter);
  }

  /**
   * Checks whether a filter exists.
   * @param {module:filterClasses.ActiveFilter} filter
   * @returns {boolean}
   * @package
   */
  has(filter)
  {
    let module = selectModule(filter.type);
    if (module)
      return module.has(filter);
    return false;
  }

  /**
   * Clears all filters.
   * @package
   */
  clear()
  {
    defaultMatcher.clear();
    elemHide.clear();
    elemHideEmulation.clear();
    elemHideExceptions.clear();
    snippets.clear();
  }

  /**
   * Matches existing URL filters against a web resource
   * (HTML document, CSS style sheet, PNG image, etc.) and returns the matching
   * filter if there's a match.
   *
   * @param {string|URL|module:url~URLInfo} url The URL of the resource.
   * @param {number} typeMask The
   *   {@link module:contentTypes.contentTypes content types} associated with
   *   the resource.
   * @param {string} documentHostname The hostname of the document of the
   *   resource.
   * @param {?string} [sitekey] An optional public key associated with the
   *   document of the resource.
   * @param {boolean} [specificOnly] Whether to ignore any generic filters.
   *
   * @returns {?string} A URL filter if there's a match; otherwise `null`.
   *
   * @public
   */
  match(url, typeMask, documentHostname, sitekey = null, specificOnly = false)
  {
    if (typeof url == "string")
      url = parseURL(url);

    let filter = defaultMatcher.match(url, typeMask, documentHostname, sitekey,
                                      specificOnly);
    return filter ? filter.text : null;
  }
}

/**
 * The `filterEngine` object maintains filters for request blocking, element
 * hiding, and snippets.
 *
 * @type {module:filterEngine~FilterEngine}
 *
 * @public
 *
 * @example
 *
 * let {contentTypes, filterEngine} = require("adblockpluscore");
 *
 * await filterEngine.initialize(
 *   [
 *     "/annoying-ad^$image",
 *     "||example.com/social-widget.html^"
 *   ]
 * );
 *
 * let resource = {
 *   url: "https://ad-server.example.net/annoying-ad.png",
 *   documentURL: "https://news.example.com/world.html"
 * };
 *
 * let filter = filterEngine.match(resource.url, contentTypes.IMAGE,
 *                                 new URL(resource.documentURL).hostname);
 * console.log(filter); // prints "/annoying-ad^$image"
 */
exports.filterEngine = new FilterEngine();
