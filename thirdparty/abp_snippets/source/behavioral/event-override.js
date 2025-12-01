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

import {formatArguments, toRegExp} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";

const {Map, Object, Reflect, WeakMap} = $(window);

const originalAddEventListener = window.EventTarget.prototype.addEventListener;
const originalRemoveEventListener = window.EventTarget.
                                    prototype.removeEventListener;

const listenerMap = new WeakMap();
let eventOverrides = [];

/**
 * Overrides events by changing their properties or disabling them.
 * @alias module:content/snippets.array-override
 *
 * @param {string} eventType The event type that to be targeted.
 * Example: click, mouseover...
 * @param {string} mode The mode of the operation. Accepts: trusted, disable.
 * 'trusted' mode makes the event property isTrusted true.
 * 'disable' mode disables the matching event.
 * @param {?string} needle Needle to look for in the event listener function
 *
 */
export function eventOverride(eventType,
                              mode,
                              needle = null) {
  const formattedArgs = formatArguments(arguments);
  const overrideConfig = {
    eventType,
    mode,
    needle: needle ? toRegExp(needle) : null,
    formattedArgs
  };

  if (!eventOverrides.includes(overrideConfig))
    eventOverrides.push(overrideConfig);

  if (eventOverrides.length > 1)
    return;

  let debugLog = getDebugger("[event-override]");
  const {mark, end} = profile("event-override");

  const addEventListenerDescriptor = Object.getOwnPropertyDescriptor(
    window.EventTarget.prototype,
    "addEventListener"
  );

  if (addEventListenerDescriptor.configurable) {
    Object.defineProperty(window.EventTarget.prototype, "addEventListener", {
      ...addEventListenerDescriptor,
      value: proxy(
        originalAddEventListener,
        function(type, listener, options) {
          mark();

          const filteredEvents = eventOverrides.filter(
            ev => ev.eventType === type
          );

          if (!filteredEvents.length || type !== filteredEvents[0].eventType) {
            end();
            return apply(originalAddEventListener, this, arguments);
          }

          const disabledEvent = filteredEvents.find(
            ev =>
              (ev.mode === "disable") &&
              (ev.needle ? ev.needle.test(listener.toString()) : true)
          );

          if (disabledEvent) {
            debugLog("success", `Disabling ${disabledEvent.eventType} event, \nFILTER: event-override ${disabledEvent.formattedArgs}`);
            end();
            return;
          }

          const changedEvents = filteredEvents.filter(
            ev =>
              (ev.mode === "trusted") &&
              (ev.needle ? ev.needle.test(listener.toString()) : true)
          );

          if (typeof listener !== "function" &&
              !(listener && typeof listener.handleEvent === "function") ||
              !changedEvents.length || type !== changedEvents[0].eventType) {
            end();
            return apply(originalAddEventListener, this, arguments);
          }

          const wrappedListener = function(originalEvent) {
            const customEvent = new Proxy(originalEvent, {
              get(target, prop) {
                if (prop === "isTrusted") {
                  debugLog("success", `Providing trusted value for ${originalEvent.type} event`);
                  return true;
                }

                const val = Reflect.get(target, prop);

                if (typeof val === "function") {
                  return function(...args) {
                    return apply(val, target, args);
                  };
                }

                return val;
              }
            });

            if (typeof listener === "function")
              return call(listener, this, customEvent);

            return call(listener.handleEvent, listener, customEvent);
          };

          wrappedListener.originalListener = listener;

          if (!listenerMap.has(listener))
            listenerMap.set(listener, new Map());

          listenerMap.get(listener).set(type, wrappedListener);
          debugLog("info", `\nWrapping event listener for ${type}`);

          end();
          return apply(
            originalAddEventListener,
            this,
            [type, wrappedListener, options]
          );
        })
    });
  }

  // Patch removeEventListener to handle wrapped listeners
  const removeEventListenerDescriptor = Object.getOwnPropertyDescriptor(
    window.EventTarget.prototype,
    "removeEventListener"
  );
  if (removeEventListenerDescriptor.configurable) {
    Object.defineProperty(window.EventTarget.prototype, "removeEventListener", {
      ...removeEventListenerDescriptor,
      value: proxy(
        originalRemoveEventListener,
        function(type, listener, options) {
          if (listener &&
            listenerMap.has(listener) && listenerMap.get(listener).has(type)) {
            const wrappedListener = listenerMap.get(listener).get(type);
            listenerMap.get(listener).delete(type);
            return apply(
              originalRemoveEventListener,
              this,
              [type, wrappedListener, options]
            );
          }

          return apply(originalRemoveEventListener, this, arguments);
        })
    });
  }

  debugLog("info", "Initialized event-override snippet");
}
