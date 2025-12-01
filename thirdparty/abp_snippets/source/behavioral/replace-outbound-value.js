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

import {getDebugger} from "../introspection/log.js";
import {formatArguments, toRegExp} from "../utils/general.js";
import {profile} from "../introspection/profile.js";
import {matchesStackTrace} from "../utils/execution.js";

const {Error, Object, atob, btoa, RegExp} = $(window);

/**
 * Replaces values returned by native methods.
 * Traps the specified method and replaces text
 * in its return value or object properties.
 * @alias module:content/snippets.replace-outbound-value
 *
 * @param {string} methodPath The method to trap
 * (e.g., "document.querySelector.textContent")
 * @param {string} textToReplace The text or regex pattern to replace
 * @param {?string} [replacement=""] The replacement text
 * @param {?string} [decodeMethod=""] The decode method.
 * Currently only "base64" is supported
 * @param {?string} [path=""] Dot-separated path to the property
 * in returned objects
 * (e.g., "user.profile.name"). If empty, applies to string return values.
 * @param {?string} [stack=""] Comma-separated list of strings to check in the
 * stack trace. If provided, the replacement will only apply when the stack
 * trace contains at least one of these patterns.
 */
export function replaceOutboundValue(methodPath, textToReplace = "",
                                     replacement = "", decodeMethod = "",
                                     path = "", stack = "") {
  if (!methodPath)
    throw new Error("[replace-outbound-value snippet]: Missing method path.");

  let debugLog = getDebugger("replace-outbound-value");
  const {mark, end} = profile("replace-outbound-value");
  const formattedArgsToLog = formatArguments(arguments);

  /**
   * Gets a property by traversing a path on an object.
   * @param {Object} base - The base object to start from
   * @param {string} propertyPath - The property path
   * (e.g., "document.createElement")
   * @return {Object} Object containing base, prop, and success indicator
   */
  function getPropertyInChain(base, propertyPath) {
    let object = base;
    let chain = $(propertyPath).split(".");

    for (let i = 0; i < chain.length - 1; i++) {
      let prop = chain[i];
      if (!object || typeof object !== "object") {
        return {
          base: object,
          prop,
          remainingPath: chain.slice(i).join("."),
          success: false
        };
      }
      object = object[prop];
    }

    let prop = chain[chain.length - 1];
    return {
      base: object,
      prop,
      success: true
    };
  }

  /**
   * Checks if a string is a valid base64 encoded string.
   * If after decoding and encoding the string is not the same as the
   * original string, then the string is not a valid base64 encoded string.
   *
   * @param {string} str - The string to be checked.
   * @return {boolean} A boolean indicating whether the string is a valid
   * base64 encoded string.
   */
  function isValidBase64(str) {
    try {
      if (str === "")
        return false;

      const decodedString = atob(str);
      const encodedString = btoa(decodedString);
      // Encoded string may contains padding characters, so it's necessary
      // to remove it before comparison
      const stringWithoutPadding = $(str).replace(/=+$/, "").toString();
      const encodedStringWithoutPadding = $(encodedString).replace(/=+$/, "").toString();
      return encodedStringWithoutPadding === stringWithoutPadding;
    }
    catch (e) {
      return false;
    }
  }

  /**
   * Handles bidirectional base64 operations and content replacement.
   * If content is base64 encoded: decodes → modifies → re-encodes
   * If content is plain text: modifies → encodes to base64
   * If decode method is not specified, content is modified without encoding.
   *
   * @param {string} content - The original content to be processed.
   * @param {RegExp} pattern - The regular expression pattern to match.
   * @param {string} textReplacement - The text to replace the matched pattern.
   * @param {string} decode - The method used to process the content.
   * For now only supported value is 'base64'.
   * @return {string} The content after processing.
   */
  function decodeAndReplaceContent(content, pattern, textReplacement, decode) {
    switch (decode) {
      case "base64":
        try {
          const isBase64Encoded = isValidBase64(content);

          if (isBase64Encoded) {
            // Content is base64 encoded: decode → modify → re-encode
            const decodedContent = atob(content);
            debugLog("info", `Decoded base64 content: ${decodedContent}`);

            const modifiedContent = pattern ?
              $(decodedContent).replace(pattern, textReplacement).toString() :
              decodedContent;

            const message = modifiedContent !== decodedContent ?
              `Modified decoded content: ${modifiedContent}` :
              "Decoded content was not modified";

            debugLog("info", message);

            const encodedContent = btoa(modifiedContent);
            debugLog("info", `Re-encoded to base64: ${encodedContent}`);
            return encodedContent;
          }
          // Content is plain text: modify → encode to base64
          debugLog("info", `Content is plain text: ${content}`);

          const modifiedContent = pattern ?
            $(content).replace(pattern, textReplacement).toString() :
            content;

          const message = modifiedContent !== content ?
            `Modified plain text content: ${modifiedContent}` :
            "Plain text content was not modified";

          debugLog("info", message);

          const encodedContent = btoa(modifiedContent);
          debugLog("info", `Encoded to base64: ${encodedContent}`);
          return encodedContent;
        }
        catch (e) {
          debugLog("info", `Error processing base64 content: ${e.message}`);
          return content;
        }
      default:
        return pattern ?
          $(content).replace(pattern, textReplacement).toString() :
          content;
    }
  }

  /**
   * Replaces a value at a specific path in an object
   * @param {Object} obj - The object to modify
   * @param {string[]} pathSegments - Array of property names forming the path
   * @param {RegExp} pattern - The regex pattern to match
   * @param {string} textReplacement - The replacement text
   * @param {string} decode - The decoding method (e.g., 'base64')
   * @return {Object} The modified object
   */
  function replaceValueAtPath(obj, pathSegments, pattern, textReplacement,
                              decode) {
    if (!pathSegments.length)
      return obj;

    let current = obj;
    const parents = [];

    for (let i = 0; i < pathSegments.length - 1; i++) {
      if (!current || typeof current !== "object") {
        debugLog("info", `Cannot navigate to path: property '${pathSegments[i]}' not found`);
        return obj;
      }
      parents.push(current);
      current = current[pathSegments[i]];
    }

    const finalProp = pathSegments[pathSegments.length - 1];
    if (!current || typeof current !== "object" || !(finalProp in current)) {
      debugLog("info", `Target property '${finalProp}' not found at path`);
      return obj;
    }

    const originalValue = current[finalProp];
    if (typeof originalValue !== "string") {
      debugLog("info", `Property at path is not a string: ${typeof originalValue}`);
      return obj;
    }

    const modifiedValue = decodeAndReplaceContent(
      originalValue, pattern, textReplacement, decode);

    if (modifiedValue !== originalValue) {
      // Create a deep copy and modify the target property
      const result = JSON.parse(JSON.stringify(obj));
      let resultCurrent = result;

      for (let i = 0; i < pathSegments.length - 1; i++)
        resultCurrent = resultCurrent[pathSegments[i]];
      resultCurrent[finalProp] = modifiedValue;

      debugLog("info", `Replaced value at path '${pathSegments.join(".")}': '${originalValue}' -> '${modifiedValue}'`);
      return result;
    }

    return obj;
  }

  /**
   * Processes a return value (either direct return or resolved Promise value)
   * and applies the appropriate modifications based on type and path
   * @param {any} returnValue - The value to process
   * @param {string[]} pathParts - Array of property names forming the path
   * @param {string} textPattern - The text pattern to replace
   * @param {string} replaceWith - The replacement text
   * @param {string} decode - The decoding method (e.g., 'base64')
   * @param {string} formattedArgs - Formatted arguments for logging
   * @return {any} The processed value
   */
  function processReturnValue(returnValue, pathParts, textPattern, replaceWith,
                              decode, formattedArgs) {
    // Handle object with path navigation
    const patternRegexp = textPattern ? new RegExp(toRegExp(textPattern), "g") :
     null;
    if (pathParts.length && typeof returnValue === "object" &&
        returnValue !== null) {
      const modifiedObject = textPattern ?
        replaceValueAtPath(
          returnValue,
          pathParts,
          patternRegexp,
          replaceWith,
          decode
        ) :
        returnValue;

      if (modifiedObject !== returnValue) {
        debugLog("success",
               `Replaced outbound value\nFILTER: replace-outbound-value ${formattedArgs}`);
      }

      return modifiedObject;
    }
    // Handle string content
    else if (typeof returnValue === "string") {
      if (!textPattern)
        debugLog("info", `Original text content: ${returnValue}`);

      const modifiedContent = textPattern ?
        decodeAndReplaceContent(
          returnValue,
          patternRegexp,
          replaceWith,
          decode
        ) :
        returnValue;

      if (modifiedContent !== returnValue) {
        debugLog("success",
               `Replaced outbound value: ${modifiedContent} \nFILTER: replace-outbound-value ${formattedArgs}`);
      }

      return modifiedContent;
    }

    // Handle other types
    debugLog("info", pathParts.length ?
      "Content is not an object or path not specified" :
      "Content is not a string");
    return returnValue;
  }

  mark();

  const result = getPropertyInChain(window, methodPath);

  if (!result.success) {
    debugLog("error", `Could not reach the end of the prop chain: ${methodPath}. Remaining path: ${result.remainingPath}`);
    end();
    return;
  }

  const {base, prop} = result;
  const nativeMethod = base[prop];
  if (!nativeMethod || typeof nativeMethod !== "function") {
    debugLog("error", `Could not retrieve the method: ${methodPath}`);
    end();
    return;
  }

  let pathSegments = [];
  if (path)
    pathSegments = $(path).split(".");

  let stackNeedles = [];
  if (stack)
    stackNeedles = $(stack).split(",").map(s => s.trim());

  // This flag allows to prevent infinite loops when trapping props
  // that are used by snippet's own code.
  let isMatchingSuspended = false;

  Object.defineProperty(base, prop, {
    value: proxy(nativeMethod, function() {
      if (isMatchingSuspended)
        return apply(nativeMethod, this, arguments);

      isMatchingSuspended = true;
      const methodResult = apply(nativeMethod, this, arguments);

      if (stackNeedles.length && !matchesStackTrace(stackNeedles, debugLog)) {
        isMatchingSuspended = false;
        return methodResult;
      }

      // Handle Promise returns
      if (methodResult && typeof methodResult.then === "function") {
        debugLog("info", "Method returned a Promise, modifying resolved value");

        isMatchingSuspended = false;
        return methodResult.then(resolvedValue => {
          const valueType = typeof resolvedValue === "object" ?
            JSON.stringify(resolvedValue) : resolvedValue;
          debugLog("info", `Promise resolved with value: ${valueType}`);

          // Apply the same logic as below but to the resolved value
          return processReturnValue(
            resolvedValue,
            pathSegments,
            textToReplace,
            replacement,
            decodeMethod,
            path,
            debugLog,
            formattedArgsToLog
          );
        }).catch(error => {
          debugLog("info", `Promise rejected: ${error.message}`);
          throw error;
        });
      }

      // Handle synchronous returns (non-Promise)
      const processedResult = processReturnValue(
        methodResult,
        pathSegments,
        textToReplace,
        replacement,
        decodeMethod,
        path,
        debugLog,
        formattedArgsToLog
      );
      isMatchingSuspended = false;
      return processedResult;
    })
  });

  debugLog("info", `Wrapped ${methodPath}`);
  end();
}
