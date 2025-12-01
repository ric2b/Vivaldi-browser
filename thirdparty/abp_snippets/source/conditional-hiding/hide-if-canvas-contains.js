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

import {hideElement} from "../utils/dom.js";
import {formatArguments, toRegExp} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";

const {CanvasRenderingContext2D,
       document,
       Map,
       MutationObserver,
       Object,
       Set,
       WeakSet} = $(window);

let canvasRules;
let pendingHideCanvasElements = new Set();
let hideCanvasSeenMap = new WeakSet();

/**
 * Hides a canvas element, or a parent element specified by the selector,
 * if the canvas contains the specified search term in its `fillText` or
 * `strokeText` calls.
 *
 * @param {string} search Regex pattern that will be searched for in `fillText`
 * or in `strokeText` calls.
 * @param {string} [selector="canvas"] - The CSS selector that identifies
 * the element to hide. This can be the canvas element itself or a parent
 * of the canvas. Defaults to the `<canvas>` element if not provided.
 *
 */
export function hideIfCanvasContains(search, selector = "canvas") {
  const debugLog = getDebugger("hide-if-canvas-contains");
  const formattedArgsToLog = formatArguments(arguments);
  const {mark, end} = profile("hide-if-canvas-contains");

  if (!search) {
    debugLog("error", "The parameter 'search' is required");
    return;
  }

  if (!canvasRules) {
    mark();
    const CanvasProto = CanvasRenderingContext2D.prototype;
    debugLog("info", "CanvasRenderingContext2D proxied");

    function overrideFunctionInCanvas(functionName){
      const originalFunction = CanvasProto[functionName];

      // eslint-disable-next-line max-len
      Object.defineProperty(window.CanvasRenderingContext2D.prototype, functionName, {
        value: proxy(originalFunction, function(text, ...args) {
          for (const [searchRegex, rule] of canvasRules) {
            if (searchRegex.test(text)) {
              const canvasElement = this.canvas;
              let elementToHide = $(canvasElement).closest(rule.selector);

              if (elementToHide && !hideCanvasSeenMap.has(elementToHide)) {
                hideElement(elementToHide);
                hideCanvasSeenMap.add(elementToHide);
                debugLog("success", "Matched: ", elementToHide, `\nFILTER: hide-if-canvas-contains ${rule.formattedArguments}`);
              }
              else {
                /* The canvas method (fillText or strokeText) has been called,
                  * but the canvas element is not yet attached to the DOM.
                  * Since the element's parent cant be found, the hiding action
                  * is deferred until the canvas is part of the DOM. */
                scheduleElementToHide(canvasElement, rule, functionName, text);
              }
            }
          }
          return apply(originalFunction, this, [text, ...args]);
        })
      });
    }

    overrideFunctionInCanvas("fillText");
    overrideFunctionInCanvas("strokeText");
    canvasRules = new Map();

    // Initialize a single MutationObserver
    const mo = new MutationObserver(mutationsList => {
      for (let mutation of $(mutationsList)) {
        if (mutation.type === "childList") {
          // Check all pending elements when DOM is mutated
          checkPendingElements();
        }
      }
    });

    // Start observing the document for additions
    mo.observe(document, {childList: true, subtree: true});
    end();
  }

  const searchRegex = toRegExp(search);
  // eslint-disable-next-line max-len
  canvasRules.set(searchRegex, {selector, formattedArguments: formattedArgsToLog});
}

function scheduleElementToHide(canvasElement, rule, functionName, text) {
  pendingHideCanvasElements.add({canvasElement, rule, functionName, text});
}

function checkPendingElements() {
  pendingHideCanvasElements.forEach(
    ({canvasElement, rule, functionName, text}) => {
      let elementToHide = $(canvasElement).closest(rule.selector);
      if (elementToHide && !hideCanvasSeenMap.has(elementToHide)) {
        hideElement(elementToHide);
        hideCanvasSeenMap.add(elementToHide);
        pendingHideCanvasElements.delete(
          {canvasElement, rule, functionName, text});
        getDebugger("hide-if-canvas-contains")("success", "Matched: ", elementToHide, `\nFILTER: hide-if-canvas-contains ${rule.formattedArguments}`);
      }
    });
}
