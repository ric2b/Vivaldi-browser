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

import {findOwner} from "../utils/execution.js";
import {getDebugger} from "../introspection/log.js";

let {Error, JSON, Map, Object, Response} = $(window);

// will be a Map of all paths, once the snippet is used at least once
let paths = null;

/**
 * Traps calls to JSON.parse, and if the result of the parsing is an Object, it
 * will remove specified properties from the result before returning to the
 * caller.
 *
 * The idea originates from
 * [uBlock Origin](https://github.com/gorhill/uBlock/commit/2fd86a66).
 * @alias module:content/snippets.json-prune
 *
 * @param {string} rawPrunePaths A list of space-separated properties to remove.
 * @param {?string} [rawNeedlePaths] A list of space-separated properties which
 *   must be all present for the pruning to occur.
 *
 * @since Adblock Plus 3.9.0
 */
export function jsonPrune(rawPrunePaths, rawNeedlePaths = "") {
  if (!rawPrunePaths)
    throw new Error("Missing paths to prune");

  if (!paths) {
    let debugLog = getDebugger("json-prune");

    function pruneObject(obj) {
      for (let {prune, needle} of paths.values()) {
        if ($(needle).some(path => !findOwner(obj, path)))
          return obj;

        for (let path of prune) {
          let details = findOwner(obj, path);
          if (typeof details != "undefined") {
            debugLog("success", `Found ${path} and deleted`);
            delete details[0][details[1]];
          }
        }
      }
      return obj;
    }
    // allow both jsonPrune and jsonOverride to work together
    let {parse} = JSON;
    paths = new Map();

    Object.defineProperty(window.JSON, "parse", {
      value: proxy(parse, function() {
        let result = apply(parse, this, arguments);
        return pruneObject(result);
      })
    });
    debugLog("info", "Wrapped JSON.parse for prune");

    let {json} = Response.prototype;
    Object.defineProperty(window.Response.prototype, "json", {
      value: proxy(json, function() {
        let resultPromise = apply(json, this, arguments);
        return resultPromise.then(obj => pruneObject(obj));
      })
    });
    debugLog("info", "Wrapped Response.json for prune");
  }

  // allow a single unique rawPrunePaths definition per domain
  // TBD: should we throw an error if it was already set?
  paths.set(rawPrunePaths, {
    prune: $(rawPrunePaths).split(/ +/),
    needle: rawNeedlePaths.length ? $(rawNeedlePaths).split(/ +/) : []
  });
}
