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

import {abortOnWrite} from "../utils/execution.js";

/**
 * Patches a property on the window object to abort execution when the
 * property is written.
 *
 * No error is printed to the console.
 *
 * The idea originates from
 * [uBlock Origin](https://github.com/uBlockOrigin/uAssets/blob/80b195436f8f8d78ba713237bfc268ecfc9d9d2b/filters/resources.txt#L1671).
 * @alias module:content/snippets.abort-on-property-write
 *
 * @param {string} property The name of the property.
 * @param {?string} setConfigurable Value of the configurable attribute. Sets
 *   the flag to false if "false" is given, the flag is set to true
 *   for all other cases.
 *
 * @since Adblock Plus 3.4.3
 */
export function abortOnPropertyWrite(property, setConfigurable) {
  const configurableFlag = !(setConfigurable === "false");
  abortOnWrite("abort-on-property-write", window, property, configurableFlag);
}
