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
import {apply, proxy} from "proxy-pants/function";

import {toRegExp} from "../utils/general.js";
import {hideElement} from "../utils/dom.js";
import {raceWinner} from "../introspection/race.js";
import {getDebugger} from "../introspection/log.js";

const {Map, MutationObserver, Object, Set, WeakSet} = $(window);

let ElementProto = Element.prototype;
let {attachShadow} = ElementProto;

let hiddenShadowRoots = new WeakSet();
let searches = new Map();
let observer = null;

/**
 * Hides any HTML element or one of its ancestors matching a CSS selector if
 * the text content of the element's shadow contains a given string.
 * @alias module:content/snippets.hide-if-shadow-contains
 *
 * @param {string} search The string to look for in every HTML element's
 *   shadow. If the string begins and ends with a slash (`/`), the text in
 *   between is treated as a regular expression.
 * @param {string} selector The CSS selector that an HTML element must match
 *   for it to be hidden.
 *
 * @since Adblock Plus 3.3
 */
export function hideIfShadowContains(search, selector = "*") {
  // Add new searches only if needed, accordingly with the selector.
  let key = `${search}\\${selector}`;
  if (!searches.has(key)) {
    searches.set(key, [toRegExp(search), selector, raceWinner(
      "hide-if-shadow-contains",
      () => {
        searches.delete(key);
      })
    ]);
  }

  const debugLog = getDebugger("hide-if-shadow-contain");

  // Bootstrap the observer and the proxied attachShadow wrap once.
  if (!observer) {
    observer = new MutationObserver(records => {
      let visited = new Set();
      for (let {target} of $(records)) {
        // retrieve the ShadowRoot
        let parent = $(target).parentNode;
        while (parent)
          [target, parent] = [parent, $(target).parentNode];

        // avoid checking hidden shadow roots
        if (hiddenShadowRoots.has(target))
          continue;

        // avoid checking twice the same shadow root node
        if (visited.has(target))
          continue;

        visited.add(target);
        for (let [re, selfOrParent, win] of searches.values()) {
          if (re.test($(target).textContent)) {
            let closest = $(target.host).closest(selfOrParent);
            if (closest) {
              win();
              // always hide the host of shadow-root
              $(target).appendChild(
                document.createElement("style")
              ).textContent = ":host {display: none !important}";
              // hide desired element, which is the host itself
              // or one of its ancestors
              hideElement(closest);
              // mark shadow root as hidden
              hiddenShadowRoots.add(target);
              debugLog("success",
                       "Hiding: ",
                       closest,
                       " for params: ",
                       ...arguments);
            }
          }
        }
      }
    });

    Object.defineProperty(ElementProto, "attachShadow", {
      // Proxy whatever was set as attachShadow
      value: proxy(attachShadow, function() {
        // Create the shadow root first. It doesn't matter if it's a closed
        // shadow root, we keep the reference in a weak map.
        let root = apply(attachShadow, this, arguments);
        debugLog("info", "attachShadow is called for: ", root);

        // Listen for relevant DOM mutations in the shadow.
        observer.observe(root, {
          childList: true,
          characterData: true,
          subtree: true
        });

        return root;
      })
    });
  }
}
