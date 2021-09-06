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

const {Prefs} = require("prefs");

const {MILLIS_IN_DAY} = require("./time");

/**
 * Converts a version string into a `Date` object with minute-level precision.
 *
 * @param {string} version The version string in `YYYYMMDD[HH[MM]]` format or
 *   just the value `"0"`.
 *
 * @returns {Date} A `Date` object. If the value of `version` is `"0"`, the
 *   returned value represents the Unix epoch.
 */
function versionToDate(version)
{
  if (version == "0")
    return new Date(0);

  let year = version.substring(0, 4);
  let month = version.substring(4, 6);
  let date = version.substring(6, 8);

  let hours = version.substring(8, 10) || "00";
  let minutes = version.substring(10, 12) || "00";

  // The input is nearly a valid ISO 8601 timestamp, except for the hyphens,
  // the colon, and the special characters T and Z. We can use the timestamp
  // string version of the Date constructor directly. Alternatively, we could
  // use Date.UTC(), but that would require some adjustment to the values
  // (e.g. the month must be 0-based).
  return new Date(`${year}-${month}-${date}T${hours}:${minutes}Z`);
}

/**
 * Strips the value of the `firstVersion` parameter down to either `YYYYMMDD`,
 * `YYYYMM`, or `YYYY` depending on its distance from the value of the
 * `currentVersion` parameter.
 *
 * @param {string} firstVersion A version string in `YYYYMMDD[HH[MM]]` format
 *   with an optional `"-E"` suffix or just `"0"` or `"0-E"`.
 * @param {string} currentVersion A version string in `YYYYMMDD[HH[MM]]` format
 *   or just `"0"`.
 *
 * @returns {?string}
 */
function stripFirstVersion(firstVersion, currentVersion)
{
  let eFlag = firstVersion.endsWith("-E");
  if (eFlag)
    firstVersion = firstVersion.slice(0, -2);

  try
  {
    let firstDate = versionToDate(firstVersion);
    let currentDate = versionToDate(currentVersion);

    if (currentDate - firstDate > 365 * MILLIS_IN_DAY)
      firstVersion = firstVersion.substring(0, 4);
    else if (currentDate - firstDate > 30 * MILLIS_IN_DAY)
      firstVersion = firstVersion.substring(0, 6);
    else
      firstVersion = firstVersion.substring(0, 8);
  }
  catch (error)
  {
    return null;
  }

  if (eFlag)
    firstVersion += "-E";

  return firstVersion;
}

/**
 * Checks whether this is a fresh installation.
 * @returns {boolean}
 */
function isFreshInstall()
{
  return !("data" in Prefs.notificationdata);
}

/**
 * `{@link module:analytics.analytics analytics}` implementation.
 */
class Analytics
{
  /**
   * @hideconstructor
   */
  constructor()
  {
  }

  /**
   * Checks whether the given URL is trusted for analytics purposes based on
   * the value of the `analytics.trustedHosts` preference.
   *
   * @param {string} url The URL.
   *
   * @returns {boolean} Whether the URL is trusted.
   */
  isTrusted(url)
  {
    let {trustedHosts} = Prefs.analytics || {};
    if (Array.isArray(trustedHosts))
    {
      try
      {
        return trustedHosts.includes(new URL(url).host);
      }
      catch (error)
      {
      }
    }

    return false;
  }

  /**
   * Returns a string indicating the version of the first ever downloaded
   * resource (as recorded by
   * `{@link module:analytics~Analytics#recordVersion recordVersion()}`), for
   * the purpose of cohort analysis.
   *
   * For privacy reasons, the original value is stripped down to either
   * `YYYYMMDD`, `YYYYMM`, or `YYYY` depending on its age. An `-E` suffix is
   * appended to the original value if it was recorded on an existing
   * installation.
   *
   * @returns {?string} A string indicating the version of the first ever
   *   downloaded resource, or `"0"` or `"0-E"` if there is no data, or `null`
   *   in case of a parsing error.
   */
  getFirstVersion()
  {
    let {analytics} = Prefs;
    if (analytics)
    {
      let {firstVersion = "0", currentVersion = "0"} = analytics.data || {};
      let strippedFirstVersion = stripFirstVersion(firstVersion,
                                                   currentVersion);
      if (strippedFirstVersion)
      {
        if (strippedFirstVersion == "0" && !isFreshInstall())
          strippedFirstVersion = "0-E";

        return strippedFirstVersion;
      }
    }

    return null;
  }

  /**
   * Records the version of a downloaded resource for the purpose of cohort
   * analysis.
   *
   * The value of the `analytics` preference must be set at least to an
   * empty object, otherwise nothing is ever recorded and
   * `{@link module:analytics~Analytics#getFirstVersion getFirstVersion()}`
   * always returns `null`.
   *
   * @param {string} version The version, in `YYYYMMDD[HH[MM]]` format.
   */
  recordVersion(version)
  {
    if (!/^(\d{8}|\d{10}|\d{12})$/.test(version))
      return;

    let {analytics} = Prefs;
    if (analytics)
    {
      if (!analytics.data)
        analytics.data = {};

      let {data} = analytics;

      data.currentVersion = version;

      if (typeof data.firstVersion == "undefined")
      {
        // If this is not a new installation, set the -E flag.
        if (!isFreshInstall())
          version += "-E";

        data.firstVersion = version;
      }

      // JSON values aren't saved unless they are assigned a different object.
      Prefs.analytics = JSON.parse(JSON.stringify(analytics));
    }
  }
}

/**
 * Implements data analytics functions.
 * @type {module:analytics~Analytics}
 */
exports.analytics = new Analytics();
