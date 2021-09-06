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

const {filterNotifier} = require("./filterNotifier");

/**
 * `{@link module:filterState.filterState filterState}` implementation.
 */
class FilterState
{
  /**
   * @private
   */
  constructor()
  {
    /**
     * Internal map containing filter state.
     * @type {Map.<string, object>}
     * @private
     */
    this._map = new Map();
  }

  _setEnabledWithState(filterText, enabled, state)
  {
    let oldEnabled = state ? !state.disabled : true;

    if (enabled)
    {
      if (state)
      {
        state.disabled = false;

        if (state.hitCount == 0 && state.lastHit == 0)
          this._map.delete(filterText);
      }
    }
    else if (state)
    {
      state.disabled = true;
    }
    else
    {
      this._map.set(filterText, {disabled: true, hitCount: 0, lastHit: 0});
    }

    if (enabled != oldEnabled)
    {
      filterNotifier.emit("filterState.enabled", filterText, enabled,
                          oldEnabled);
    }
  }

  /**
   * Checks whether a filter is enabled.
   * @param {string} filterText The text of the filter.
   * @returns {boolean} Whether the filter is enabled.
   */
  isEnabled(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (state)
      return !state.disabled;

    return true;
  }

  /**
   * Sets the enabled state of a filter.
   * @param {string} filterText The text of the filter.
   * @param {boolean} enabled The new enabled state of the filter.
   */
  setEnabled(filterText, enabled)
  {
    this._setEnabledWithState(filterText, enabled, this._map.size > 0 ?
                                                 this._map.get(filterText) :
                                                 void 0);
  }

  /**
   * Toggles the enabled state of a filter.
   * @param {string} filterText The text of the filter.
   */
  toggleEnabled(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;

    this._setEnabledWithState(filterText, state ? state.disabled : false,
                              state);
  }

  /**
   * Resets the enabled state of a filter.
   * @param {string} filterText The text of the filter.
   */
  resetEnabled(filterText)
  {
    this.setEnabled(filterText, true);
  }

  /**
   * Returns the hit count of a filter.
   * @param {string} filterText The text of the filter.
   * @returns {number} The hit count of the filter.
   */
  getHitCount(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (state)
      return state.hitCount;

    return 0;
  }

