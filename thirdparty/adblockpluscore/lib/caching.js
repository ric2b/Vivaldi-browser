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

/**
 * A `Cache` object represents a cache of arbitrary data.
 * @package
 */
exports.Cache = class Cache
{
  /**
   * Creates a cache.
   * @param {number} capacity The maximum number of entries that can exist in
   *   the cache.
   */
  constructor(capacity)
  {
    // Note: This check works for non-number values.
    if (!(capacity >= 1))
      throw new Error("capacity must be a positive number.");

    this._capacity = capacity;
    this._storage = new Map();
  }

  /**
   * Reads an entry from the cache.
   * @param {?*} key The key for the entry.
   * @returns {?*} The value of the entry, or `undefined` if the entry doesn't
   *   exist in the cache.
   */
  get(key)
  {
    return this._storage.get(key);
  }

  /**
   * Writes an entry to the cache.
   *
   * If the cache has reached the specified maximum number of entries, all the
   * old entries are cleared first.
   *
   * @param {?*} key The key for the entry.
   * @param {?*} value The value of the entry.
   */
  set(key, value)
  {
    // To prevent logical errors, neither key nor value is allowed to be
    // undefined.
    if (typeof key == "undefined")
      throw new Error("key must not be undefined.");

    if (typeof value == "undefined")
      throw new Error("value must not be undefined.");

    if (this._storage.size == this._capacity && !this._storage.has(key))
      this._storage.clear();

    this._storage.set(key, value);
  }

  /**
   * Clears the cache.
   */
  clear()
  {
    this._storage.clear();
  }
};
