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
 * @typedef {object} Environment
 * @property {Array.<Array>} debugCSSProperties Highlighting options.
 * CSS properties to be applied to the targeted element.
 * @property {bool} debugJS Javascript debugging.
 * Enables/disables JS Devtools debugging
 * @property {string} world Target injection world. 'ISOLATED' or 'MAIN'.
 */

/**
 * A configuration object passed by integrators.
 * @type {Environment}
 * @private
 */
// eslint-disable-next-line no-undef
export const libEnvironment = typeof environment !== "undefined" ? environment :
                                                                   {};
