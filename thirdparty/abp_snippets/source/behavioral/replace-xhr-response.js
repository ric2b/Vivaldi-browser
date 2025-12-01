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
import {debug} from "../introspection/debug.js";
import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";
import {formatArguments, toRegExp} from "../utils/general.js";

let {RegExp, XMLHttpRequest, WeakMap} = $(window);
let xhrInFlightRequests;
let xhrRules;

/**
 * Replaces the response of XMLHttpRequest (XHR) requests
 * if the response text matches a given string.
 *
 * @param {string} search - The string or regex pattern to match
 * 	in the response text.
 * @param {?string} [replacement=""] - The string to replace
 * 	the matched pattern with.
 * @param {?string} [needle=null] - An optional string
 * representing properties to match in the XMLHttpRequest details.
 *
 * @example
 * // Replaces "Hello" with "Hi" in the response text of all XHR requests.
 * replaceXhrResponse(/Hello/, "Hi");
 *
 * @example
 * // Replaces "Hello" with "Hi" in the response text of XHR requests
 * only if the URL includes "example.com".
 * replaceXhrResponse(/Hello/, "Hi", "url:example.com");
 */
export function replaceXhrResponse(search, replacement = "", needle = null) {
  const formattedArgsToLog = formatArguments(arguments);
  const debugLog = getDebugger("replace-xhr-response");
  const {mark, end} = profile("replace-xhr-response");

  if (!search) {
    debugLog("error", "The parameter 'pattern' is required");
    return;
  }

  // Override the XMLHttpRequest class only once
  if (!xhrInFlightRequests) {
    xhrInFlightRequests = new WeakMap();
    xhrRules = new Map();
    debugLog("info", "XMLHttpRequest proxied");
    // Intercept XMLHttpRequest open method
    window.XMLHttpRequest = class extends XMLHttpRequest {
      open(method, url, ...args) {
        const originalXhr = this;
        const xhrData = {method, url};
        xhrInFlightRequests.set(originalXhr, xhrData);
        return super.open(method, url, ...args);
      }
      /* In some websites it has been observed that importing XMLHttpRequest
       from $(window) might make the send method read-only.
       Here we override it without changing its behavior.
       This prevents the method from becoming read-only.
      */
      send(...args) {
        return super.send(...args);
      }
      get response() {
        const innerResponse = super.response;
        const xhrData = xhrInFlightRequests.get(this);
        if (typeof xhrData === "undefined")
          return innerResponse;
        mark();
        // Optimization: if response wasn't changed since last time,
        // return the cached response
        const responseLength = typeof innerResponse === "string" ?
          innerResponse.length : void 0;
        if (xhrData.lastResponseLength !== responseLength) {
          xhrData.response = void 0;
          xhrData.lastResponseLength = responseLength;
        }

        // If we have a cached transformed response, return it
        if (typeof xhrData.response !== "undefined")
          return xhrData.response;

        // If the response is not a string we can't replace anything,
        // return it untransformed
        if (typeof innerResponse !== "string")
          return (xhrData.response = innerResponse);

        let replacedText = innerResponse;
        // eslint-disable-next-line max-len
        for (const [thisSearch, {replacement: thisReplacement, needle: thisNeedle, formattedArgs}] of xhrRules) {
          if (thisNeedle) {
            const needleRegex = toRegExp(thisNeedle);
            // Needle found
            if (needleRegex.test(replacedText)) {
              if (debug()) {
                console.groupCollapsed(`DEBUG [replace-xhr-response] success: '${thisNeedle}' found in XHR response`);
                debugLog("info", replacedText);
                console.groupEnd();
              }
            }
            else {
              if (debug()) {
                console.groupCollapsed(`DEBUG [replace-xhr-response] warn: '${thisNeedle}' not found in XHR response`);
                debugLog("warn", replacedText);
                console.groupEnd();
              }
              continue;
            }
          }
          replacedText =
            $(replacedText).replace(thisSearch, thisReplacement).toString();
          if (debug() && innerResponse.toString() !== replacedText.toString()) {
            console.groupCollapsed(`DEBUG [replace-xhr-response] success: '${thisSearch}' replaced with '${thisReplacement}' in XHR response`,
                                    `\nFILTER: replace-xhr-response ${formattedArgs}`);
            debugLog("success", replacedText);
            console.groupEnd();
          }
        }
        end();
        return (xhrData.response = replacedText.toString());
      }
      get responseText() {
        const response = this.response;
        if (typeof response !== "string")
          return super.responseText;

        return response;
      }
    };
  }

  const regex = toRegExp(search);
  // replaceAll is not supported in older browsers, this simulates it.
  const globalisedRegEx = new RegExp(regex, "g");
  xhrRules.set(globalisedRegEx,
               {replacement, needle, formattedArgs: formattedArgsToLog});
}
