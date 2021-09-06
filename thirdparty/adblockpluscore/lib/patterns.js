/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @module */

"use strict";

const {filterToRegExp} = require("./common");

/**
 * The maximum number of patterns that
 * `{@link module:patterns.compilePatterns compilePatterns()}` will compile
 * into regular expressions.
 * @type {number}
 */
const COMPILE_PATTERNS_MAX = 100;

/**
 * A `CompiledPatterns` object represents the compiled version of multiple URL
 * request patterns. It is returned by
 * `{@link module:patterns.compilePatterns compilePatterns()}`.
 */
class CompiledPatterns
{
  /**
   * Creates an object with the given regular expressions for case-sensitive
   * and case-insensitive matching respectively.
   * @param {?RegExp} [caseSensitive]
   * @param {?RegExp} [caseInsensitive]
   * @private
   */
  constructor(caseSensitive, caseInsensitive)
  {
    this._caseSensitive = caseSensitive;
    this._caseInsensitive = caseInsensitive;
  }

  /**
   * Tests whether the given URL request matches the patterns used to create
   * this object.
   * @param {module:url.URLRequest} request
   * @returns {boolean}
   */
  test(request)
  {
    return ((this._caseSensitive &&
             this._caseSensitive.test(request.href)) ||
            (this._caseInsensitive &&
             this._caseInsensitive.test(request.lowerCaseHref)));
  }
}

/**
 * Compiles patterns from the given filters into a single
 * `{@link module:patterns~CompiledPatterns CompiledPatterns}` object.
 *
 * @param {module:filterClasses.URLFilter|
 *         Set.<module:filterClasses.URLFilter>} filters
 *   The filters. If the number of filters exceeds
 *   `{@link module:patterns~COMPILE_PATTERNS_MAX COMPILE_PATTERNS_MAX}`, the
 *   function returns `null`.
 *
 * @returns {?module:patterns~CompiledPatterns}
 *
 * @package
 */
exports.compilePatterns = function compilePatterns(filters)
{
  // If the number of filters is too large, it may choke especially on low-end
  // platforms. As a precaution, we refuse to compile. Ideally we would check
  // the length of the regular expression source rather than the number of
  // filters, but this is far more straightforward and practical.
  if (filters.size > COMPILE_PATTERNS_MAX)
    return null;

  let caseSensitive = "";
  let caseInsensitive = "";

  for (let filter of filters)
  {
    let source = filter.pattern != null ? filterToRegExp(filter.pattern) :
                   filter.regexp.source;

    if (filter.matchCase)
      caseSensitive += source + "|";
    else
      caseInsensitive += source + "|";
  }

  let caseSensitiveRegExp = null;
  let caseInsensitiveRegExp = null;

  try
  {
    if (caseSensitive)
      caseSensitiveRegExp = new RegExp(caseSensitive.slice(0, -1));

    if (caseInsensitive)
      caseInsensitiveRegExp = new RegExp(caseInsensitive.slice(0, -1));
  }
  catch (error)
  {
    // It is possible in theory for the regular expression to be too large
    // despite COMPILE_PATTERNS_MAX
    return null;
  }

  return new CompiledPatterns(caseSensitiveRegExp, caseInsensitiveRegExp);
};
