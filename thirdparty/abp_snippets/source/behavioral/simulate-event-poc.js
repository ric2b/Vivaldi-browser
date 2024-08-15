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

import {initQueryAndApply} from "../utils/dom.js";

let {
  parseInt,
  setTimeout,
  Error,
  MouseEvent,
  MutationObserver,
  WeakSet
} = $(window);

/**
 * Simulate a mouse event on the page.
 * @alias module:content/snippets.simulate-event-poc
 *
 * @param {string} event Pattern that matches the type(s) of event
 * we want to prevent. If the string starts and ends with a slash (`/`),
 * the text in between is treated as a regular expression.
 * @param {string} selector The CSS/Xpath selector that an HTML element must
 * match for the event to be triggered.
 * @param {?string} delay The delay between the moment when the node is inserted
 * and the moment when the event is dispatched.
 *
 * @since Adblock Plus 3.11.2
 */
export function simulateEvent(event, selector, delay = "0") {
  if (!event)
    throw new Error("[simulate-event snippet]: No event type provided.");
  if (!selector)
    throw new Error("[simulate-event snippet]: No selector provided.");

  let queryAndApply = initQueryAndApply(selector);
  let delayInMiliseconds = parseInt(delay, 10);
  let dispatchedNodes = new WeakSet();

  let observer = new MutationObserver(findNodesAndDispatchEvents);
  observer.observe(document, {childList: true, subtree: true});
  findNodesAndDispatchEvents();

  function findNodesAndDispatchEvents() {
    queryAndApply(node => {
      if (!dispatchedNodes.has(node)) {
        dispatchedNodes.add(node);
        setTimeout(() => {
          $(node).dispatchEvent(
            new MouseEvent(event, {bubbles: true, cancelable: true})
          );
        }, delayInMiliseconds);
      }
    });
  }
}
