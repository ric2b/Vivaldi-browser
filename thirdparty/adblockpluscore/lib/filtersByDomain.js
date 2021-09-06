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
 * Map to be used instead when a filter has a blank `domains` property.
 * @type {Map.<string, boolean>}
 */
let defaultDomains = new Map([["", true]]);

let FilterMap =
/**
 * A `FilterMap` object contains a set of filters, each mapped to a boolean
 * value indicating whether the filter should be applied.
 *
 * It is used by
 * `{@link module:filtersByDomain.FiltersByDomain FiltersByDomain}`.
 *
 * @package
 */
exports.FilterMap = class FilterMap
{
  /**
   * Creates a `FilterMap` object.
   * @param {?Array.<Array>} [entries] The initial entries in the object.
   * @see #entries
   * @private
   */
  constructor(entries)
  {
    this._map = new Map(entries);
  }

  /**
   * Returns the number of filters in the object.
   * @returns {number}
   */
  get size()
  {
    return this._map.size;
  }

  /**
   * Yields all the filters in the object along with a boolean value for each
   * filter indicating whether the filter should be applied.
   *
   * @returns {object} An iterator that yields a two-tuple containing an
   *   `{@link module:filterClasses.ActiveFilter ActiveFilter}` object and a
   *   `boolean` value.
   */
  entries()
  {
    return this._map.entries();
  }

  /**
   * Yields all the filters in the object.
   *
   * @returns {object} An iterator that yields an
   *   `{@link module:filterClasses.ActiveFilter ActiveFilter}` object.
   */
  keys()
  {
    return this._map.keys();
  }

  /**
   * Returns a boolean value indicating whether the filter referenced by the
   * given key should be applied.
   *
   * @param {module:filterClasses.ActiveFilter} key The filter.
   *
   * @returns {boolean|undefined} Whether the filter should be applied. If the
   *   object does not contain the filter referenced by `key`, returns
   *   `undefined`.
   */
  get(key)
  {
    return this._map.get(key);
  }

  /**
   * Sets the boolean value for the filter referenced by the given key
   * indicating whether the filter should be applied.
   *
   * @param {module:filterClasses.ActiveFilter} key The filter.
   * @param {boolean} value The boolean value.
   */
  set(key, value)
  {
    this._map.set(key, value);
  }

  /**
   * Removes the filter referenced by the given key.
   *
   * @param {module:filterClasses.ActiveFilter} key The filter.
   */
  delete(key)
  {
    this._map.delete(key);
  }
};

/**
 * A `FiltersByDomain` object contains a set of domains, each mapped to a set
 * of filters along with a boolean value for each filter indicating whether the
 * filter should be applied to the domain.
 *
 * @package
 */
exports.FiltersByDomain = class FiltersByDomain
{
  /**
   * Creates a `FiltersByDomain` object.
   * @param {?Array.<Array>} [entries] The initial entries in the object.
   * @see #entries
   */
  constructor(entries)
  {
    this._map = new Map(entries);
  }

  /**
   * Returns the number of domains in the object.
   * @returns {number}
   */
  get size()
  {
    return this._map.size;
  }

  /**
   * Yields all the domains in the object along with a set of filters for each
   * domain, each filter in turn mapped to a boolean value indicating whether
   * the filter should be applied to the domain.
   *
   * @returns {object} An iterator that yields a two-tuple containing a
   *   `string` and either a
   *   `{@link module:filtersByDomain.FilterMap FilterMap}` object
   *   or a single `{@link module:filterClasses.ActiveFilter ActiveFilter}`
   *   object. In the latter case, it directly indicates that the filter
   *   should be applied.
   */
  entries()
  {
    return this._map.entries();
  }

  /**
   * Returns a boolean value asserting whether the object contains the domain
   * referenced by the given key.
   *
   * @param {string} key The domain.
   *
   * @returns {boolean} Whether the object contains the domain referenced by
   *   `key`.
   */
  has(key)
  {
    return this._map.has(key);
  }

  /**
   * Returns a set of filters for the domain referenced by the given key, along
   * with a boolean value for each filter indicating whether the filter should
   * be applied to the domain.
   *
   * @param {string} key The domain.
   *
   * @returns {module:filtersByDomain.FilterMap|
   *   module:filterClasses.ActiveFilter|undefined} Either a
   *   `{@link module:filtersByDomain.FilterMap FilterMap}` object or a single
   *   `{@link module:filterClasses.ActiveFilter ActiveFilter}` object. In the
   *   latter case, it directly indicates that the filter should be applied.
   *   If this `FiltersByDomain` object does not contain the domain referenced
   *   by `key`, the return value is `undefined`.
   */
  get(key)
  {
    return this._map.get(key);
  }

  /**
   * Removes all the domains in the object.
   */
  clear()
  {
    this._map.clear();
  }

  /**
   * Adds a filter and all of its domains to the object.
   *
   * @param {module:filterClasses.ActiveFilter} filter The filter.
   * @param {Map.<string, boolean>} [domains] The filter's domains. If not
   *   given, the `{@link module:filterClasses.ActiveFilter#domains domains}`
   *   property of `filter` is used.
   */
  add(filter, domains = filter.domains)
  {
    for (let [domain, include] of domains || defaultDomains)
    {
      if (!include && domain == "")
        continue;

      let map = this._map.get(domain);
      if (!map)
      {
        this._map.set(domain, include ? filter :
                                new FilterMap([[filter, false]]));
      }
      else if (map.size == 1 && !(map instanceof FilterMap))
      {
        if (filter != map)
        {
          this._map.set(domain, new FilterMap([[map, true],
                                               [filter, include]]));
        }
      }
      else
      {
        map.set(filter, include);
      }
    }
  }

  /**
   * Removes a filter and all of its domains from the object.
   *
   * @param {module:filterClasses.ActiveFilter} filter The filter.
   * @param {Map.<string, boolean>} [domains] The filter's domains. If not
   *   given, the `{@link module:filterClasses.ActiveFilter#domains domains}`
   *   property of `filter` is used.
   */
  remove(filter, domains = filter.domains)
  {
    for (let domain of (domains || defaultDomains).keys())
    {
      let map = this._map.get(domain);
      if (map)
      {
        if (map.size > 1 || map instanceof FilterMap)
        {
          map.delete(filter);

          if (map.size == 0)
          {
            this._map.delete(domain);
          }
          else if (map.size == 1)
          {
            for (let [lastFilter, include] of map.entries())
            {
              // Reduce Map { "example.com" => Map { filter => true } } to
              // Map { "example.com" => filter }
              if (include)
                this._map.set(domain, lastFilter);

              break;
            }
          }
        }
        else if (filter == map)
        {
          this._map.delete(domain);
        }
      }
    }
  }
};
