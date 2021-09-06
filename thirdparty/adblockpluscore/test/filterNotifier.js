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

describe("Filter notifier", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier")
    );
  });

  let triggeredListeners = [];
  let listeners = [
    (...args) => triggeredListeners.push(["listener1", ...args]),
    (...args) => triggeredListeners.push(["listener2", ...args]),
    (...args) => triggeredListeners.push(["listener3", ...args])
  ];

  function addListener(listener)
  {
    filterNotifier.on("foo", listener);
  }

  function removeListener(listener)
  {
    filterNotifier.off("foo", listener);
  }

  function compareListeners(testDescription, list)
  {
    assert.equal(filterNotifier.hasListeners(), list.length > 0, testDescription);
    assert.equal(filterNotifier.hasListeners("foo"), list.length > 0,
                 testDescription);

    assert.equal(filterNotifier.hasListeners("bar"), false, testDescription);

    let result1 = triggeredListeners = [];
    filterNotifier.emit("foo", {bar: true});

    let result2 = triggeredListeners = [];
    for (let observer of list)
      observer({bar: true});

    assert.deepEqual(result1, result2, testDescription);
  }

  it("Adding/removing listeners", function()
  {
    let [listener1, listener2, listener3] = listeners;

    compareListeners("No listeners", []);

    addListener(listener1);
    compareListeners("addListener(listener1)", [listener1]);

    addListener(listener1);
    compareListeners("addListener(listener1) again", [listener1, listener1]);

    addListener(listener2);
    compareListeners("addListener(listener2)", [listener1, listener1, listener2]);

    removeListener(listener1);
    compareListeners("removeListener(listener1)", [listener1, listener2]);

    removeListener(listener1);
    compareListeners("removeListener(listener1) again", [listener2]);

    addListener(listener3);
    compareListeners("addListener(listener3)", [listener2, listener3]);

    addListener(listener1);
    compareListeners("addListener(listener1)", [listener2, listener3, listener1]);

    removeListener(listener3);
    compareListeners("removeListener(listener3)", [listener2, listener1]);

    removeListener(listener1);
    compareListeners("removeListener(listener1)", [listener2]);

    removeListener(listener2);
    compareListeners("removeListener(listener2)", []);
  });

  it("Removing listeners while being called", function()
  {
    let listener1 = function(...args)
    {
      listeners[0](...args);
      removeListener(listener1);
    };
    let listener2 = listeners[1];
    addListener(listener1);
    addListener(listener2);

    compareListeners("Initial call", [listener1, listener2]);
    compareListeners("Subsequent calls", [listener2]);
  });
});
