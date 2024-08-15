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
import {call} from "proxy-pants/function";

import {overrideOnError, wrapPropertyAccess} from "../utils/execution.js";
import {randomId, toRegExp} from "../utils/general.js";
import {getDebugger} from "../introspection/log.js";

let {HTMLScriptElement, Object, ReferenceError} = $(window);
let Script = Object.getPrototypeOf(HTMLScriptElement);

/**
 * Aborts the execution of an inline script.
 * @alias module:content/snippets.abort-current-inline-script
 *
 * @param {string} api API function or property name to anchor on.
 * @param {?string} [search] If specified, only scripts containing the given
 *   string are prevented from executing. If the string begins and ends with a
 *   slash (`/`), the text in between is treated as a regular expression.
 *
 * @since Adblock Plus 3.4.3
 */
export function abortCurrentInlineScript(api, search = null) {
  const debugLog = getDebugger("abort-current-inline-script");
  const re = search ? toRegExp(search) : null;

  const rid = randomId();
  const us = $(document).currentScript;

  let object = window;
  const path = $(api).split(".");
  const name = $(path).pop();

  for (let node of $(path)) {
    object = object[node];
    if (
      !object || !(typeof object == "object" || typeof object == "function")) {
      debugLog("warn", path, " is not found");
      return;
    }
  }

  // Get original getter and setter so we can access them if this call
  // is not to be aborted
  const {get: prevGetter, set: prevSetter} =
    Object.getOwnPropertyDescriptor(object, name) || {};

  let currentValue = object[name];
  if (typeof currentValue === "undefined")
    debugLog("warn", "The property", name, "doesn't exist yet. Check typos.");

  const abort = () => {
    const element = $(document).currentScript;
    if (element instanceof Script &&
        $(element, "HTMLScriptElement").src == "" &&
        element != us &&
        (!re || re.test($(element).textContent))) {
      debugLog("success", path, " is aborted \n", element);
      throw new ReferenceError(rid);
    }
  };

  const descriptor = {
    get() {
      abort();

      if (prevGetter)
        return call(prevGetter, this);

      return currentValue;
    },
    set(value) {
      abort();

      if (prevSetter)
        call(prevSetter, this, value);
      else
        currentValue = value;
    }
  };

  wrapPropertyAccess(object, name, descriptor);

  overrideOnError(rid);
}
