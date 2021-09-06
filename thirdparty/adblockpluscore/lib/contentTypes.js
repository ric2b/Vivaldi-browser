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

let contentTypes =
/**
 * Content types for request blocking and allowing (allowlisting).
 *
 * There are two kinds of content types: resource types, and special
 * (non-resource) types.
 *
 * Resource types include web resources like scripts, images, and so on.
 *
 * Special types include filter options for popup blocking and CSP header
 * injection as well as flags for allowing documents.
 *
 * By default a filter matches any resource type, but if a filter has an
 * explicit resource type, special option, or allowing flag, like
 * `$script`, `$popup`, or `$elemhide`, then it matches only the given type,
 * option, or flag.
 *
 * @type {object}
 *
 * @public
 */
exports.contentTypes = {
  // Types of web resources.
  OTHER: 1,
  SCRIPT: 2,
  IMAGE: 4,
  STYLESHEET: 8,
  OBJECT: 16,
  SUBDOCUMENT: 32,
  WEBSOCKET: 128,
  WEBRTC: 256,
  PING: 1024,
  XMLHTTPREQUEST: 2048, // 1 << 11

  MEDIA: 16384,
  FONT: 32768, // 1 << 15

  // Special filter options.
  POPUP: 1 << 24,
  CSP: 1 << 25,

  // Allowing flags.
  DOCUMENT: 1 << 26,
  GENERICBLOCK: 1 << 27,
  ELEMHIDE: 1 << 28,
  GENERICHIDE: 1 << 29
};

// Backwards compatibility.
contentTypes.BACKGROUND = contentTypes.IMAGE;
contentTypes.XBL = contentTypes.OTHER;
contentTypes.DTD = contentTypes.OTHER;

const RESOURCE_TYPES =
/**
 * Bitmask for resource types like `$script`, `$image`, `$stylesheet`, and so
 * on.
 *
 * If a filter has no explicit content type, it applies to all resource types
 * (but not to any {@link module:contentTypes.SPECIAL_TYPES special types}).
 *
 * @const {number}
 *
 * @package
 */
// The first 24 bits are reserved for resource types like "script", "image",
// and so on.
// https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/ResourceType
exports.RESOURCE_TYPES = (1 << 24) - 1;

const SPECIAL_TYPES = ~RESOURCE_TYPES & (1 << 31) - 1;
/**
 * Bitmask for special "types" (options and flags) like `$csp`, `$elemhide`,
 * and so on.
 *
 * @const {number}
 *
 * @package
 */
exports.SPECIAL_TYPES = SPECIAL_TYPES;

/**
 * Bitmask for "types" (flags) that are for exception rules only, like
 * `$document`, `$elemhide`, and so on.
 *
 * @const {number}
 *
 * @package
 */
exports.ALLOWING_TYPES = contentTypes.DOCUMENT |
                         contentTypes.GENERICBLOCK |
                         contentTypes.ELEMHIDE |
                         contentTypes.GENERICHIDE;

/**
 * Yields individual types from a filter's type mask.
 *
 * @param {number} contentType A filter's type mask.
 * @param {number} [selection] Which types to yield.
 *
 * @generator
 * @yields {number}
 *
 * @package
 */
exports.enumerateTypes = function* enumerateTypes(contentType, selection = ~0)
{
  for (let mask = contentType & selection, bitIndex = 0;
       mask != 0; mask >>>= 1, bitIndex++)
  {
    if ((mask & 1) != 0)
    {
      // Note: The zero-fill right shift by zero is necessary for dropping the
      // sign.
      yield 1 << bitIndex >>> 0;
    }
  }
};
