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

import {$$, $closest, hideElement, isVisible} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";
import {toRegExp} from "../utils/general.js";

let {getComputedStyle, MutationObserver, WeakSet} = $(window);

/**
 * Hides any HTML element that uses an `aria-labelledby`, or one of its
 * ancestors, if the related aria element contains the searched text.
 * @alias module:content/snippets.hide-if-labelled-by
 *
 * @param {string} search The string to look for in HTML elements. If the
 *   string begins and ends with a slash (`/`), the text in between is treated
 *   as a regular expression.
 * @param {string} selector The CSS selector of an HTML element that uses as
 *   `aria-labelledby` attribute.
 * @param {?string} [searchSelector] The CSS selector of an ancestor of the
 *   HTML element that uses as `aria-labelledby` attribute. Defaults to the
 *   value of the `selector` argument.
 *
 * @since Adblock Plus 3.9
 */
export function hideIfLabelledBy(search, selector, searchSelector = null) {
  let sameSelector = searchSelector == null;

  let searchRegExp = toRegExp(search);

  let matched = new WeakSet();

  let callback = () => {
    for (const {element, rootParents} of $$(selector, true)) {
      let closest = sameSelector ?
                    element :
                    $closest($(element), searchSelector, rootParents);
      if (!closest ||
          !isVisible(element, getComputedStyle(element), closest))
        continue;

      let attr = $(element).getAttribute("aria-labelledby");
      let fallback = () => {
        if (matched.has(closest))
          return;

        if (searchRegExp.test(
          $(element).getAttribute("aria-label") || ""
        )) {
          win();
          matched.add(closest);
          hideElement(closest);
        }
      };

      if (attr) {
        for (let label of $(attr).split(/\s+/)) {
          let target = $(document).getElementById(label);
          if (target) {
            if (!matched.has(target) && searchRegExp.test(target.innerText)) {
              win();
              matched.add(target);
              hideElement(closest);
            }
          }
          else {
            fallback();
          }
        }
      }
      else {
        fallback();
      }
    }
  };

  let mo = new MutationObserver(callback);
  let win = raceWinner(
    "hide-if-labelled-by",
    () => mo.disconnect()
  );
  mo.observe(document, {characterData: true, childList: true, subtree: true});
  callback();
}
