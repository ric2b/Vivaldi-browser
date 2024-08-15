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

import {getDebugger} from "../introspection/log.js";
import {overrideValue, wrapPropertyAccess} from "../utils/execution.js";

let {Error} = $(window);

/**
 * Overrides a property's value on the window object with a set of
 * available properties.
 *
 * Possible values to override the property with:
 *   undefined
 *   false
 *   true
 *   null
 *   noopFunc   - function with empty body
 *   trueFunc   - function returning true
 *   falseFunc  - function returning false
 *   emptyArray  - an array with no elements
 *   emptyObject - an object with no properties
 *   ''         - empty string
 *   positive decimal integer, no sign, with maximum value of 0x7FFF
 *
 * The idea originates from
 * [uBlock Origin](https://github.com/uBlockOrigin/uAssets/blob/80b195436f8f8d78ba713237bfc268ecfc9d9d2b/filters/resources.txt#L2105).
 * @alias module:content/snippets.override-property-read
 *
 * @param {string} property The name of the property.
 * @param {string} value The value to override the property with.
 * @param {?string} setConfigurable Value of the configurable attribute. Sets
 *   the flag to false if "false" is given, the flag is set to true
 *   for all other cases.
 *
 * @since Adblock Plus 3.9.4
 */
export function overridePropertyRead(property, value, setConfigurable) {
  if (!property) {
    throw new Error("[override-property-read snippet]: " +
                     "No property to override.");
  }
  if (typeof value === "undefined") {
    throw new Error("[override-property-read snippet]: " +
                     "No value to override with.");
  }

  let debugLog = getDebugger("override-property-read");

  let cValue = overrideValue(value);

  let newGetter = () => {
    debugLog("success", `${property} override done.`);
    return cValue;
  };

  debugLog("info", `Overriding ${property}.`);

  const configurableFlag = !(setConfigurable === "false");

  wrapPropertyAccess(window,
                     property,
                     {get: newGetter, set() {}},
                     configurableFlag);
}
