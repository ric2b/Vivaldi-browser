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
import {getDebugger} from "../introspection/log.js";

const {parseFloat, Math, MutationObserver, WeakSet} = $(window);
const {min} = Math;

// https://webreflection.blogspot.com/2009/02/levenshtein-algorithm-revisited-25.html
const ld = (a, b) => {
  const len1 = a.length + 1;
  const len2 = b.length + 1;
  const d = [[0]];
  let i = 0;
  let I = 0;

  while (++i < len2)
    d[0][i] = i;

  i = 0;
  while (++i < len1) {
    const c = a[I];
    let j = 0;
    let J = 0;
    d[i] = [i];
    while (++j < len2) {
      d[i][j] = min(d[I][j] + 1, d[i][J] + 1, d[I][J] + (c != b[J]));
      ++J;
    }
    ++I;
  }
  return d[len1 - 1][len2 - 1];
};

/**
 * Hides any HTML element matching a CSS selector if the text content
 * contains someting similar to the string to search for.
 * @alias module:content/snippets.hide-if-contains-similar-text
 *
 * @param {string} search The string to look for, such as "Sponsored" or
 *   similar.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 * @param {?string} [searchSelector] The CSS selector that an HTML element
 *   containing the given string must match. Defaults to the value of the
 *   `selector` argument.
 * @param {?number} [ignoreChars] The amount of extra chars to ignore while
 *   looking for the text, allowing possible intermediate chars. If the
 *   `search` string is "Sponsored" and `ignoreChars` is 1, elements containing
 *   "zSponsored", as example, will be considered a match too.
 * @param {?number} [maxSearches] The amount of searches to perform. By default
 *   the search is performed through the whole length of the found text. Pass
 *   an integer to never perform more than X searches per node. As example,
 *   if the word to look for is usually at the beginning, use 1 or 2 to
 *   improve performance while surfing.
 *
 * @since @eyeo/snippets 0.5.2
 */
export function hideIfContainsSimilarText(
  search, selector,
  searchSelector = null,
  ignoreChars = 0,
  maxSearches = 0
) {
  const visitedNodes = new WeakSet();
  const debugLog = getDebugger("hide-if-contains-similar-text");
  const $search = $(search);
  const {length} = $search;
  const chars = length + parseFloat(ignoreChars) || 0;
  const find = $([...$search]).sort();
  const guard = parseFloat(maxSearches) || Infinity;

  if (searchSelector == null)
    searchSelector = selector;

  debugLog("Looking for similar text: " + $search);

  const callback = () => {
    for (const {element, rootParents} of $$(searchSelector, true)) {
      if (visitedNodes.has(element))
        continue;

      visitedNodes.add(element);
      const {innerText} = $(element);
      const loop = min(guard, innerText.length - chars + 1);
      for (let i = 0; i < loop; i++) {
        const str = $(innerText).substr(i, chars);
        const distance = ld(find, $([...str]).sort()) - ignoreChars;
        if (distance <= 0) {
          const closest = $closest($(element), selector, rootParents);
          debugLog("success", "Found similar text: " + $search, closest);
          if (closest) {
            win();
            hideElement(closest);
            break;
          }
        }
      }
    }
  };

  let mo = new MutationObserver(callback);
  let win = raceWinner(
    "hide-if-contains-similar-text",
    () => mo.disconnect()
  );
  mo.observe(document, {childList: true, characterData: true, subtree: true});
  callback();
}
