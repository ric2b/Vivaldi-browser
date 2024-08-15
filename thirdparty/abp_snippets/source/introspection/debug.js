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

/**
 * Whether debug mode is enabled.
 * @type {boolean}
 * @private
 */
let debugging = false;

/**
 * Tells if the debug mode is inactive.
 * @memberOf module:content/snippets.debug
 * @returns {boolean}
 */
export function debug() {
  return debugging;
}

/**
 * Enables debug mode.
 * @alias module:content/snippets.debug
 *
 * @example
 * example.com#$#debug; log 'Hello, world!'
 *
 * @since Adblock Plus 3.8
 */
export function setDebug() {
  debugging = true;
}
