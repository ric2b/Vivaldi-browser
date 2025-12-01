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
import {formatArguments, toRegExp} from "../utils/general.js";
import {profile} from "../introspection/profile.js";
import {matchesStackTrace} from "../utils/execution.js";

const {Error, Object, Map} = $(window);

// Contains all the values to override after the snippet is used at least once
let mapValues = null;

/**
 * Checks if a value matches the given regex pattern
 * @param {*} val - The value to check
 * @param {RegExp} needle - The regex pattern to match against
 * @param {string[]} pathSegments - Parsed path to look for the needle
 * @return {boolean} True if a match is found
 */
function isMatchingValue(val, needle, pathSegments) {
  // If no path is specified, check the value directly
  if (!pathSegments.length) {
    if (typeof val === "string" || typeof val === "number") {
      const valStr = val.toString();
      return needle.test(valStr);
    }
    return false;
  }

  // Navigate through the object based on the path
  let current = val;
  for (const segment of pathSegments) {
    // If current is null, or the property doesn't exist, return false
    if (!current || !hasOwnProperty(current, segment))
      return false;
    current = current[segment];
  }

  // Check if the value at the path matches the regex
  if (typeof current === "string" || typeof current === "number") {
    const currStr = current.toString();
    return needle.test(currStr);
  }

  return false;
}

/**
 * Traps calls to Map.prototype functions. If the needle matches
 * the parameter to the function call, the snippet changes the behaviour
 * of the function to ignore that call or return another value.
 * @alias module:content/snippets.map-override
 *
 * @param {string} method The Map function to override.
 * Possible values to override the property with:
 *   set
 *   get
 *   has
 * @param {string} needle The string or regex used to determine
 * which function calls to trap. For 'set', this matches the value.
 * For 'get' and 'has', this matches the key.
 * @param {?string} [returnValue=""] The return value for the matched
 * function calls. For 'set', ignored. For 'get', the value to return.
 * For 'has', "true" or "false".
 * @param {?string} path Path to look for the needle (only for 'set' method
 * when checking object values)
 * @param {?string} stack Comma-separated list of strings to check in the
 * stack trace. If provided, the override will only apply when the stack
 * trace contains at least one of these patterns.
 */
export function mapOverride(method, needle, returnValue = "", path,
                            stack) {
  if (!method)
    throw new Error("[map-override snippet]: Missing method to override.");

  if (!needle)
    throw new Error("[map-override snippet]: Missing needle.");

  if (!mapValues)
    mapValues = new Map();

  let debugLog = getDebugger("map-override");
  const {mark, end} = profile("map-override");
  const formattedArgsToLog = formatArguments(arguments);

  // Map.prototype.set
  if (method === "set" && !mapValues.has("set")) {
    mark();
    const {set} = Map.prototype;
    mapValues.set("set", $([]));

    Object.defineProperty(window.Map.prototype, "set", {
      value: proxy(set, function(key, val) {
        const overrideVals = mapValues.get("set");
        for (const {needleRegex, pathSegments, stackNeedles} of overrideVals) {
          // Check if we should ignore this value
          if (isMatchingValue(val, needleRegex, pathSegments) &&
              matchesStackTrace(stackNeedles, debugLog)) {
            debugLog("success", `Map.set is ignored for value matching needle: ${needleRegex}\nFILTER: map-override ${formattedArgsToLog}`);
            return this; // Return the Map instance as per spec
          }
        }
        return apply(set, this, arguments);
      })
    });
    debugLog("info", "Wrapped Map.prototype.set");
    end();
  }
  // Map.prototype.get
  else if (method === "get" && !mapValues.has("get")) {
    mark();
    const {get} = Map.prototype;
    mapValues.set("get", $([]));

    Object.defineProperty(window.Map.prototype, "get", {
      value: proxy(get, function(key) {
        const overrideVals = mapValues.get("get");
        for (const {needleRegex, retVal, stackNeedles} of overrideVals) {
          // Check if the key matches
          if (typeof key === "string" || typeof key === "number") {
            const keyStr = key.toString();
            if (needleRegex.test(keyStr) &&
                matchesStackTrace(stackNeedles, debugLog)) {
              debugLog("success", `Map.get returned ${retVal} for key: ${keyStr}\nFILTER: map-override ${formattedArgsToLog}`);
              return retVal;
            }
          }
        }
        return apply(get, this, arguments);
      })
    });
    debugLog("info", "Wrapped Map.prototype.get");
    end();
  }
  // Map.prototype.has
  else if (method === "has" && !mapValues.has("has")) {
    mark();
    const {has} = Map.prototype;
    mapValues.set("has", $([]));

    Object.defineProperty(window.Map.prototype, "has", {
      value: proxy(has, function(key) {
        const overrideVals = mapValues.get("has");
        for (const {needleRegex, retVal, stackNeedles} of overrideVals) {
          // Check if the key matches
          if (typeof key === "string" || typeof key === "number") {
            const keyStr = key.toString();
            if (needleRegex.test(keyStr) &&
                matchesStackTrace(stackNeedles, debugLog)) {
              debugLog("success", `Map.has returned ${retVal} for key: ${keyStr}\nFILTER: map-override ${formattedArgsToLog}`);
              return retVal;
            }
          }
        }
        return apply(has, this, arguments);
      })
    });
    debugLog("info", "Wrapped Map.prototype.has");
    end();
  }

  const needleRegex = toRegExp(needle);
  let pathSegments = [];
  if (path)
    pathSegments = path.split(".");

  // Parse stack trace needles
  let stackNeedles = [];
  if (stack)
    stackNeedles = stack.split(",").map(s => s.trim());

  const overrideVals = mapValues.get(method);

  // Parse return value based on method
  let retVal;
  if (method === "get") {
    // For get, return the specified value or undefined
    retVal = returnValue === "" ? void 0 : returnValue;
  }
  else if (method === "has") {
    // For has, return boolean
    retVal = returnValue === "true";
  }

  overrideVals.push({needleRegex, retVal, pathSegments, stackNeedles});
  mapValues.set(method, overrideVals);
}
