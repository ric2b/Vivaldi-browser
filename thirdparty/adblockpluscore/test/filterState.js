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

let filterNotifier = null;
let filterState = null;

beforeEach(function()
{
  let sandboxedRequire = createSandbox();
  (
    {filterNotifier} = sandboxedRequire("../lib/filterNotifier"),
    {filterState} = sandboxedRequire("../lib/filterState")
  );
});

describe("filterState.isEnabled()", function()
{
  context("No state", function()
  {
    it("should return true for enabled filter", function()
    {
      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter is re-enabled", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter is re-disabled", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setEnabled("||example.com^", true);
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter is disabled and filter's enabled state is reset", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter's enabled state is re-toggled", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.toggleEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's enabled state is toggled and reset", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter's hit count is set to 1 and filter is disabled", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return false after filter's hit count is set to 1, filter is disabled, and filter's hit count is reset", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setEnabled("||example.com^", false);
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter's last hit time is set to 946684800000 and filter is disabled", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return false after filter's last hit time is set to 946684800000, filter is disabled, and filter's last hit time is reset", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setEnabled("||example.com^", false);
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter hit is registered and filter is disabled", function()
    {
      filterState.registerHit("||example.com^");
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return false after filter hit is registered, filter is disabled, and filter hits are reset", function()
    {
      filterState.registerHit("||example.com^");
      filterState.setEnabled("||example.com^", false);
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter is disabled and filter state is reset", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter is disabled and filter state is serialized", function()
    {
      filterState.setEnabled("||example.com^", false);
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter state is serialized and filter is re-enabled", function()
    {
      filterState.setEnabled("||example.com^", false);
      [...filterState.serialize("||example.com^")];
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });
  });

  context("State: disabled = true", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {disabled: true});
    });

    it("should return false for disabled filter", function()
    {
      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter is enabled", function()
    {
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter is re-disabled", function()
    {
      filterState.setEnabled("||example.com^", true);
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter is re-enabled", function()
    {
      filterState.setEnabled("||example.com^", true);
      filterState.setEnabled("||example.com^", false);
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter is enabled and filter's enabled state is reset", function()
    {
      filterState.setEnabled("||example.com^", true);
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter's enabled state is re-toggled", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.toggleEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter's enabled state is toggled and reset", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return false after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter's hit count is set to 1 and filter is enabled", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's hit count is set to 1, filter is enabled, and filter's hit count is reset", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setEnabled("||example.com^", true);
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return false after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter's last hit time is set to 946684800000 and filter is enabled", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter's last hit time is set to 946684800000, filter is enabled, and filter's last hit time is reset", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setEnabled("||example.com^", true);
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return false after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter hit is registered and filter is enabled", function()
    {
      filterState.registerHit("||example.com^");
      filterState.setEnabled("||example.com^", true);

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter hit is registered, filter is enabled, and filter hits are reset", function()
    {
      filterState.registerHit("||example.com^");
      filterState.setEnabled("||example.com^", true);
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return true after filter is enabled and filter state is reset", function()
    {
      filterState.setEnabled("||example.com^", true);
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });

    it("should return true after filter is enabled and filter state is serialized", function()
    {
      filterState.setEnabled("||example.com^", true);
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.isEnabled("||example.com^"), true);
    });

    it("should return false after filter state is serialized and filter is re-disabled", function()
    {
      filterState.setEnabled("||example.com^", true);
      [...filterState.serialize("||example.com^")];
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.isEnabled("||example.com^"), false);
    });
  });
});

describe("filterState.setEnabled()", function()
{
  let events = null;

  function checkEvents(func, expectedEvents = [])
  {
    events = [];

    func();

    assert.deepEqual(events, expectedEvents);
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {filterState} = sandboxedRequire("../lib/filterState"),
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier")
    );

    filterNotifier.on("filterState.enabled", (...args) => events.push(args));
  });

  context("No state", function()
  {
    it("should emit filterState.enabled when filter is disabled", function()
    {
      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should not emit filterState.enabled when filter is enabled", function()
    {
      checkEvents(() => filterState.setEnabled("||example.com^", true));
    });

    it("should emit filterState.enabled when filter is re-enabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter is re-disabled", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setEnabled("||example.com^", true);

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should not emit filterState.enabled when filter is disabled after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false));
    });

    it("should emit filterState.enabled when filter is disabled after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is disabled after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });
  });

  context("State: disabled = true", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {disabled: true});
    });

    it("should emit filterState.enabled when filter is enabled", function()
    {
      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should not emit filterState.enabled when filter is disabled", function()
    {
      checkEvents(() => filterState.setEnabled("||example.com^", false));
    });

    it("should emit filterState.enabled when filter is re-disabled", function()
    {
      filterState.setEnabled("||example.com^", true);

      checkEvents(() => filterState.setEnabled("||example.com^", false),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter is re-enabled", function()
    {
      filterState.setEnabled("||example.com^", true);
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should not emit filterState.enabled when filter is enabled after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true));
    });

    it("should not emit filterState.enabled when filter is enabled after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true));
    });

    it("should emit filterState.enabled when filter is enabled after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter is enabled after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter is enabled after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter is enabled after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter is enabled after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter is enabled after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });

    it("should not emit filterState.enabled when filter is enabled after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.setEnabled("||example.com^", true));
    });

    it("should emit filterState.enabled when filter is enabled after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.setEnabled("||example.com^", true),
                  [["||example.com^", true, false]]);
    });
  });
});

