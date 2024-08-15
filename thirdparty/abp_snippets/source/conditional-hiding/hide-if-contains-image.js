/*!
 * This file is part of eyeo's Anti-Circumvention Snippets module (@eyeo/snippets),
 * Copyright (C) 2006-present eyeo GmbH
 * 
 * @eyeo/snippets is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 * 
 * @eyeo/snippets is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with @eyeo/snippets.  If not, see <http://www.gnu.org/licenses/>.
 */

import $ from "../$.js";

import {$$, $closest, hideElement} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";
import {toRegExp} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";

let {
  clearTimeout,
  fetch,
  getComputedStyle,
  setTimeout,
  Map,
  MutationObserver,
  Uint8Array
} = $(window);

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * the background image of the element matches a given pattern.
 * @alias module:content/snippets.hide-if-contains-image
 *
 * @param {string} search The pattern to look for in the background images of
 *   HTML elements. This must be the hexadecimal representation of the image
 *   data for which to look. If the string begins and ends with a slash (`/`),
 *   the text in between is treated as a regular expression.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 * @param {?string} [searchSelector] The CSS selector that an HTML element
 *   containing the given pattern must match. Defaults to the value of the
 *   `selector` argument.
 *
 * @since Adblock Plus 3.4.2
 */
export function hideIfContainsImage(search, selector, searchSelector) {
  if (searchSelector == null)
    searchSelector = selector;

  let searchRegExp = toRegExp(search);

  const debugLog = getDebugger("hide-if-contains-image");

  let callback = () => {
    for (const {element, rootParents} of $$(searchSelector, true)) {
      let style = getComputedStyle(element);
      let match = $(style["background-image"]).match(/^url\("(.*)"\)$/);
      if (match) {
        fetchContent(match[1]).then(content => {
          if (searchRegExp.test(uint8ArrayToHex(new Uint8Array(content)))) {
            let closest = $closest($(element), selector, rootParents);
            if (closest) {
              win();
              hideElement(closest);
              debugLog("success", "Matched: ", closest, " for:", ...arguments);
            }
          }
        });
      }
    }
  };

  let mo = new MutationObserver(callback);
  let win = raceWinner(
    "hide-if-contains-image",
    () => mo.disconnect()
  );
  mo.observe(document, {childList: true, subtree: true});
  callback();
}


/**
 * @typedef {object} FetchContentInfo
 * @property {function} remove
 * @property {Promise} result
 * @property {number} timer
 * @private
 */

/**
 * @type {Map.<string, FetchContentInfo>}
 * @private
 */
let fetchContentMap = new Map();


/**
 * Returns a potentially already resolved fetch auto cleaning, if not requested
 * again, after a certain amount of milliseconds.
 *
 * The resolved fetch is by default `arrayBuffer` but it can be any other kind
 * through the configuration object.
 *
 * @param {string} url The url to fetch
 * @param {object} [options] Optional configuration options.
 *                            By default is {as: "arrayBuffer", cleanup: 60000}
 * @param {string} [options.as] The fetch type: "arrayBuffer", "json", "text"..
 * @param {number} [options.cleanup] The cache auto-cleanup delay in ms: 60000
 *
 * @returns {Promise} The fetched result as Uint8Array|string.
 *
 * @example
 * fetchContent('https://any.url.com').then(arrayBuffer => { ... })
 * @example
 * fetchContent('https://a.com', {as: 'json'}).then(json => { ... })
 * @example
 * fetchContent('https://a.com', {as: 'text'}).then(text => { ... })
 * @private
 */
function fetchContent(url, {as = "arrayBuffer", cleanup = 60000} = {}) {
  // make sure the fetch type is unique as the url fetching text or arrayBuffer
  // will fetch same url twice but it will resolve it as expected instead of
  // keeping the fetch potentially hanging forever.
  let uid = as + ":" + url;
  let details = fetchContentMap.get(uid) || {
    remove: () => fetchContentMap.delete(uid),
    result: null,
    timer: 0
  };
  clearTimeout(details.timer);
  details.timer = setTimeout(details.remove, cleanup);
  if (!details.result) {
    details.result = fetch(url).then(res => res[as]()).catch(details.remove);
    fetchContentMap.set(uid, details);
  }
  return details.result;
}

/**
 * Converts a number to its hexadecimal representation.
 *
 * @param {number} number The number to convert.
 * @param {number} [length] The <em>minimum</em> length of the hexadecimal
 *   representation. For example, given the number `1024` and the length `8`,
 *   the function returns the value `"00000400"`.
 *
 * @returns {string} The hexadecimal representation of the given number.
 * @private
 */
function toHex(number, length = 2) {
  let hex = $(number).toString(16);

  if (hex.length < length)
    hex = $("0").repeat(length - hex.length) + hex;

  return hex;
}

/**
 * Converts a `Uint8Array` object into its hexadecimal representation.
 *
 * @param {Uint8Array} uint8Array The `Uint8Array` object to convert.
 *
 * @returns {string} The hexadecimal representation of the given `Uint8Array`
 *   object.
 * @private
 */
function uint8ArrayToHex(uint8Array) {
  return uint8Array.reduce((hex, byte) => hex + toHex(byte), "");
}
