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

import {$$, $closest, getComputedCSSText, hideElement} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";
import {toRegExp} from "../utils/general.js";
import {waitUntilEvent} from "../utils/execution.js";

import {debug} from "../introspection/debug.js";
import {getDebugger} from "../introspection/log.js";

let {MutationObserver, WeakSet, getComputedStyle} = $(window);

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * the text content of the element contains a given string and, optionally, if
 * the element's computed style contains a given string.
 * @alias module:content/snippets.hide-if-contains-and-matches-style
 *
 * @param {string} search The string to look for in HTML elements. If the
 *   string begins and ends with a slash (`/`), the text in between is treated
 *   as a regular expression.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 * @param {string?} [searchSelector] The CSS selector that an HTML element
 *   containing the given string must match. Defaults to the value of the
 *   `selector` argument.
 * @param {string?} [style] The string that the computed style of an HTML
 *   element matching `selector` must contain. If the string begins and ends
 *   with a slash (`/`), the text in between is treated as a regular
 *   expression.
 * @param {string?} [searchStyle] The string that the computed style of an HTML
 *   element matching `searchSelector` must contain. If the string begins and
 *   ends with a slash (`/`), the text in between is treated as a regular
 *   expression.
 * @param {string} waitUntil Optional parameter that can be used to delay
 *   the running of the snippet until the given state is reached.
 *   Accepts: loading, interactive, complete, load or any event name
 * @param {string} windowWidthMin Optional parameter that can be used to
 *   disable the snippet if window.innerWidth is smaller than the given value
 * @param {string} windowWidthMax Optional parameter that can be used to
 *   disable the snippet if window.innerWidth is greater than the given value
 *
 * @since Adblock Plus 3.3.2
 */

export function hideIfContainsAndMatchesStyle(search,
                                              selector = "*",
                                              searchSelector = null,
                                              style = null,
                                              searchStyle = null,
                                              waitUntil,
                                              windowWidthMin = null,
                                              windowWidthMax = null
) {
  const debugLog = getDebugger("hide-if-contains-and-matches-style");
  const hiddenMap = new WeakSet();
  const logMap = debug() && new WeakSet();
  if (searchSelector == null)
    searchSelector = selector;

  const searchRegExp = toRegExp(search);

  const styleRegExp = style ? toRegExp(style) : null;
  const searchStyleRegExp = searchStyle ? toRegExp(searchStyle) : null;
  const mainLogic = () => {
    const callback = () => {
      if ((windowWidthMin && window.innerWidth < windowWidthMin) ||
         (windowWidthMax && window.innerWidth > windowWidthMax)
      )
        return;
      for (const {element, rootParents} of $$(searchSelector, true)) {
        if (hiddenMap.has(element))
          continue;
        if (searchRegExp.test($(element).textContent)) {
          if (!searchStyleRegExp ||
            searchStyleRegExp.test(getComputedCSSText(element))) {
            const closest = $closest($(element), selector, rootParents);
            if (!closest)
              continue;
            if (!styleRegExp || styleRegExp.test(getComputedCSSText(closest))) {
              win();
              hideElement(closest);
              hiddenMap.add(element);
              debugLog("success",
                       "Matched: ",
                       closest,
                       "which contains: ",
                       element,
                       " for params: ",
                       ...arguments);
            }
            else {
              if (!logMap || logMap.has(closest))
                continue;
              debugLog("info",
                       "In this element the searchStyle matched " +
                       "but style didn't:\n",
                       closest,
                       getComputedStyle(closest),
                       ...arguments);
              logMap.add(closest);
            }
          }
          else {
            if (!logMap || logMap.has(element))
              continue;
            debugLog("info",
                     "In this element the searchStyle didn't match:\n",
                     element,
                     getComputedStyle(element),
                     ...arguments);
            logMap.add(element);
          }
        }
      }
    };

    const mo = new MutationObserver(callback);
    const win = raceWinner(
      "hide-if-contains-and-matches-style",
      () => mo.disconnect()
    );
    mo.observe(document, {childList: true, characterData: true, subtree: true});
    callback();
  };
  waitUntilEvent(debugLog, mainLogic, waitUntil);
}
