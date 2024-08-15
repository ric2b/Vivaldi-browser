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

import {hideIfMatches} from "../utils/dom.js";
import {toRegExp} from "../utils/general.js";
import {raceWinner} from "../introspection/race.js";
import {getDebugger} from "../introspection/log.js";

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * the text content of the element contains a given string.
 * @alias module:content/snippets.hide-if-contains
 *
 * @param {string} search The string to look for in HTML elements. If the
 *   string begins and ends with a slash (`/`), the text in between is treated
 *   as a regular expression.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 * @param {?string} [searchSelector] The CSS selector that an HTML element
 *   containing the given string must match. Defaults to the value of the
 *   `selector` argument.
 *
 * @since Adblock Plus 3.3
 */
export function hideIfContains(search, selector = "*", searchSelector = null) {
  const debugLog = getDebugger("hide-if-contains");
  const onHideCallback = node => {
    debugLog("success",
             "Matched: ",
             node,
             " for selector: ",
             selector,
             searchSelector);
  };
  let re = toRegExp(search);

  const mo = hideIfMatches(element => re.test($(element).textContent),
                           selector,
                           searchSelector,
                           onHideCallback);
  mo.race(raceWinner(
    "hide-if-contains",
    () => {
      mo.disconnect();
    }
  ));
}
