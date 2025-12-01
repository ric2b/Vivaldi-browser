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
import {hasOwnProperty} from "proxy-pants/object";

import {getDebugger} from "../introspection/log.js";
import {profile} from "../introspection/profile.js";
import {formatArguments, toRegExp} from "../utils/general.js";
import {findOwner, overrideValue} from "../utils/execution.js";

const {Array, Error, JSON, Map, Object, Response} = $(window);

// will be a Map of all paths, once the snippet is used at least once
let paths = null;

/**
 * Traps calls to JSON.parse, and if the result of the parsing is an Object, it
 * will replace specified properties from the result before returning to the
 * caller.
 * @alias module:content/snippets.json-override
 *
 * @param {string} rawOverridePaths A list of space-separated properties
 * to replace. Can include placeholders {} and [] to iterate over nested
 * objects and arrays respectively.
 * @param {string} value The value to override the properties with.
 * Possible values to override the property with:
 *   undefined
 *   false
 *   true
 *   null
 *   noopFunc    - function with empty body
 *   trueFunc    - function returning true
 *   falseFunc   - function returning false
 *   ''          - empty string
 *   positive decimal integer, no sign, with maximum value of 0x7FFF
 *   emptyArray  - an array with no elements
 *   emptyObject - an object with no properties
 *
 * @param {?string} [rawNeedlePaths] A list of space-separated properties which
 *   must be all present for the pruning to occur.
 * @param {?string} [filter] A string to look for in the raw string,
 * before it's passed to JSON.parse.
 * If no match is found no further search is done on the resulting object.
 * If the string begins and ends with a slash (/),
 * the text in between is treated as a regular expression.
 *
 * @since Adblock Plus 3.11.2
 */
export function jsonOverride(rawOverridePaths, value,
                             rawNeedlePaths = "", filter = "") {
  if (!rawOverridePaths)
    throw new Error("[json-override snippet]: Missing paths to override.");

  if (typeof value == "undefined")
    throw new Error("[json-override snippet]: No value to override with.");

  if (!paths) {
    let debugLog = getDebugger("json-override");
    const {mark, end} = profile("json-override");
    mark();

    function overrideObject(obj, str) {
      // eslint-disable-next-line max-len
      for (let {formattedArgs, prune, needle, filter: flt, value: val} of paths.values()) {
        if (flt && !flt.test(str))
          continue;

        if ($(needle).some(path => !findOwner(obj, path)))
          return obj;

        for (let path of prune) {
          if (path.includes("{}") || path.includes("[]"))
            overridePathWithPlaceholders(obj, path, val, formattedArgs);
          else
            overridePathSimple(obj, path, val, formattedArgs);
        }
      }
      return obj;
    }

    function overridePathWithPlaceholders(obj, path, newValue, formattedArgs) {
      let pathParts = $(path).split(".");
      let currentObj = obj;

      for (let i = 0; i < pathParts.length; i++) {
        let part = pathParts[i];

        if (part === "[]") {
          // Handle arrays
          if (Array.isArray(currentObj)) {
            debugLog("info", `Iterating over array at: ${part}`);
            $(currentObj).forEach(item => {
              if (item !== null && typeof item !== "undefined") {
                overridePathWithPlaceholders(item,
                                             pathParts.slice(i + 1).join("."),
                                             newValue,
                                             formattedArgs);
              }
            });
          }
          return;
        }
        else if (part === "{}") {
          // Handle objects
          if (currentObj && typeof currentObj === "object") {
            debugLog("info", `Iterating over object at: ${part}`);
            Object.keys(currentObj).forEach(key => {
              let nextItem = currentObj[key];
              if (nextItem !== null && typeof nextItem !== "undefined") {
                overridePathWithPlaceholders(nextItem,
                                             pathParts.slice(i + 1).join("."),
                                             newValue,
                                             formattedArgs);
              }
            });
          }
          return;
        }
        else if (currentObj && typeof currentObj === "object" &&
          hasOwnProperty(currentObj, part)) {
          // Standard property replacement case
          if (i === pathParts.length - 1) {
            debugLog("success", `Found ${path}, replaced it with ${newValue}`, `\nFILTER: json-override ${formattedArgs}`);
            currentObj[part] = overrideValue(newValue);
          }
          else {
            currentObj = currentObj[part];
          }
        }
        else {
          return;
        }
      }
    }

    function overridePathSimple(obj, path, newValue, formattedArgs) {
      let details = findOwner(obj, path);
      if (typeof details != "undefined") {
        debugLog("success", `Found ${path}, replaced it with ${newValue}`, `\nFILTER: json-override ${formattedArgs}`);
        details[0][details[1]] = overrideValue(newValue);
      }
    }

    // allow both jsonPrune and jsonOverride to work together
    let {parse} = JSON;
    paths = new Map();

    Object.defineProperty(window.JSON, "parse", {
      value: proxy(parse, function(str) {
        let result = apply(parse, this, arguments);
        return overrideObject(result, str);
      })
    });
    debugLog("info", "Wrapped JSON.parse for override");

    let {json} = Response.prototype;
    Object.defineProperty(window.Response.prototype, "json", {
      value: proxy(json, function(str) {
        let resultPromise = apply(json, this, arguments);
        return resultPromise.then(obj => overrideObject(obj, str));
      })
    });
    debugLog("info", "Wrapped Response.json for override");
    end();
  }

  const formattedArgsToLog = formatArguments(arguments);
  // allow a single unique rawOverridePaths definition per domain
  // TBD: should we throw an error if it was already set?
  paths.set(rawOverridePaths, {
    formattedArgs: formattedArgsToLog,
    prune: $(rawOverridePaths).split(/ +/),
    needle: rawNeedlePaths.length ? $(rawNeedlePaths).split(/ +/) : [],
    filter: filter ? toRegExp(filter) : null,
    value
  });
}
