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
 * @fileOverview `filterStorage` object responsible for managing the user's
 * subscriptions and filters.
 */

const {IO} = require("io");
const {Prefs} = require("prefs");
const {isActiveFilter, Filter} = require("./filterClasses");
const {Subscription, SpecialSubscription} = require("./subscriptionClasses");
const {filterNotifier} = require("./filterNotifier");
const {INIParser} = require("./iniParser");
const {filterState} = require("./filterState");

/**
 * Version number of the filter storage file format.
 * @type {number}
 */
const FORMAT_VERSION = 5;

/**
 * `{@link module:filterStorage.filterStorage filterStorage}` implementation.
 */
class FilterStorage
{
  /**
   * @hideconstructor
   */
  constructor()
  {
    /**
     * Will be set to `true` after the initial
     * `{@link module:filterStorage~FilterStorage#loadFromDisk loadFromDisk()}`
     * call completes.
     * @type {boolean}
     */
    this.initialized = false;

    /**
     * Will be set to `true` if no `patterns.ini` file exists.
     * @type {boolean}
     */
    this.firstRun = false;

    /**
     * Map of properties listed in the filter storage file before the sections
     * start. Right now this should be only the format version.
     * @type {object}
     */
    this.fileProperties = Object.create(null);

    /**
     * Map of subscriptions already on the list, by their URL/identifier.
     * @type {Map.<string,module:subscriptionClasses.Subscription>}
     * @private
     */
    this._knownSubscriptions = new Map();

    /**
     * Will be set to `true` if
     * `{@link module:filterStorage~FilterStorage#saveToDisk saveToDisk()}`
     * is running (reentrance protection).
     * @type {boolean}
     * @private
     */
    this._saving = false;

    /**
     * Will be set to `true` if a
     * `{@link module:filterStorage~FilterStorage#saveToDisk saveToDisk()}`
     * call arrives while
     * `{@link module:filterStorage~FilterStorage#saveToDisk saveToDisk()}`
     * is already running (delayed execution).
     * @type {boolean}
     * @private
     */
    this._needsSave = false;
  }

  /**
   * The version number of the `patterns.ini` format used.
   * @type {number}
   */
  get formatVersion()
  {
    return FORMAT_VERSION;
  }

  /**
   * The file containing the subscriptions.
   * @type {string}
   */
  get sourceFile()
  {
    return "patterns.ini";
  }

  /**
   * Yields subscriptions in the storage.
   * @param {?string} [filterText] The filter text for which to look. If
   *   specified, the function yields only those subscriptions that contain the
   *   given filter text. By default the function yields all subscriptions.
   * @yields {module:subscriptionClasses.Subscription}
   */
  *subscriptions(filterText = null)
  {
    if (filterText == null)
    {
      yield* this._knownSubscriptions.values();
    }
    else
    {
      for (let subscription of this._knownSubscriptions.values())
      {
        if (subscription.hasFilterText(filterText))
          yield subscription;
      }
    }
  }

  /**
   * Returns the number of subscriptions in the storage.
   * @param {?string} [filterText] The filter text for which to look. If
   *   specified, the function counts only those subscriptions that contain the
   *   given filter text. By default the function counts all subscriptions.
   * @returns {number}
   */
  getSubscriptionCount(filterText = null)
  {
    if (filterText == null)
      return this._knownSubscriptions.size;

    let count = 0;
    for (let subscription of this._knownSubscriptions.values())
    {
      if (subscription.hasFilterText(filterText))
        count++;
    }
    return count;
  }

  /**
   * Finds the filter group that a filter should be added to by default. Will
   * return `null` if this group doesn't exist yet.
   * @param {Filter} filter
   * @returns {?module:subscriptionClasses.SpecialSubscription}
   */
  getGroupForFilter(filter)
  {
    let generalSubscription = null;
    for (let subscription of this._knownSubscriptions.values())
    {
      if (subscription instanceof SpecialSubscription && !subscription.disabled)
      {
        // Always prefer specialized subscriptions
        if (subscription.isDefaultFor(filter))
          return subscription;

        // If this is a general subscription - store it as fallback
        if (!generalSubscription &&
            (!subscription.defaults || !subscription.defaults.length))
          generalSubscription = subscription;
      }
    }
    return generalSubscription;
  }