describe("filterState.toggleEnabled()", function()
{
  let events = null;

  function checkEvents(func, expectedEvents = [])
  {
    events = [];

    func();

    assert.deepEqual(events, expectedEvents);
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {filterState} = sandboxedRequire("../lib/filterState"),
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier")
    );

    filterNotifier.on("filterState.enabled", (...args) => events.push(args));
  });

  context("No state", function()
  {
    it("should emit filterState.enabled when filter's enabled state is toggled", function()
    {
      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is re-toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });
  });

  context("State: disabled = true", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {disabled: true});
    });

    it("should emit filterState.enabled when filter's enabled state is toggled", function()
    {
      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is re-toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter is enabled", function()
    {
      filterState.setEnabled("||example.com^", true);

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", false, true]]);
    });

    it("should emit filterState.enabled when filter's enabled state is toggled after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.toggleEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });
  });
});

describe("filterState.resetEnabled()", function()
{
  let events = null;

  function checkEvents(func, expectedEvents = [])
  {
    events = [];

    func();

    assert.deepEqual(events, expectedEvents);
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {filterState} = sandboxedRequire("../lib/filterState"),
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier")
    );

    filterNotifier.on("filterState.enabled", (...args) => events.push(args));
  });

  context("No state", function()
  {
    it("should not emit filterState.enabled when filter's enabled state is reset", function()
    {
      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is re-reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });
  });

  context("State: disabled = true", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {disabled: true});
    });

    it("should emit filterState.enabled when filter's enabled state is reset", function()
    {
      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should not emit filterState.enabled when filter's enabled state is re-reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter is enabled", function()
    {
      filterState.setEnabled("||example.com^", true);

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });

    it("should not emit filterState.enabled when filter's enabled state is reset after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.resetEnabled("||example.com^"), []);
    });

    it("should emit filterState.enabled when filter's enabled state is reset after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.resetEnabled("||example.com^"),
                  [["||example.com^", true, false]]);
    });
  });
});

