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
 * @fileOverview Synchronization between filter storage and the filter engine.
 */

const {filterStorage} = require("./filterStorage");
const {filterNotifier} = require("./filterNotifier");
const {isActiveFilter, Filter} = require("./filterClasses");
const {SpecialSubscription} = require("./subscriptionClasses");
const {filterState} = require("./filterState");

/**
 * Checks whether filters from a given subscription should be deployed to the
 * filter engine.
 *
 * If the subscription is both valid and enabled, the function returns `true`;
 * otherwise, it returns `false`.
 *
 * @param {module:subscriptionClasses.Subscription} subscription
 *   The subscription.
 *
 * @returns {boolean} Whether filters from the subscription should be deployed
 *   to the filter engine.
 */
function shouldDeployFilters(subscription)
{
  return subscription.valid && !subscription.disabled;
}

/**
 * Deploys a filter to the filter engine.
 *
 * The filter is deployed only if it belongs to at least one subscription that
 * is both valid and enabled.
 *
 * If the filter is a snippet filter, it is deployed only if it belongs to at
 * least one subscription that is valid, enabled, and of
 * {@link module:subscriptionClasses.Subscription#type type}
 * `circumvention` or a
 * {@link module:subscriptionClasses.SpecialSubscription special subscription}
 * that keeps user-defined filters.
 *
 * @param {module:filterEngine~FilterEngine} engine A reference to the
 *   filter engine.
 * @param {Filter} filter The filter.
 * @param {?Array.<module:subscriptionClasses.Subscription>} [subscriptions]
 *   A list of subscriptions to which the filter belongs. If omitted or `null`,
 *   the information is looked up from
 *   {@link module:filterStorage.filterStorage filter storage}.
 */
function deployFilter(engine, filter, subscriptions = null)
{
  if (!isActiveFilter(filter) || !filterState.isEnabled(filter.text))
    return;

  let deploy = false;
  let allowSnippets = false;

  for (let subscription of subscriptions ||
                           filterStorage.subscriptions(filter.text))
  {
    if (shouldDeployFilters(subscription))
    {
      deploy = true;

      // Allow snippets to be executed only by the circumvention lists or the
      // user's own filters.
      if (subscription.type == "circumvention" ||
          subscription instanceof SpecialSubscription)
      {
        allowSnippets = true;
        break;
      }
    }
  }

  if (!deploy)
    return;

  if (!allowSnippets && filter.type == "snippet")
    return;

  engine.add(filter);
}

/**
 * Undeploys a filter from the filter engine.
 *
 * The filter is undeployed only if it does not belong to at least one
 * subscription that is both valid and enabled.
 *
 * @param {module:filterEngine~FilterEngine} engine A reference to the
 *   filter engine.
 * @param {module:filterClasses.Filter} filter The filter.
 */
function undeployFilter(engine, filter)
{
  if (!isActiveFilter(filter))
    return;

  if (filterState.isEnabled(filter.text))
  {
    let keep = false;
    for (let subscription of filterStorage.subscriptions(filter.text))
    {
      if (shouldDeployFilters(subscription))
      {
        keep = true;
        break;
      }
    }

    if (keep)
      return;
  }

  engine.remove(filter);
}

/**
 * `{@link module:filterListener.filterListener filterListener}`
 * implementation.
 */
class FilterListener
{
  /**
   * Initializes filter listener on startup, registers the necessary hooks.
   *
   * Initialization is asynchronous; once complete,
   * `{@link module:filterNotifier.filterNotifier filterNotifier}` emits the
   * `ready` event.
   *
   * @hideconstructor
   */
  constructor()
  {
    /**
     * A reference to the filter engine.
     * @type {?module:filterEngine~FilterEngine}
     * @private
     */
    this._engine = null;

    /**
     * Increases on filter changes, filters will be saved if it exceeds 1.
     * @type {number}
     * @private
     */
    this._isDirty = 0;
  }

