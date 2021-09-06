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
 * @fileOverview INI parsing.
 */

const {isActiveFilter, Filter} = require("./filterClasses");
const {Subscription} = require("./subscriptionClasses");
const {filterState} = require("./filterState");

/**
 * Parses filter data.
 * @package
 */
exports.INIParser = class INIParser
{
  constructor()
  {
    /**
     * Properties of the filter data.
     * @type {object}
     */
    this.fileProperties = {};

    /**
     * The list of subscriptions in the filter data.
     * @type {Array.<module:subscriptionClasses.Subscription>}
     */
    this.subscriptions = [];

    this._wantObj = true;
    this._curObj = this.fileProperties;
    this._curSection = null;
  }

  /**
   * Processes a line of filter data.
   *
   * @param {string?} line The line of filter data to process. This may be
   *   `null`, which indicates the end of the filter data.
   */
  process(line)
  {
    let match = null;
    if (this._wantObj === true && (match = /^(\w+)=(.*)$/.exec(line)))
    {
      this._curObj[match[1]] = match[2];
    }
    else if (line === null || (match = /^\s*\[(.+)\]\s*$/.exec(line)))
    {
      if (this._curObj)
      {
        // Process current object before going to next section
        switch (this._curSection)
        {
          case "filter":
            if ("text" in this._curObj)
            {
              let {text} = this._curObj;
              if (isActiveFilter(Filter.fromText(text)))
                filterState.fromObject(text, this._curObj);
            }
            break;

          case "subscription":
            let subscription = Subscription.fromObject(this._curObj);
            if (subscription)
              this.subscriptions.push(subscription);
            break;

          case "subscription filters":
            if (this.subscriptions.length)
            {
              let currentSubscription = this.subscriptions[
                this.subscriptions.length - 1
              ];
              currentSubscription.updateFilterText(this._curObj);
            }
            break;
        }
      }

      if (line === null)
        return;

      this._curSection = match[1].toLowerCase();
      switch (this._curSection)
      {
        case "filter":
        case "subscription":
          this._wantObj = true;
          this._curObj = {};
          break;
        case "subscription filters":
          this._wantObj = false;
          this._curObj = [];
          break;
        default:
          this._wantObj = null;
          this._curObj = null;
      }
    }
    else if (this._wantObj === false && line)
    {
      this._curObj.push(line.replace(/\\\[/g, "["));
    }
  }
};