describe("filterState.getHitCount()", function()
{
  context("No state", function()
  {
    it("should return 0 for filter with no hits", function()
    {
      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's hit count is set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's hit count is re-set to 0", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's hit count is re-set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setHitCount("||example.com^", 0);
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's hit count is set to 1 and then reset", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 2 after two filter hits are registered", function()
    {
      filterState.registerHit("||example.com^");
      filterState.registerHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 2);
    });

    it("should return 0 after filter hit is registered and filter hits are reset", function()
    {
      filterState.registerHit("||example.com^");
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter is disabled and filter's hit count is set to 1", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 1 after filter is disabled, filter's hit count is set to 1, and filter's enabled state is reset", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setHitCount("||example.com^", 1);
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's enabled state is toggled and filter's hit count is set to 1", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 1 after filter's enabled state is toggled, filter's hit count is set to 1, and filter's enabled state is reset", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.setHitCount("||example.com^", 1);
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's last hit time is set to 946684800000 and filter's hit count is set to 1", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 1 after filter's last hit time is set to 946684800000, filter's hit count is set to 1, and filter's last hit time is reset", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setHitCount("||example.com^", 1);
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's hit count is set to 1 and filter state is reset", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's hit count is set to 1 and filter state is serialized", function()
    {
      filterState.setHitCount("||example.com^", 1);
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter state is serialized and filter's hit count is re-set to 0", function()
    {
      filterState.setHitCount("||example.com^", 1);
      [...filterState.serialize("||example.com^")];
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });
  });

  context("State: hitCount = 1", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {hitCount: 1});
    });

    it("should return 1 for filter with one hit", function()
    {
      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's hit count is set to 0", function()
    {
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's hit count is re-set to 1", function()
    {
      filterState.setHitCount("||example.com^", 0);
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's hit count is re-set to 0", function()
    {
      filterState.setHitCount("||example.com^", 0);
      filterState.setHitCount("||example.com^", 1);
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's hit count is set to 0 and then reset", function()
    {
      filterState.setHitCount("||example.com^", 0);
      filterState.resetHitCount("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 2 after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 2);
    });

    it("should return 3 after two filter hits are registered", function()
    {
      filterState.registerHit("||example.com^");
      filterState.registerHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 3);
    });

    it("should return 0 after filter hit is registered and filter hits are reset", function()
    {
      filterState.registerHit("||example.com^");
      filterState.resetHits("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 1 after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter is disabled and filter's hit count is set to 0", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter is disabled, filter's hit count is set to 0, and filter's enabled state is reset", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setHitCount("||example.com^", 0);
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's enabled state is toggled and filter's hit count is set to 0", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's enabled state is toggled, filter's hit count is set to 0, and filter's enabled state is reset", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.setHitCount("||example.com^", 0);
      filterState.resetEnabled("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 1 after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's last hit time is set to 946684800000 and filter's hit count is set to 0", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setHitCount("||example.com^", 0);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's last hit time is set to 946684800000, filter's hit count is set to 0, and filter's last hit time is reset", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setHitCount("||example.com^", 0);
      filterState.resetLastHit("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 0 after filter's hit count is set to 0 and filter state is reset", function()
    {
      filterState.setHitCount("||example.com^", 0);
      filterState.reset("||example.com^");

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });

    it("should return 0 after filter's hit count is set to 0 and filter state is serialized", function()
    {
      filterState.setHitCount("||example.com^", 0);
      [...filterState.serialize("||example.com^")];

      assert.strictEqual(filterState.getHitCount("||example.com^"), 0);
    });

    it("should return 1 after filter state is serialized and filter's hit count is re-set to 1", function()
    {
      filterState.setHitCount("||example.com^", 0);
      [...filterState.serialize("||example.com^")];
      filterState.setHitCount("||example.com^", 1);

      assert.strictEqual(filterState.getHitCount("||example.com^"), 1);
    });
  });
});

describe("filterState.setHitCount()", function()
{
  let events = null;

  function checkEvents(func, expectedEvents = [])
  {
    events = [];

    func();

    assert.deepEqual(events, expectedEvents);
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {filterState} = sandboxedRequire("../lib/filterState"),
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier")
    );

    filterNotifier.on("filterState.hitCount", (...args) => events.push(args));
  });

  context("No state", function()
  {
    it("should emit filterState.hitCount when filter's hit count is set to 1", function()
    {
      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is set to 0", function()
    {
      checkEvents(() => filterState.setHitCount("||example.com^", 0));
    });

    it("should emit filterState.hitCount when filter's hit count is re-set to 0", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is re-set to 1", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.setHitCount("||example.com^", 0);

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is set to 1 after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1));
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 1 after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });
  });

  context("State: hitCount = 1", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {hitCount: 1});
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0", function()
    {
      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is set to 1", function()
    {
      checkEvents(() => filterState.setHitCount("||example.com^", 1));
    });

    it("should emit filterState.hitCount when filter's hit count is re-set to 1", function()
    {
      filterState.setHitCount("||example.com^", 0);

      checkEvents(() => filterState.setHitCount("||example.com^", 1),
                  [["||example.com^", 1, 0]]);
    });

    it("should emit filterState.hitCount when filter's hit count is re-set to 0", function()
    {
      filterState.setHitCount("||example.com^", 0);
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is set to 0 after filter's hit count is reset", function()
    {
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0));
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 2]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is set to 0 after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0));
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is set to 0 after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.setHitCount("||example.com^", 0));
    });

    it("should emit filterState.hitCount when filter's hit count is set to 0 after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.setHitCount("||example.com^", 0),
                  [["||example.com^", 0, 1]]);
    });
  });
});