  /**
   * Initializes filter listener.
   * @param {module:filterEngine~FilterEngine} engine A reference to the
   *   filter engine.
   * @returns {Promise} A promise that is fulfilled when the initialization is
   *   complete.
   * @package
   */
  async initialize(engine)
  {
    if (engine == null || typeof engine != "object")
      throw new Error("engine must be a non-null object.");

    if (this._engine != null)
      throw new Error("Filter listener already initialized.");

    this._engine = engine;

    await filterStorage.loadFromDisk();

    let promise = Promise.resolve();

    // Initialize filters from each subscription asynchronously on startup by
    // setting up a chain of promises.
    for (let subscription of filterStorage.subscriptions())
    {
      if (shouldDeployFilters(subscription))
      {
        promise = promise.then(() =>
        {
          for (let text of subscription.filterText())
            deployFilter(this._engine, Filter.fromText(text), [subscription]);
        });
      }
    }

    await promise;

    filterNotifier.on("filter.added", this._onFilterAdded.bind(this));
    filterNotifier.on("filter.removed", this._onFilterRemoved.bind(this));
    filterNotifier.on("filter.moved", this._onGenericChange.bind(this));

    filterNotifier.on("filterState.enabled",
                      this._onFilterStateEnabled.bind(this));
    filterNotifier.on("filterState.hitCount",
                      this._onFilterStateHitCount.bind(this));
    filterNotifier.on("filterState.lastHit",
                      this._onFilterStateLastHit.bind(this));

    filterNotifier.on("subscription.added",
                      this._onSubscriptionAdded.bind(this));
    filterNotifier.on("subscription.removed",
                      this._onSubscriptionRemoved.bind(this));
    filterNotifier.on("subscription.disabled",
                      this._onSubscriptionDisabled.bind(this));
    filterNotifier.on("subscription.updated",
                      this._onSubscriptionUpdated.bind(this));
    filterNotifier.on("subscription.title", this._onGenericChange.bind(this));
    filterNotifier.on("subscription.fixedTitle",
                      this._onGenericChange.bind(this));
    filterNotifier.on("subscription.homepage",
                      this._onGenericChange.bind(this));
    filterNotifier.on("subscription.downloadStatus",
                      this._onGenericChange.bind(this));
    filterNotifier.on("subscription.lastCheck",
                      this._onGenericChange.bind(this));
    filterNotifier.on("subscription.errors",
                      this._onGenericChange.bind(this));

    filterNotifier.on("load", this._onLoad.bind(this));
    filterNotifier.on("save", this._onSave.bind(this));

    // Indicate that all filters are ready for use.
    filterNotifier.emit("ready");
  }

  /**
   * Increases "dirty factor" of the filters and calls
   * filterStorage.saveToDisk() if it becomes 1 or more.
   *
   * Save is executed delayed to prevent multiple subsequent calls. If the
   * parameter is 0 it forces saving filters if any changes were recorded after
   * the previous save.
   *
   * @param {number} factor
   *
   * @private
   */
  _setDirty(factor)
  {
    if (factor == 0 && this._isDirty > 0)
      this._isDirty = 1;
    else
      this._isDirty += factor;
    if (this._isDirty >= 1)
    {
      this._isDirty = 0;
      filterStorage.saveToDisk();
    }
  }

  _onSubscriptionAdded(subscription)
  {
    this._setDirty(1);

    if (shouldDeployFilters(subscription))
    {
      for (let text of subscription.filterText())
        deployFilter(this._engine, Filter.fromText(text), [subscription]);
    }
  }

  _onSubscriptionRemoved(subscription)
  {
    this._setDirty(1);

    if (shouldDeployFilters(subscription))
    {
      for (let text of subscription.filterText())
        undeployFilter(this._engine, Filter.fromText(text));
    }
  }

  _onSubscriptionDisabled(subscription, newValue)
  {
    this._setDirty(1);

    if (filterStorage.hasSubscription(subscription))
    {
      if (newValue == false)
      {
        for (let text of subscription.filterText())
          deployFilter(this._engine, Filter.fromText(text), [subscription]);
      }
      else
      {
        for (let text of subscription.filterText())
          undeployFilter(this._engine, Filter.fromText(text));
      }
    }
  }

  _onSubscriptionUpdated(subscription, textDelta)
  {
    this._setDirty(1);

    if (shouldDeployFilters(subscription) &&
        filterStorage.hasSubscription(subscription))
    {
      for (let text of textDelta.removed)
        undeployFilter(this._engine, Filter.fromText(text));

      for (let text of textDelta.added)
        deployFilter(this._engine, Filter.fromText(text), [subscription]);
    }
  }

  _onFilterAdded(filter)
  {
    this._setDirty(1);

    if (filterState.isEnabled(filter.text))
      deployFilter(this._engine, filter);
  }

  _onFilterRemoved(filter)
  {
    this._setDirty(1);

    if (filterState.isEnabled(filter.text))
      undeployFilter(this._engine, filter);
  }

  _onFilterStateEnabled(text, newValue)
  {
    this._setDirty(1);

    if (newValue == false)
      undeployFilter(this._engine, Filter.fromText(text));
    else
      deployFilter(this._engine, Filter.fromText(text));
  }

  _onFilterStateHitCount(text, newValue)
  {
    if (newValue == 0)
      this._setDirty(0);
    else
      this._setDirty(0.002);
  }

  _onFilterStateLastHit()
  {
    this._setDirty(0.002);
  }

  _onGenericChange()
  {
    this._setDirty(1);
  }

  _onLoad()
  {
    this._isDirty = 0;

    this._engine.clear();

    for (let subscription of filterStorage.subscriptions())
    {
      if (shouldDeployFilters(subscription))
      {
        for (let text of subscription.filterText())
          deployFilter(this._engine, Filter.fromText(text), [subscription]);
      }
    }
  }

  _onSave()
  {
    this._isDirty = 0;
  }
}

/**
 * Component synchronizing filter storage with the filter engine.
 * @type {module:filterListener~FilterListener}
 * @package
 */
exports.filterListener = new FilterListener();
