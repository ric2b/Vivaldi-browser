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
 * Registers and emits named events.
 */
exports.EventEmitter = class EventEmitter
{
  constructor()
  {
    this._listeners = new Map();
  }

  /**
   * Adds a listener for the specified event name.
   *
   * @param {string}   name
   * @param {function} listener
   */
  on(name, listener)
  {
    let listeners = this._listeners.get(name);
    if (listeners)
      listeners.push(listener);
    else
      this._listeners.set(name, [listener]);
  }

  /**
   * Removes a listener for the specified event name.
   *
   * @param {string}   name
   * @param {function} listener
   */
  off(name, listener)
  {
    let listeners = this._listeners.get(name);
    if (listeners)
    {
      if (listeners.length > 1)
      {
        let idx = listeners.indexOf(listener);
        if (idx != -1)
          listeners.splice(idx, 1);
      }
      else if (listeners[0] === listener)
      {
        // We must use strict equality above for compatibility with
        // Array.prototype.indexOf
        this._listeners.delete(name);
      }
    }
  }

  /**
   * Returns a copy of the array of listeners for the specified event.
   *
   * @param {string} name
   *
   * @returns {Array.<function>}
   */
  listeners(name)
  {
    let listeners = this._listeners.size > 0 ? this._listeners.get(name) : null;
    return listeners ? listeners.slice() : [];
  }

  /**
   * Checks whether there are any listeners for the specified event.
   *
   * @param {string} [name] The name of the event. If omitted, checks whether
   *   there are any listeners for any event.
   *
   * @returns {boolean}
   */
  hasListeners(name)
  {
    return this._listeners.size > 0 &&
           (typeof name == "undefined" || this._listeners.has(name));
  }

  /**
   * Calls all previously added listeners for the given event name.
   *
   * @param {string} name
   * @param {...*}   [args]
   */
  emit(name, ...args)
  {
    let listeners = this._listeners.size > 0 ? this._listeners.get(name) : null;
    if (listeners)
    {
      for (let listener of listeners.slice())
        listener(...args);
    }
  }
};
