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
import {apply, call, proxy} from "proxy-pants/function";

import {debug} from "../introspection/debug.js";
import {toRegExp} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";

let {Error, Map, Object, console} = $(window);

let {toString} = Function.prototype;
let EventTargetProto = EventTarget.prototype;
let {addEventListener} = EventTargetProto;

// will be a Map of all events, once the snippet is used at least once
let events = null;

/**
 * Prevents adding event listeners.
 * @alias module:content/snippets.prevent-listener
 *
 * @param {string} event Pattern that matches the type(s) of event
 * we want to prevent. If the string starts and ends with a slash (`/`),
 * the text in between is treated as a regular expression.
 * @param {?string} eventHandler Pattern that matches the event handler's
 * declaration. If the string starts and ends with a slash (`/`),
 * the text in between is treated as a regular expression.
 * @param {?string} selector The CSS selector that the event target must match.
 * If the event target is not an HTML element the event handler is added.
 * @since Adblock Plus 3.11.2
 */
export function preventListener(event, eventHandler, selector) {
  if (!event)
    throw new Error("[prevent-listener snippet]: No event type.");

  if (!events) {
    events = new Map();

    let debugLog = getDebugger("[prevent]");

    Object.defineProperty(EventTargetProto, "addEventListener", {
      value: proxy(addEventListener, function(type, listener) {
        for (let {evt, handlers, selectors} of events.values()) {
          // bail out ASAP if current type doesn't match
          if (!evt.test(type))
            continue;

          let isElement = this instanceof Element;

          // check every possible handler and selector per same event type
          for (let i = 0; i < handlers.length; i++) {
            const handler = handlers[i];
            const sel = selectors[i];

            // If we have a selcetor and we don't match an element,
            // we don't prevent the event from being added.
            if (sel && !(isElement && $(this).matches(sel)))
              continue;

            if (handler) {
              const proxiedHandlerMatch = function() {
                try {
                  const proxiedHandlerString = call(
                    toString,
                    typeof listener === "function" ?
                      listener : listener.handleEvent
                  );
                  return handler.test(proxiedHandlerString);
                }
                catch (e) {
                  debugLog("error",
                           "Error while trying to stringify listener: ",
                           e);
                  return false;
                }
              };

              const actualHandlerMatch = function() {
                try {
                  const actualHandlerString = String(
                    typeof listener === "function" ?
                      listener : listener.handleEvent
                  );
                  return handler.test(actualHandlerString);
                }
                catch (e) {
                  debugLog("error",
                           "Error while trying to stringify listener: ",
                           e);
                  return false;
                }
              };

              // If an eventHandler is provided and we don't find a match,
              // we don't prevent the event from being added.
              if (!proxiedHandlerMatch() && !actualHandlerMatch())
                continue;
            }

            if (debug()) {
              console.groupCollapsed("DEBUG [prevent] was successful");
              debugLog("success", `type: ${type} matching ${evt}`);
              debugLog("success", "handler:", listener);
              if (handler)
                debugLog("success", `matching ${handler}`);
              if (sel)
                debugLog("success", "on element: ", this, ` matching ${sel}`);
              debugLog("success", "was prevented from being added");
              console.groupEnd();
            }
            return;
          }
        }
        return apply(addEventListener, this, arguments);
      })
    });

    debugLog("info", "Wrapped addEventListener");
  }

  if (!events.has(event))
    events.set(event, {evt: toRegExp(event), handlers: [], selectors: []});

  let {handlers, selectors} = events.get(event);

  handlers.push(eventHandler ? toRegExp(eventHandler) : null);
  selectors.push(selector);
}
