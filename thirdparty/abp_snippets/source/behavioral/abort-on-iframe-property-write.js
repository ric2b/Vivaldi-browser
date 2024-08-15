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

import {abortOnIframe} from "../utils/execution.js";

/**
 * Patches a list of properties on the iframes' window object to abort execution
 * when the property is written.
 *
 * No error is printed to the console.
 * @alias module:content/snippets.abort-on-iframe-property-write
 *
 * @param {...string} properties The list with the properties.
 *
 * @since Adblock Plus 3.10.1
 */
export function abortOnIframePropertyWrite(...properties) {
  abortOnIframe(properties, false, true);
}
