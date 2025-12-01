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

import {caller} from "proxy-pants/function";
import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";
import {formatArguments, toRegExp} from "../utils/general.js";
import {addPreFetchCallback} from "../utils/fetchManipulation.js";

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
  const formattedArgs = formatArguments(arguments);
  const debugLog = getDebugger("strip-fetch-query-parameter");
  const {mark, end} = profile("strip-fetch-query-parameter");

  const stripFunction = url => {
    mark();
    for (let [key, value] of parameters.entries()) {
      const {reg, args} = value;
      if (!reg || reg.test(url)) {
        if (hasParam(url.searchParams, key)) {
          debugLog("success", `${key} has been stripped from url ${url}`, `\nFILTER: strip-fetch-query-parameter ${args}`);
          deleteParam(url.searchParams, key);
        }
      }
    }
    end();
  };

  if (!parameters) {
    parameters = new Map();
    addPreFetchCallback(stripFunction);
  }

  // Store the formatted arguments along with the regexp in the map
  parameters.set(name,
                 {reg: urlPattern && toRegExp(urlPattern),
                  args: formattedArgs});
}

