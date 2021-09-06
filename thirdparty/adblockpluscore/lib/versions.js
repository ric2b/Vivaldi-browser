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

function parseVersionComponent(comp)
{
  if (comp == "*")
    return Infinity;
  return parseInt(comp, 10) || 0;
}

/**
 * Compares two versions.
 *
 * @param {string} v1 The first version.
 * @param {string} v2 The second version.
 *
 * @returns {number} `-1` if `v1` is older than `v2`; `1` if `v1` is newer than
 *   `v2`; otherwise `0`.
 *
 * @package
 */
exports.compareVersions = function compareVersions(v1, v2)
{
  let regexp = /^(.*?)([a-z].*)?$/i;
  let [, head1, tail1] = regexp.exec(v1);
  let [, head2, tail2] = regexp.exec(v2);
  let components1 = head1.split(".");
  let components2 = head2.split(".");

  for (let i = 0; i < components1.length ||
                  i < components2.length; i++)
  {
    let result = parseVersionComponent(components1[i]) -
                 parseVersionComponent(components2[i]) || 0;

    if (result < 0)
      return -1;
    else if (result > 0)
      return 1;
  }

  // Compare version suffix (e.g. 0.1alpha < 0.1b1 < 01.b2 < 0.1).
  // However, note that this is a simple string comparision, meaning: b10 < b2
  if (tail1 == tail2)
    return 0;
  if (!tail1 || tail2 && tail1 > tail2)
    return 1;
  return -1;
};
