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

import {initQueryAndApply, initQueryAll} from "../utils/dom.js";
import {getDebugger} from "../introspection/log.js";

let {
  parseInt,
  setTimeout,
  Error,
  MouseEvent,
  MutationObserver,
  WeakSet
} = $(window);

const VALID_TYPES = ["auxclick", "click", "dblclick",	"gotpointercapture",
                     "lostpointercapture", "mouseenter", "mousedown",
                     "mouseleave", "mousemove", "mouseout", "mouseover",
                     "mouseup",	"pointerdown", "pointerenter",
                     "pointermove", "pointerover", "pointerout",
                     "pointerup", "pointercancel", "pointerleave"];

/**
 * Simulate a mouse event on the page.
 * @alias module:content/snippets.simulate-mouse-event
 *
 * @param {string[]} selectors The CSS/Xpath selectors that an HTML element must
 * match for the event to be triggered.
 *
 * @since Adblock Plus 3.15.3
 */
export function simulateMouseEvent(...selectors) {
  const debugLog = getDebugger("simulate-mouse-event");
  const MAX_ARGS = 7;
  if (selectors.length < 1)
    throw new Error("[simulate-mouse-event snippet]: No selector provided.");
  if (selectors.length > MAX_ARGS) {
    // Truncate any parameters after
    selectors = selectors.slice(0, MAX_ARGS);
  }
  function parseArg(theRule) {
    if (!theRule)
      return null;

    // Default values for the rules
    const result = {
      selector: "",
      continue: false,
      trigger: false,
      event: "click",
      delay: "500",
      clicked: false,
      found: false
    };
    const textArr = theRule.split("$");
    let options = [];
    if (textArr.length >= 2)
      options = textArr[1].toLowerCase().split(",");

    [result.selector] = textArr;

    for (const option of options) {
      if (option === "trigger") {
        result.trigger = true;
      }
      else if (option === "continue") {
        result.continue = true;
      }
      else if (option.startsWith("event")) {
        const event = option.toLowerCase().split("=");
        event[1] ? result.event = event[1] : result.event = "click";
      }
      else if (option.startsWith("delay")) {
        const delay = option.toLowerCase().split("=");
        delay[1] ? result.delay = delay[1] : result.delay = "500";
      }
    }
    if (!VALID_TYPES.includes(result.event)) {
      debugLog("warn",
               result.event,
               " might be misspelled, check for typos.\n",
               "These are the supported events:",
               VALID_TYPES);
    }
    return result;
  }

  const parsedArgs = $([]);

  $(selectors).forEach(rule => {
    const parsedRule = parseArg(rule);
    parsedArgs.push(parsedRule);
  });

  function checkIfAllSelectorsFound() {
    parsedArgs.forEach(arg => {
      if (!arg.found) {
        const queryAll = initQueryAll(arg.selector);
        const elems = queryAll();
        if (elems.length > 0)
          arg.found = true;
      }
    });
    return parsedArgs.every(arg => arg.found);
  }

  function triggerEvent(node, event, delay) {
    // If the node is removed, or the function is called wrongly.
    if (!node || !event)
      return;

    if (event === "click" && node.click) {
      node.click();
      debugLog("success",
               "Clicked on this node:\n",
               node,
               "\nwith a delay of",
               delay,
               "ms"
      );
    }
    else {
      node.dispatchEvent(
        new MouseEvent(event, {bubbles: true, cancelable: true})
      );
      debugLog("success",
               "A",
               event,
               "event was dispatched with a delay of",
               delay,
               "ms on this node:\n",
               node
      );
    }
  }
  let allFound = false;
  // The event for the last selector should be triggered always.
  const [last] = parsedArgs.slice(-1);
  last.trigger = true;

  let dispatchedNodes = new WeakSet();

  let observer = new MutationObserver(findNodesAndDispatchEvents);
  observer.observe(document, {childList: true, subtree: true});
  findNodesAndDispatchEvents();

  function findNodesAndDispatchEvents() {
    // Check if all selectors are found
    if (!allFound)
      allFound = checkIfAllSelectorsFound();
    if (allFound) {
      for (const parsedRule of parsedArgs) {
        const queryAndApply = initQueryAndApply(parsedRule.selector);
        const delayInMiliseconds = parseInt(parsedRule.delay, 10);
        if (parsedRule.trigger) {
          queryAndApply(node => {
            if (!dispatchedNodes.has(node)) {
              dispatchedNodes.add(node);
              if (parsedRule.continue) {
                setInterval(() => {
                  triggerEvent(node, parsedRule.event, parsedRule.delay);
                }, delayInMiliseconds);
              }
              else {
                setTimeout(() => {
                  triggerEvent(node, parsedRule.event, parsedRule.delay);
                }, delayInMiliseconds);
              }
            }
          });
        }
      }
    }
  }
}