  /**
   * Sets the hit count of a filter.
   * @param {string} filterText The text of the filter.
   * @param {number} hitCount The new hit count of the filter.
   */
  setHitCount(filterText, hitCount)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (state)
    {
      let oldHitCount = state.hitCount;

      if (hitCount == 0 && !state.disabled && state.lastHit == 0)
        this._map.delete(filterText);
      else
        state.hitCount = hitCount;

      if (hitCount != oldHitCount)
      {
        filterNotifier.emit("filterState.hitCount", filterText, hitCount,
                            oldHitCount);
      }
    }
    else if (hitCount != 0)
    {
      this._map.set(filterText, {disabled: false, hitCount, lastHit: 0});

      filterNotifier.emit("filterState.hitCount", filterText, hitCount, 0);
    }
  }

  /**
   * Resets the hit count of a filter.
   * @param {string} filterText The text of the filter.
   */
  resetHitCount(filterText)
  {
    this.setHitCount(filterText, 0);
  }

  /**
   * Returns the last hit time of a filter.
   * @param {string} filterText The text of the filter.
   * @returns {number} The last hit time of the filter in milliseconds since
   *   the Unix epoch.
   */
  getLastHit(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (state)
      return state.lastHit;

    return 0;
  }

  /**
   * Sets the last hit time of a filter.
   * @param {string} filterText The text of the filter.
   * @param {number} lastHit The new last hit time of the filter in
   *   milliseconds since the Unix epoch.
   */
  setLastHit(filterText, lastHit)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (state)
    {
      let oldLastHit = state.lastHit;

      if (lastHit == 0 && !state.disabled && state.hitCount == 0)
        this._map.delete(filterText);
      else
        state.lastHit = lastHit;

      if (lastHit != oldLastHit)
      {
        filterNotifier.emit("filterState.lastHit", filterText, lastHit,
                            oldLastHit);
      }
    }
    else if (lastHit != 0)
    {
      this._map.set(filterText, {disabled: false, hitCount: 0, lastHit});

      filterNotifier.emit("filterState.lastHit", filterText, lastHit, 0);
    }
  }

  /**
   * Resets the last hit time of a filter.
   * @param {string} filterText The text of the filter.
   */
  resetLastHit(filterText)
  {
    this.setLastHit(filterText, 0);
  }

  /**
   * Registers a filter hit by incrementing the hit count of the filter and
   * setting the last hit time of the filter to the current time.
   * @param {string} filterText The text of the filter.
   */
  registerHit(filterText)
  {
    let now = Date.now();

    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (state)
    {
      let oldHitCount = state.hitCount;
      let oldLastHit = state.lastHit;

      state.hitCount++;
      state.lastHit = now;

      if (state.hitCount != oldHitCount)
      {
        filterNotifier.emit("filterState.hitCount", filterText, state.hitCount,
                            oldHitCount);
      }

      if (state.lastHit != oldLastHit)
      {
        filterNotifier.emit("filterState.lastHit", filterText, state.lastHit,
                            oldLastHit);
      }
    }
    else
    {
      this._map.set(filterText, {disabled: false, hitCount: 1, lastHit: now});

      filterNotifier.emit("filterState.hitCount", filterText, 1, 0);
      filterNotifier.emit("filterState.lastHit", filterText, now, 0);
    }
  }

  /**
   * Resets the hit count and last hit time of a filter.
   * @param {string} filterText The text of the filter.
   */
  resetHits(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (!state)
      return;

    let oldHitCount = state.hitCount;
    let oldLastHit = state.lastHit;

    if (state.disabled)
    {
      state.hitCount = 0;
      state.lastHit = 0;
    }
    else
    {
      this._map.delete(filterText);
    }

    if (oldHitCount != 0)
      filterNotifier.emit("filterState.hitCount", filterText, 0, oldHitCount);

    if (oldLastHit != 0)
      filterNotifier.emit("filterState.lastHit", filterText, 0, oldLastHit);
  }

  /**
   * Resets the enabled state, hit count, and last hit time of a filter.
   * @param {string} filterText The text of the filter.
   */
  reset(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (!state)
      return;

    let oldDisabled = state.disabled;
    let oldHitCount = state.hitCount;
    let oldLastHit = state.lastHit;

    this._map.delete(filterText);

    if (oldDisabled)
      filterNotifier.emit("filterState.disabled", filterText, false, true);

    if (oldHitCount != 0)
      filterNotifier.emit("filterState.hitCount", filterText, 0, oldHitCount);

    if (oldLastHit != 0)
      filterNotifier.emit("filterState.lastHit", filterText, 0, oldLastHit);
  }

  /**
   * Serializes the state of a filter.
   * @param {string} filterText The text of the filter.
   * @yields {string} The next line in the serialized representation of the
   *   state of the filter.
   * @see module:filterState~FilterState#fromObject
   * @package
   */
  *serialize(filterText)
  {
    let state = this._map.size > 0 ? this._map.get(filterText) : void 0;
    if (!state)
      return;

    yield "[Filter]";
    yield "text=" + filterText;

    if (state.disabled)
      yield "disabled=true";
    if (state.hitCount != 0)
      yield "hitCount=" + state.hitCount;
    if (state.lastHit != 0)
      yield "lastHit=" + state.lastHit;
  }

  /**
   * Reads the state of a filter from an object representation.
   * @param {string} filterText The text of the filter.
   * @param {object} object An object containing at least one of `disabled`,
   *   `hitCount`, and `lastHit` properties and their appropriate values.
   * @see module:filterState~FilterState#serialize
   * @package
   */
  fromObject(filterText, object)
  {
    if (!("disabled" in object || "hitCount" in object || "lastHit" in object))
      return;

    let state = {
      disabled: String(object.disabled) == "true",
      hitCount: parseInt(object.hitCount, 10) || 0,
      lastHit: parseInt(object.lastHit, 10) || 0
    };

    if (!state.disabled && state.hitCount == 0 && state.lastHit == 0)
      this._map.delete(filterText);
    else
      this._map.set(filterText, state);
  }
}

/**
 * Maintains filter state.
 * @type {module:filterState~FilterState}
 */
exports.filterState = new FilterState();