describe("filterState.resetHitCount()", function()
{
  let events = null;

  function checkEvents(func, expectedEvents = [])
  {
    events = [];

    func();

    assert.deepEqual(events, expectedEvents);
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {filterState} = sandboxedRequire("../lib/filterState"),
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier")
    );

    filterNotifier.on("filterState.hitCount", (...args) => events.push(args));
  });

  context("No state", function()
  {
    it("should not emit filterState.hitCount when filter's hit count is reset", function()
    {
      checkEvents(() => filterState.resetHitCount("||example.com^"));
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1", function()
    {
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is re-reset from 1", function()
    {
      filterState.setHitCount("||example.com^", 1);
      filterState.resetHitCount("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"));
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is reset after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"));
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter state is reset", function()
    {
      filterState.reset("||example.com^");
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset from 1 after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];
      filterState.setHitCount("||example.com^", 1);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });
  });

  context("State: hitCount = 1", function()
  {
    beforeEach(function()
    {
      filterState.fromObject("||example.com^", {hitCount: 1});
    });

    it("should emit filterState.hitCount when filter's hit count is reset", function()
    {
      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is reset from 0", function()
    {
      filterState.setHitCount("||example.com^", 0);

      checkEvents(() => filterState.resetHitCount("||example.com^"));
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter hit is registered", function()
    {
      filterState.registerHit("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 2]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is reset after filter hits are reset", function()
    {
      filterState.resetHits("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"));
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter is disabled", function()
    {
      filterState.setEnabled("||example.com^", false);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter's enabled state is reset", function()
    {
      filterState.resetEnabled("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter's enabled state is toggled", function()
    {
      filterState.toggleEnabled("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter's last hit time is set to 946684800000", function()
    {
      filterState.setLastHit("||example.com^", 946684800000);

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter's last hit time is reset", function()
    {
      filterState.resetLastHit("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });

    it("should not emit filterState.hitCount when filter's hit count is reset after filter state is reset", function()
    {
      filterState.reset("||example.com^");

      checkEvents(() => filterState.resetHitCount("||example.com^"));
    });

    it("should emit filterState.hitCount when filter's hit count is reset after filter state is serialized", function()
    {
      [...filterState.serialize("||example.com^")];

      checkEvents(() => filterState.resetHitCount("||example.com^"),
                  [["||example.com^", 0, 1]]);
    });
  });
});