  /**
   * Checks whether a given subscription is in the storage.
   * @param {module:subscriptionClasses.Subscription} subscription
   * @returns {boolean}
   */
  hasSubscription(subscription)
  {
    return this._knownSubscriptions.has(subscription.url);
  }

  /**
   * Adds a subscription to the storage.
   * @param {module:subscriptionClasses.Subscription} subscription The
   *   subscription to be added.
   */
  addSubscription(subscription)
  {
    if (this._knownSubscriptions.has(subscription.url))
      return;

    this._knownSubscriptions.set(subscription.url, subscription);

    filterNotifier.emit("subscription.added", subscription);
  }

  /**
   * Removes a subscription from the storage.
   * @param {module:subscriptionClasses.Subscription} subscription The
   *   subscription to be removed.
   */
  removeSubscription(subscription)
  {
    if (!this._knownSubscriptions.has(subscription.url))
      return;

    this._knownSubscriptions.delete(subscription.url);

    // This should be the last remaining reference to the Subscription
    // object.
    Subscription.knownSubscriptions.delete(subscription.url);

    filterNotifier.emit("subscription.removed", subscription);
  }

  /**
   * Replaces the list of filters in a subscription with a new list.
   * @param {module:subscriptionClasses.Subscription} subscription The
   *   subscription to be updated.
   * @param {Array.<string>} filterText The new filter text.
   */
  updateSubscriptionFilters(subscription, filterText)
  {
    filterNotifier.emit("subscription.updated", subscription,
                        subscription.updateFilterText(filterText));
  }

  /**
   * Adds a user-defined filter to the storage.
   * @param {Filter} filter
   * @param {?module:subscriptionClasses.SpecialSubscription} [subscription]
   *   The subscription that the filter should be added to.
   * @param {number} [position] The position within the subscription at which
   *   the filter should be added. If not specified, the filter is added at the
   *   end of the subscription.
   */
  addFilter(filter, subscription, position)
  {
    if (!subscription)
    {
      for (let currentSubscription of this._knownSubscriptions.values())
      {
        if (currentSubscription instanceof SpecialSubscription &&
            !currentSubscription.disabled &&
            currentSubscription.hasFilterText(filter.text))
          return;   // No need to add
      }
      subscription = this.getGroupForFilter(filter);
    }
    if (!subscription)
    {
      // No group for this filter exists, create one
      subscription = SpecialSubscription.createForFilter(filter);
      this.addSubscription(subscription);
      return;
    }

    if (typeof position == "undefined")
      position = subscription.filterCount;

    subscription.insertFilterAt(filter, position);
    filterNotifier.emit("filter.added", filter, subscription, position);
  }

  /**
   * Removes a user-defined filter from the storage.
   * @param {module:filterClasses.Filter} filter
   * @param {?module:subscriptionClasses.SpecialSubscription} [subscription]
   *   The subscription that the filter should be removed from. If not
   *   specified, the filter will be removed from all subscriptions.
   * @param {number} [position] The position within the subscription at which
   *   the filter should be removed. If not specified, all instances of the
   *   filter will be removed.
   */
  removeFilter(filter, subscription, position)
  {
    let subscriptions = (
      subscription ? [subscription] : this._knownSubscriptions.values()
    );
    for (let currentSubscription of subscriptions)
    {
      if (currentSubscription instanceof SpecialSubscription &&
          (currentSubscription == subscription ||
           currentSubscription.hasFilterText(filter.text)))
      {
        let positions = [];
        if (typeof position == "undefined")
        {
          let index = -1;
          do
          {
            index = currentSubscription.findFilterIndex(filter, index + 1);
            if (index >= 0)
              positions.push(index);
          } while (index >= 0);
        }
        else
        {
          positions.push(position);
        }

        for (let j = positions.length - 1; j >= 0; j--)
        {
          let currentPosition = positions[j];
          let currentFilterText =
            currentSubscription.filterTextAt(currentPosition);
          if (currentFilterText && currentFilterText == filter.text)
          {
            currentSubscription.deleteFilterAt(currentPosition);
            filterNotifier.emit("filter.removed", filter, currentSubscription,
                                currentPosition);
          }
        }
      }
    }
  }

