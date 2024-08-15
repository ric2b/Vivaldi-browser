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
import {apply, caller, proxy} from "proxy-pants/function";
import {getDebugger} from "../introspection/log.js";
import {toRegExp} from "../utils/general.js";

let {URL, fetch} = $(window);

// purposely a trap for the native URLSearchParams.prototype
let {delete: deleteParam, has: hasParam} = caller(URLSearchParams.prototype);

let parameters;

/**
 * Strips a query string parameter from `fetch()` calls.
 * @alias module:content/snippets.strip-fetch-query-parameter
 *
 * @param {string} name The name of the parameter.
 * @param {?string} [urlPattern] An optional pattern that the URL must match.
 *
 * @since Adblock Plus 3.5.1
 */
export function stripFetchQueryParameter(name, urlPattern = null) {
  const debugLog = getDebugger("strip-fetch-query-parameter");
  // override the `window.fetch` only once
  if (!parameters) {
    parameters = new Map();
    window.fetch = proxy(fetch, (...args) => {
      let [source] = args;
      if (typeof source === "string") {
        let url = new URL(source);
        for (let [key, reg] of parameters) {
          if (!reg || reg.test(source)) {
            if (hasParam(url.searchParams, key)) {
              debugLog("success", `${key} has been stripped from url ${source}`);
              deleteParam(url.searchParams, key);
              args[0] = url.href;
            }
          }
        }
      }
      return apply(fetch, self, args);
    });
  }
  parameters.set(name, urlPattern && toRegExp(urlPattern));
}
