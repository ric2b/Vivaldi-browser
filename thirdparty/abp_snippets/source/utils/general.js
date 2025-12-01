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

let {Array, Math, RegExp} = $(window);

/**
 * Escapes regular expression special characters in a string.
 *
 * The returned string may be passed to the `RegExp` constructor to match the
 * original string.
 *
 * @param {string} string The string in which to escape special characters.
 *
 * @returns {string} A new string with the special characters escaped.
 * @private
 */
function regexEscape(string) {
  return $(string).replace(/[-/\\^$*+?.()|[\]{}]/g, "\\$&");
}

/**
 * Converts a given pattern to a regular expression.
 *
 * @param {string} pattern The pattern to convert. If the pattern begins and
 *   ends with a slash (`/`), the text in between is treated as a regular
 *   expression. If the pattern begins with a slash (`/`) and it ends with a
 *   (`/i`), the text in between is treated as case insensitive regular
 *   expression; otherwise the pattern is treated as raw text.
 *
 * @returns {RegExp} A `RegExp` object based on the given pattern.
 * @private
 */
export function toRegExp(pattern) {
  let {length} = pattern;

  // regexp in /.../ slashes must have at least length of 2
  if (length > 1 && pattern[0] === "/") {
    let isCaseSensitive = pattern[length - 1] === "/";
    // if not case sensitive, ensure it's not the string "/i" itself
    if (isCaseSensitive || (length > 2 && $(pattern).endsWith("/i"))) {
      let args = [$(pattern).slice(1, isCaseSensitive ? -1 : -2)];
      if (!isCaseSensitive)
        args.push("i");

      return new RegExp(...args);
    }
  }

  return new RegExp(regexEscape(pattern));
}

/**
 * Generates a random alphanumeric ID consisting of 6 base-36 digits
 * from the range 100000..zzzzzz (both inclusive).
 *
 * @returns {string} The random ID.
 * @private
 */
export function randomId() {
  // 2176782336 is 36^6 which mean 6 chars [a-z0-9]
  // 60466176 is 36^5
  // 2176782336 - 60466176 = 2116316160. This ensure to always have 6
  // chars even if Math.random() returns its minimum value 0.0
  //
  return $(Math.floor(Math.random() * 2116316160 + 60466176)).toString(36);
}

/**
 * Formats the given arguments by wrapping each argument in single quotes
 * and joining them into a single string.
 * This is done for logging purposes, so that each snippet will print
 * the list of arguments when it runs successfully.
 *
 * @param {IArguments | Array} args - The arguments object or an array of
 *  arguments to format.
 * @returns {string} A string with each argument wrapped in single quotes.
 */
export function formatArguments(args) {
  return $(Array.from(args)).map(arg => `'${arg}'`).join(" ");
}

/**
 * Converts a number to its hexadecimal representation.
 *
 * @param {number} number The number to convert.
 * @param {number} [length] The <em>minimum</em> length of the hexadecimal
 *   representation. For example, given the number `1024` and the length `8`,
 *   the function returns the value `"00000400"`.
 *
 * @returns {string} The hexadecimal representation of the given number.
 * @private
 */
export function toHex(number, length = 2) {
  let hex = $(number).toString(16);

  if (hex.length < length)
    hex = $("0").repeat(length - hex.length) + hex;

  return hex;
}

/**
 * Converts a `Uint8Array` object into its hexadecimal representation.
 *
 * @param {Uint8Array} uint8Array The `Uint8Array` object to convert.
 *
 * @returns {string} The hexadecimal representation of the given `Uint8Array`
 *   object.
 * @private
 */
export function uint8ArrayToHex(uint8Array) {
  return uint8Array.reduce((hex, byte) => hex + toHex(byte), "");
}