  /**
   * Moves a user-defined filter to a new position.
   * @param {module:filterClasses.Filter} filter
   * @param {module:subscriptionClasses.SpecialSubscription} subscription The
   *   subscription where the filter is located.
   * @param {number} oldPosition The current position of the filter.
   * @param {number} newPosition The new position of the filter.
   */
  moveFilter(filter, subscription, oldPosition, newPosition)
  {
    if (!(subscription instanceof SpecialSubscription))
      return;

    let currentFilterText = subscription.filterTextAt(oldPosition);
    if (!currentFilterText || currentFilterText != filter.text)
      return;

    newPosition = Math.min(Math.max(newPosition, 0),
                           subscription.filterCount - 1);
    if (oldPosition == newPosition)
      return;

    subscription.deleteFilterAt(oldPosition);
    subscription.insertFilterAt(filter, newPosition);
    filterNotifier.emit("filter.moved", filter, subscription, oldPosition,
                        newPosition);
  }

  /**
   * Increases the hit count for a filter by one.
   * @param {Filter} filter
   */
  increaseHitCount(filter)
  {
    if (!Prefs.savestats || !isActiveFilter(filter))
      return;

    filterState.registerHit(filter.text);
  }

  /**
   * Resets hit count for some filters.
   * @param {?Array.<Filter>} [filters] The filters to be reset. If not
   *   specified, all filters will be reset.
   */
  resetHitCounts(filters)
  {
    if (!filters)
      filters = Filter.knownFilters.values();

    for (let filter of filters)
      filterState.resetHits(filter.text);
  }

  /**
   * @callback TextSink
   * @param {string?} line
   */

  /**
   * Allows importing previously serialized filter data.
   * @param {boolean} silent If `true`, no "load" notification will be sent
   *   out.
   * @returns {TextSink} The function to be called for each line of data.
   *   Calling it with `null` as the argument finalizes the import and replaces
   *   existing data. No changes will be applied before finalization, so import
   *   can be "aborted" by forgetting this callback.
   * @package
   */
  importData(silent)
  {
    let parser = new INIParser();
    return line =>
    {
      parser.process(line);
      if (line === null)
      {
        let knownSubscriptions = new Map();
        for (let subscription of parser.subscriptions)
        {
          // Convert to the new terminology when loading.
          if (Array.isArray(subscription.defaults))
          {
            subscription.defaults.forEach((type, i, arr) =>
            {
              if (type == "whitelist")
                arr[i] = "allowing";
            });
          }
          knownSubscriptions.set(subscription.url, subscription);
        }

        this.fileProperties = parser.fileProperties;
        this._knownSubscriptions = knownSubscriptions;

        if (!silent)
          filterNotifier.emit("load");
      }
    };
  }

  /**
   * Loads all subscriptions from disk.
   * @returns {Promise} A promise resolved or rejected when loading is complete.
   * @package
   */
  async loadFromDisk()
  {
    let tryBackup = async backupIndex =>
    {
      try
      {
        await this.restoreBackup(backupIndex, true);

        if (this._knownSubscriptions.size == 0)
          return tryBackup(backupIndex + 1);
      }
      catch (error)
      {
        // Give up
      }
    };

    try
    {
      let statData = await IO.statFile(this.sourceFile);

      if (!statData.exists)
      {
        this.firstRun = true;
        return;
      }

      let parser = this.importData(true);
      await IO.readFromFile(this.sourceFile, parser);
      parser(null);

      if (this._knownSubscriptions.size == 0)
      {
        // No filter subscriptions in the file, this isn't right.
        throw new Error("No data in the file");
      }
    }
    catch (error)
    {
      console.warn(error);
      await tryBackup(1);
    }

    this.initialized = true;
    filterNotifier.emit("load");
  }

  /**
   * Constructs the file name for a `patterns.ini` backup.
   * @param {number} backupIndex Number of the backup file (1 being the most
   *   recent).
   * @returns {string} Backup file name.
   * @package
   */
  getBackupName(backupIndex)
  {
    let [name, extension] = this.sourceFile.split(".", 2);
    return (name + "-backup" + backupIndex + "." + extension);
  }

