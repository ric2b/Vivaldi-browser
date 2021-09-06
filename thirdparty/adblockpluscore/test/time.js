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

"use strict";

const assert = require("assert");
const {createSandbox} = require("./_common");

let MILLIS_IN_SECOND = null;
let MILLIS_IN_MINUTE = null;
let MILLIS_IN_HOUR = null;
let MILLIS_IN_DAY = null;

beforeEach(function()
{
  let sandboxedRequire = createSandbox();
  (
    {MILLIS_IN_SECOND, MILLIS_IN_MINUTE, MILLIS_IN_HOUR,
     MILLIS_IN_DAY} = sandboxedRequire("../lib/time")
  );
});

describe("MILLIS_IN_SECOND", function()
{
  it("should be equal to 1000", function()
  {
    assert.equal(MILLIS_IN_SECOND, 1000);
  });
});

describe("MILLIS_IN_MINUTE", function()
{
  it("should be equal to 60 times MILLIS_IN_SECOND", function()
  {
    assert.equal(MILLIS_IN_MINUTE, 60 * MILLIS_IN_SECOND);
  });
});

describe("MILLIS_IN_HOUR", function()
{
  it("should be equal to 60 times MILLIS_IN_MINUTE", function()
  {
    assert.equal(MILLIS_IN_HOUR, 60 * MILLIS_IN_MINUTE);
  });
});

describe("MILLIS_IN_DAY", function()
{
  it("should be equal to 24 times MILLIS_IN_HOUR", function()
  {
    assert.equal(MILLIS_IN_DAY, 24 * MILLIS_IN_HOUR);
  });
});
