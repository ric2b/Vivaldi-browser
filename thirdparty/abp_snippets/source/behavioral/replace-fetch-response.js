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
import {addPostFetchCallback} from "../utils/fetchManipulation.js";

let {Map, Object, RegExp, Response} = $(window);
let fetchRules;

/**
 * Replaces the response of a fetch request if the response text matches
 * a given regular expression pattern.
 *
 * @param {string} search - The string or regex pattern to match
 * in the response text.
 * @param {?string} [replacement=""] - The string to replace the matched
 * pattern with.
 * @param {?string} [needle=null] - An optional string or regex to check in the
 * response text before applying the replacement.
 *
 * @example
 * // Replaces "Hello" with "Hi" in the response text of all fetch requests.
 *    replaceFetchResponse(/Hello/g, "Hi");
 *
 * @example
 * // Replaces "Hello" with "Hi" in the response text of fetch requests
 * only if the response text includes "Greeting".
 *    replaceFetchResponse(/Hello/g, "Hi", "Greeting");
 */

export function replaceFetchResponse(search, replacement = "", needle = null) {
  const formattedArgsToLog = formatArguments(arguments);
  const debugLog = getDebugger("replace-fetch-response");
  const {mark, end} = profile("replace-fetch-response");
  if (!search) {
    debugLog("error", "The parameter 'search' is required");
    return;
  }

  if (!fetchRules) {
    const mainLogic = origResponse => {
      mark();
      const clonedResponse = $(origResponse).clone();
      return clonedResponse.text().then(origText => {
        let replacedText = $(origText);
        // eslint-disable-next-line max-len
        for (const [thisSearch, {replacement: thisReplacement, needle: thisNeedle, formattedArgs}] of fetchRules) {
          if (thisNeedle) {
            const needleRegex = toRegExp(thisNeedle);
            // Needle found
            if (needleRegex.test(replacedText)) {
              if (debug()) {
                console.groupCollapsed(`DEBUG [replace-fetch-response] success: '${thisNeedle}' found in fetch response`);
                debugLog("info", `${replacedText}`);
                console.groupEnd();
              }
            }
            else {
              if (debug()) {
                console.groupCollapsed(`DEBUG [replace-fetch-response] warn: '${thisNeedle}' not found in fetch response`);
                debugLog("warn", `${replacedText}`);
                console.groupEnd();
              }
              continue;
            }
          }
          replacedText = replacedText.replace(thisSearch, thisReplacement);
          if (debug() && replacedText.toString() !== origText.toString()) {
            console.groupCollapsed(`DEBUG [replace-fetch-response] success: '${thisSearch}' replaced with '${thisReplacement}' in fetch response`,
              `\nFILTER: replace-fetch-response ${formattedArgs}`
            );
            debugLog("success", `${replacedText}`);
            console.groupEnd();
          }
        }
        // check if the response text was modified
        if (replacedText.toString() === origText.toString())
          return origResponse;

        const replacedResponse = new Response(replacedText.toString(), {
          status: origResponse.status,
          statusText: origResponse.statusText,
          headers: origResponse.headers
        });
        Object.defineProperties(replacedResponse, {
          ok: {value: origResponse.ok},
          redirected: {value: origResponse.redirected},
          type: {value: origResponse.type},
          url: {value: origResponse.url}
        });
        end();
        return replacedResponse;
      });
    };

    fetchRules = new Map();
    debugLog("info", "Network API proxied");
    addPostFetchCallback(mainLogic);
  }

  const regex = toRegExp(search);
  // replaceAll is not supported in older browsers, this simulates it.
  const globalisedRegEx = new RegExp(regex, "g");
  fetchRules.set(globalisedRegEx,
                 {replacement, needle, formattedArgs: formattedArgsToLog});
}