  /**
   * Restores an automatically created backup.
   * @param {number} backupIndex Number of the backup to restore (1 being the
   *   most recent).
   * @param {boolean} silent If `true`, no "load" notification will be sent
   *   out.
   * @returns {Promise} A promise resolved or rejected when restoration is
   *   complete.
   * @package
   */
  async restoreBackup(backupIndex, silent)
  {
    let backupFile = this.getBackupName(backupIndex);
    let parser = this.importData(silent);
    await IO.readFromFile(backupFile, parser);
    parser(null);
    return this.saveToDisk();
  }

  /**
   * Generator serializing filter data and yielding it line by line.
   * @yields {string}
   */
  *exportData()
  {
    // Do not persist external subscriptions
    let subscriptions = [];
    for (let subscription of this._knownSubscriptions.values())
    {
      if (!(subscription instanceof SpecialSubscription &&
            subscription.filterCount == 0))
        subscriptions.push(subscription);
    }

    yield "# Adblock Plus preferences";
    yield "version=" + this.formatVersion;

    let saved = new Set();

    // Save subscriptions
    for (let subscription of subscriptions)
    {
      yield* subscription.serialize();
      yield* subscription.serializeFilters();
    }

    // Save filter data
    for (let subscription of subscriptions)
    {
      for (let text of subscription.filterText())
      {
        if (!saved.has(text))
        {
          yield* filterState.serialize(text);
          saved.add(text);
        }
      }
    }
  }

  /**
   * Saves all subscriptions back to disk.
   * @returns {Promise} A promise resolved or rejected when saving is complete.
   * @package
   */
  async saveToDisk()
  {
    if (this._saving)
    {
      this._needsSave = true;
      return;
    }

    this._saving = true;

    try
    {
      await Promise.resolve();

      let isBackupRequired = async() =>
      {
        // First check whether we need to create a backup
        if (Prefs.patternsbackups <= 0)
          return false;

        let statData = await IO.statFile(this.sourceFile);
        if (!statData.exists)
          return false;

        let backupStatData = await IO.statFile(this.getBackupName(1));
        if (backupStatData.exists &&
            (Date.now() - backupStatData.lastModified) / 3600000 <
              Prefs.patternsbackupinterval)
          return false;

        return true;
      };

      if (await isBackupRequired())
      {
        let renameBackup = async index =>
        {
          if (index > 0)
          {
            try
            {
              await IO.renameFile(this.getBackupName(index),
                                  this.getBackupName(index + 1));
            }
            catch (error)
            {
              // Expected error, backup file doesn't exist.
            }

            return renameBackup(index - 1);
          }

          try
          {
            return IO.renameFile(this.sourceFile, this.getBackupName(1));
          }
          catch (error)
          {
            // Expected error, backup file doesn't exist.
          }
        };

        // Rename existing files
        await renameBackup(Prefs.patternsbackups - 1);
      }
    }
    catch (error)
    {
      // Errors during backup creation shouldn't prevent writing filters.
      console.warn(error);
    }

    try
    {
      await IO.writeToFile(this.sourceFile, this.exportData());
      filterNotifier.emit("save");
    }
    catch (error)
    {
      // If saving failed, report error but continue - we still have to process
      // flags.
      console.warn(error);
    }

    this._saving = false;
    if (this._needsSave)
    {
      this._needsSave = false;
      this.saveToDisk();
    }
  }

  /**
   * @typedef FileInfo
   * @type {object}
   * @property {number} index
   * @property {number} lastModified
   */

  /**
   * Returns a promise resolving in a list of existing backup files.
   * @returns {Promise.<Array.<FileInfo>>}
   * @package
   */
  async getBackupFiles()
  {
    let backups = [];

    let checkBackupFile = async index =>
    {
      try
      {
        let statData = IO.statFile(this.getBackupName(index));
        if (!statData.exists)
          return backups;

        backups.push({
          index,
          lastModified: statData.lastModified
        });

        return checkBackupFile(index + 1);
      }
      catch (error)
      {
        // Something went wrong, return whatever data we got so far.
        console.warn(error);
        return backups;
      }
    };

    return checkBackupFile(1);
  }
}

/**
 * Reads the user's filters from disk, manages them in memory, and writes them
 * back to disk.
 * @type {module:filterStorage~FilterStorage}
 */
exports.filterStorage = new FilterStorage();
