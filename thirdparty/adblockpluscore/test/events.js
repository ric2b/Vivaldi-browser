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

describe("EventEmitter", function()
{
  let EventEmitter = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {EventEmitter} = sandboxedRequire("../lib/events")
    );
  });

  describe("#on()", function()
  {
    let eventEmitter = null;

    beforeEach(function()
    {
      eventEmitter = new EventEmitter();
    });

    it("should not throw when listener is added", function()
    {
      assert.doesNotThrow(() => eventEmitter.on("event", function() {}));
    });

    it("should not throw when second listener is added for same event", function()
    {
      eventEmitter.on("event", function() {});

      assert.doesNotThrow(() => eventEmitter.on("event", function() {}));
    });

    it("should not throw when second listener is added for different event", function()
    {
      eventEmitter.on("event", function() {});

      assert.doesNotThrow(() => eventEmitter.on("otherEvent", function() {}));
    });

    it("should not throw when same listener is re-added for same event", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);

      assert.doesNotThrow(() => eventEmitter.on("event", listener));
    });

    it("should not throw when same listener is re-added for different event", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);

      assert.doesNotThrow(() => eventEmitter.on("otherEvent", listener));
    });

    it("should not throw when removed listener is re-added for same event", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.off("event", listener);

      assert.doesNotThrow(() => eventEmitter.on("event", listener));
    });

    it("should not throw when removed listener is re-added for different event", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.off("event", listener);

      assert.doesNotThrow(() => eventEmitter.on("otherEvent", listener));
    });

    it("should not throw when listener is added for event already dispatched", function()
    {
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.on("event", function() {}));
    });

    it("should not throw when listener is added for event already dispatched and handled", function()
    {
      eventEmitter.on("event", function() {});
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.on("event", function() {}));
    });

    it("should not throw when thousandth listener is added for same event", function()
    {
      for (let i = 0; i < 999; i++)
        eventEmitter.on("event", function() {});

      assert.doesNotThrow(() => eventEmitter.on("event", function() {}));
    });

    it("should not throw when thousandth listener is added for different events", function()
    {
      for (let i = 0; i < 999; i++)
        eventEmitter.on(`event-${i + 1}`, function() {});

      assert.doesNotThrow(() => eventEmitter.on("event-1000", function() {}));
    });
  });

  describe("#off()", function()
  {
    let eventEmitter = null;

    beforeEach(function()
    {
      eventEmitter = new EventEmitter();
    });

    it("should not throw when non-existent listener is removed", function()
    {
      assert.doesNotThrow(() => eventEmitter.off("event", function() {}));
    });

    it("should not throw when non-existent listener is removed a second time", function()
    {
      let listener = function() {};

      eventEmitter.off("event", listener);

      assert.doesNotThrow(() => eventEmitter.off("event", listener));
    });

    it("should not throw when second non-existent listener is removed", function()
    {
      eventEmitter.off("event", function() {});

      assert.doesNotThrow(() => eventEmitter.off("event", function() {}));
    });

    it("should not throw when previously added listener is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);

      assert.doesNotThrow(() => eventEmitter.off("event", listener));
    });

    it("should not throw when different listener than the one previously added is removed", function()
    {
      eventEmitter.on("event", function() {});

      assert.doesNotThrow(() => eventEmitter.off("event", function() {}));
    });

    it("should not throw when listener is removed for different event than the one for which it was added", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);

      assert.doesNotThrow(() => eventEmitter.off("otherEvent", listener));
    });

    it("should not throw when previously added and removed listener is removed a second time", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.off("event", listener);

      assert.doesNotThrow(() => eventEmitter.off("event", listener));
    });

    it("should not throw when different listener than the one previously added and removed is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.off("event", listener);

      assert.doesNotThrow(() => eventEmitter.off("event", function() {}));
    });

    it("should not throw when non-existent listener is removed for event already dispatched", function()
    {
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.off("event", function() {}));
    });

    it("should not throw when non-existent listener is removed for event already dispatched and handled", function()
    {
      eventEmitter.on("event", function() {});
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.off("event", function() {}));
    });

    it("should not throw when previously added listener is removed for event already dispatched and handled", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.off("event", listener));
    });

    it("should not throw when thousandth previously added listener is removed for same event", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on("event", listeners[i]);
      }

      for (let i = 0; i < listeners.length - 1; i++)
        eventEmitter.off("event", listeners[i]);

      assert.doesNotThrow(() => eventEmitter.off("event", listeners[listeners.length - 1]));
    });

    it("should not throw when thousandth previously added listener is removed for different events", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length - 1; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.doesNotThrow(() => eventEmitter.off("event-1000", listeners[listeners.length - 1]));
    });
  });

  describe("#listeners()", function()
  {
    let eventEmitter = null;

    beforeEach(function()
    {
      eventEmitter = new EventEmitter();
    });

    it("should return empty array when no listeners exist", function()
    {
      assert.deepEqual(eventEmitter.listeners("event"), []);
    });

    it("should return array containing listener when one listener exists", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);

      assert.deepEqual(eventEmitter.listeners("event"), [listener]);
    });

    it("should return array containing listeners when multiple listeners exist", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.deepEqual(eventEmitter.listeners("event"), [listener1, listener2]);
    });

    it("should return empty array when listeners exist for different event", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("otherEvent", listener1);
      eventEmitter.on("otherEvent", listener2);

      assert.deepEqual(eventEmitter.listeners("event"), []);
    });

    it("should return array containing specific listeners when listeners exist for multiple events", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};
      let listener3 = function() {};
      let listener4 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("otherEvent", listener2);
      eventEmitter.on("event", listener3);
      eventEmitter.on("otherEvent", listener4);

      assert.deepEqual(eventEmitter.listeners("event"), [listener1, listener3]);
    });

    it("should return array not containing first previously added listener after listener is removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);

      assert.deepEqual(eventEmitter.listeners("event"), [listener2]);
    });

    it("should return array not containing last previously added listener after listener is removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener2);

      assert.deepEqual(eventEmitter.listeners("event"), [listener1]);
    });

    it("should return empty array after all previously added listeners are removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      assert.deepEqual(eventEmitter.listeners("event"), []);
    });

    it("should return array containing specific listeners after previously added listeners are removed for different event", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};
      let listener3 = function() {};
      let listener4 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("otherEvent", listener2);
      eventEmitter.on("event", listener3);
      eventEmitter.on("otherEvent", listener4);

      eventEmitter.off("otherEvent", listener2);
      eventEmitter.off("otherEvent", listener4);

      assert.deepEqual(eventEmitter.listeners("event"), [listener1, listener3]);
    });

    it("should return array containing first previously added listener after listener is removed and re-added", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);

      eventEmitter.on("event", listener1);

      assert.deepEqual(eventEmitter.listeners("event"), [listener2, listener1]);
    });

    it("should return array containing last previously added listener after listener is removed and re-added", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener2);

      assert.deepEqual(eventEmitter.listeners("event"), [listener1, listener2]);
    });

    it("should return array containing all previously added listeners after listeners are removed and re-added", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.deepEqual(eventEmitter.listeners("event"), [listener1, listener2]);
    });

    it("should return array containing duplicate listeners when duplicate listeners exist", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      assert.deepEqual(eventEmitter.listeners("event"), [listener, listener]);
    });

    it("should return array containing listener after copy is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.deepEqual(eventEmitter.listeners("event"), [listener]);
    });

    it("should return empty array after all copies of listener are removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);
      eventEmitter.off("event", listener);

      assert.deepEqual(eventEmitter.listeners("event"), []);
    });

    it("should return empty array when no listeners exist for event already dispatched", function()
    {
      eventEmitter.emit("event");

      assert.deepEqual(eventEmitter.listeners("event"), []);
    });

    it("should return array containing listeners for event already dispatched and handled", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.emit("event");

      assert.deepEqual(eventEmitter.listeners("event"), [listener1, listener2]);
    });

    it("should return array containing all thousand previously added listeners for same event", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on("event", listeners[i]);
      }

      assert.deepEqual(eventEmitter.listeners("event"), listeners);
    });

    it("should return array containing specific listener out of thousand previously added listeners for different events", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      assert.deepEqual(eventEmitter.listeners("event-1000"), [listeners[listeners.length - 1]]);
    });

    it("should return empty array after all thousand previously added listeners for same event are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on("event", listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off("event", listeners[i]);

      assert.deepEqual(eventEmitter.listeners("event"), []);
    });

    it("should return empty array after all thousand previously added listeners for different events are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.deepEqual(eventEmitter.listeners("event-1000"), []);
    });
  });

  describe("#hasListeners()", function()
  {
    let eventEmitter = null;

    beforeEach(function()
    {
      eventEmitter = new EventEmitter();
    });

    it("should return false when no listeners exist", function()
    {
      assert.strictEqual(eventEmitter.hasListeners("event"), false);
    });

    it("should return false with no argument when no listeners exist", function()
    {
      assert.strictEqual(eventEmitter.hasListeners(), false);
    });

    it("should return true when one listener exists", function()
    {
      eventEmitter.on("event", function() {});

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument when one listener exists", function()
    {
      eventEmitter.on("event", function() {});

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return true when multiple listeners exist", function()
    {
      eventEmitter.on("event", function() {});
      eventEmitter.on("event", function() {});

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument when multiple listeners exist", function()
    {
      eventEmitter.on("event", function() {});
      eventEmitter.on("event", function() {});

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return false when listeners exist for different event", function()
    {
      eventEmitter.on("otherEvent", function() {});
      eventEmitter.on("otherEvent", function() {});

      assert.strictEqual(eventEmitter.hasListeners("event"), false);
    });

    it("should return true when listeners exist for multiple events", function()
    {
      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", function() {});
      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", function() {});

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument when listeners exist for multiple events", function()
    {
      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", function() {});
      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", function() {});

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return true after first of multiple previously added listeners is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", function() {});

      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument after first of multiple previously added listeners is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", function() {});

      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return true after last of multiple previously added listeners is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", function() {});
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument after last of multiple previously added listeners is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", function() {});
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return false after all previously added listeners are removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      assert.strictEqual(eventEmitter.hasListeners("event"), false);
    });

    it("should return false with no argument after all previously added listeners are removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      assert.strictEqual(eventEmitter.hasListeners(), false);
    });

    it("should return true after previously added listeners for different event are removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", listener1);
      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", listener2);

      eventEmitter.off("otherEvent", listener1);
      eventEmitter.off("otherEvent", listener2);

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument after previously added listeners for different event are removed", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", listener1);
      eventEmitter.on("event", function() {});
      eventEmitter.on("otherEvent", listener2);

      eventEmitter.off("otherEvent", listener1);
      eventEmitter.off("otherEvent", listener2);

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return true after all previously added listeners are removed and re-added", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument after all previously added listeners are removed and re-added", function()
    {
      let listener1 = function() {};
      let listener2 = function() {};

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return true after one of multiple copies of listener is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument after one of multiple copies of listener is removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return false after all copies of listener are removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);
      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners("event"), false);
    });

    it("should return false with no argument after all copies of listener are removed", function()
    {
      let listener = function() {};

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);
      eventEmitter.off("event", listener);

      assert.strictEqual(eventEmitter.hasListeners(), false);
    });

    it("should return false when no listeners exist for event already dispatched", function()
    {
      eventEmitter.emit("event");

      assert.strictEqual(eventEmitter.hasListeners("event"), false);
    });

    it("should return true for event already dispatched and handled", function()
    {
      eventEmitter.on("event", function() {});

      eventEmitter.emit("event");

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true when thousand listeners exist for same event", function()
    {
      for (let i = 0; i < 1000; i++)
        eventEmitter.on("event", function() {});

      assert.strictEqual(eventEmitter.hasListeners("event"), true);
    });

    it("should return true with no argument when thousand listeners exist for same event", function()
    {
      for (let i = 0; i < 1000; i++)
        eventEmitter.on("event", function() {});

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return true for thousandth event when listeners exist for thousand different events", function()
    {
      for (let i = 0; i < 1000; i++)
        eventEmitter.on(`event-${i + 1}`, function() {});

      assert.strictEqual(eventEmitter.hasListeners("event-1000"), true);
    });

    it("should return true with no argument when listeners exist for thousand different events", function()
    {
      for (let i = 0; i < 1000; i++)
        eventEmitter.on(`event-${i + 1}`, function() {});

      assert.strictEqual(eventEmitter.hasListeners(), true);
    });

    it("should return false after all thousand previously added listeners for same event are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on("event", listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off("event", listeners[i]);

      assert.strictEqual(eventEmitter.hasListeners("event"), false);
    });

    it("should return false with no argument after all thousand previously added listeners for same event are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on("event", listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off("event", listeners[i]);

      assert.strictEqual(eventEmitter.hasListeners(), false);
    });

    it("should return false for thousandth event after all previously added listeners for thousand different events are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.strictEqual(eventEmitter.hasListeners("event-1000"), false);
    });

    it("should return false for thousandth event after all previously added listeners for thousand different events are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.strictEqual(eventEmitter.hasListeners("event-1000"), false);
    });

    it("should return false with no argument after all previously added listeners for thousand different events are removed", function()
    {
      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function() {};
        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.strictEqual(eventEmitter.hasListeners(), false);
    });
  });

  describe("#emit()", function()
  {
    let eventEmitter = null;

    beforeEach(function()
    {
      eventEmitter = new EventEmitter();
    });

    it("should not throw when no listeners exist", function()
    {
      assert.doesNotThrow(() => eventEmitter.emit("event"));
    });

    it("should not throw with argument when no listeners exist", function()
    {
      assert.doesNotThrow(() => eventEmitter.emit("event"), true);
    });

    it("should invoke listener when one listener exists", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener]]);
    });

    it("should invoke listener with argument when one listener exists", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener, true]]);
    });

    it("should invoke listeners when multiple listeners exist", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1], [listener2]]);
    });

    it("should invoke listeners with argument when multiple listeners exist", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true], [listener2, true]]);
    });

    it("should invoke no listeners when listeners exist for different event", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("otherEvent", listener1);
      eventEmitter.on("otherEvent", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), []);
    });

    it("should invoke no listeners with argument when listeners exist for different event", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("otherEvent", listener1);
      eventEmitter.on("otherEvent", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), []);
    });

    it("should invoke specific listeners when listeners exist for multiple events", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      let listener3 = function(...args)
      {
        calls.push([listener3, ...args]);
      };

      let listener4 = function(...args)
      {
        calls.push([listener4, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("otherEvent", listener2);
      eventEmitter.on("event", listener3);
      eventEmitter.on("otherEvent", listener4);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1], [listener3]]);
    });

    it("should invoke specific listeners with argument when listeners exist for multiple events", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      let listener3 = function(...args)
      {
        calls.push([listener3, ...args]);
      };

      let listener4 = function(...args)
      {
        calls.push([listener4, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("otherEvent", listener2);
      eventEmitter.on("event", listener3);
      eventEmitter.on("otherEvent", listener4);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true], [listener3, true]]);
    });

    it("should not invoke first previously added listener after listener is removed", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener2]]);
    });

    it("should not invoke first previously added listener with argument after listener is removed", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener2, true]]);
    });

    it("should not invoke last previously added listener after listener is removed", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1]]);
    });

    it("should not invoke last previously added listener with argument after listener is removed", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true]]);
    });

    it("should invoke no listeners after all previously added listeners are removed", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), []);
    });

    it("should invoke no listeners with argument after all previously added listeners are removed", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), []);
    });

    it("should invoke specific listeners after previously added listeners are removed for different event", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      let listener3 = function(...args)
      {
        calls.push([listener3, ...args]);
      };

      let listener4 = function(...args)
      {
        calls.push([listener4, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("otherEvent", listener2);
      eventEmitter.on("event", listener3);
      eventEmitter.on("otherEvent", listener4);

      eventEmitter.off("otherEvent", listener2);
      eventEmitter.off("otherEvent", listener4);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1], [listener3]]);
    });

    it("should invoke specific listeners with argument after previously added listeners are removed for different event", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      let listener3 = function(...args)
      {
        calls.push([listener3, ...args]);
      };

      let listener4 = function(...args)
      {
        calls.push([listener4, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("otherEvent", listener2);
      eventEmitter.on("event", listener3);
      eventEmitter.on("otherEvent", listener4);

      eventEmitter.off("otherEvent", listener2);
      eventEmitter.off("otherEvent", listener4);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true], [listener3, true]]);
    });

    it("should invoke first previously added listener after listener is removed and re-added", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);

      eventEmitter.on("event", listener1);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener2], [listener1]]);
    });

    it("should invoke first previously added listener with argument after listener is removed and re-added", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);

      eventEmitter.on("event", listener1);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener2, true], [listener1, true]]);
    });

    it("should invoke last previously added listener after listener is removed and re-added", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1], [listener2]]);
    });

    it("should invoke last previously added listener with argument after listener is removed and re-added", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true], [listener2, true]]);
    });

    it("should invoke all previously added listeners after listeners are removed and re-added", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1], [listener2]]);
    });

    it("should invoke all previously added listeners with argument after listeners are removed and re-added", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.off("event", listener1);
      eventEmitter.off("event", listener2);

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true], [listener2, true]]);
    });

    it("should invoke duplicate listeners when duplicate listeners exist", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener], [listener]]);
    });

    it("should invoke duplicate listeners with argument when duplicate listeners exist", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener, true], [listener, true]]);
    });

    it("should invoke listener after copy is removed", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener]]);
    });

    it("should invoke listener with argument after copy is removed", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener, true]]);
    });

    it("should invoke no listeners after all copies of listener are removed", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);
      eventEmitter.off("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), []);
    });

    it("should invoke no listeners with argument after all copies of listener are removed", function()
    {
      let calls = [];

      let listener = function(...args)
      {
        calls.push([listener, ...args]);
      };

      eventEmitter.on("event", listener);
      eventEmitter.on("event", listener);

      eventEmitter.off("event", listener);
      eventEmitter.off("event", listener);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), []);
    });

    it("should not throw when no listeners exist for event already dispatched", function()
    {
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.emit("event"));
    });

    it("should not throw with argument when no listeners exist for event already dispatched", function()
    {
      eventEmitter.emit("event");

      assert.doesNotThrow(() => eventEmitter.emit("event", true));
    });

    it("should invoke listeners for event already dispatched and handled", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.emit("event");

      calls = [];

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), [[listener1], [listener2]]);
    });

    it("should invoke listeners with argument for event already dispatched and handled", function()
    {
      let calls = [];

      let listener1 = function(...args)
      {
        calls.push([listener1, ...args]);
      };

      let listener2 = function(...args)
      {
        calls.push([listener2, ...args]);
      };

      eventEmitter.on("event", listener1);
      eventEmitter.on("event", listener2);

      eventEmitter.emit("event", true);

      calls = [];

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), [[listener1, true], [listener2, true]]);
    });

    it("should invoke all thousand previously added listeners for same event", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on("event", listeners[i]);
      }

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), listeners.map(listener => [listener]));
    });

    it("should invoke all thousand previously added listeners with argument for same event", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on("event", listeners[i]);
      }

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), listeners.map(listener => [listener, true]));
    });

    it("should invoke specific listener out of thousand previously added listeners for different events", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      assert.deepStrictEqual((eventEmitter.emit("event-1000"), calls), [[listeners[listeners.length - 1]]]);
    });

    it("should invoke specific listener with argument out of thousand previously added listeners for different events", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      assert.deepStrictEqual((eventEmitter.emit("event-1000", true), calls), [[listeners[listeners.length - 1], true]]);
    });

    it("should invoke no listeners after all thousand previously added listeners for same event are removed", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on("event", listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off("event", listeners[i]);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), []);
    });

    it("should invoke no listeners with argument after all thousand previously added listeners for same event are removed", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on("event", listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off("event", listeners[i]);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), []);
    });

    it("should invoke no listeners after all thousand previously added listeners for different events are removed", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.deepStrictEqual((eventEmitter.emit("event"), calls), []);
    });

    it("should invoke no listeners with argument after all thousand previously added listeners for different events are removed", function()
    {
      let calls = [];

      let listeners = new Array(1000);
      for (let i = 0; i < listeners.length; i++)
      {
        listeners[i] = function(...args)
        {
          calls.push([listeners[i], ...args]);
        };

        eventEmitter.on(`event-${i + 1}`, listeners[i]);
      }

      for (let i = 0; i < listeners.length; i++)
        eventEmitter.off(`event-${i + 1}`, listeners[i]);

      assert.deepStrictEqual((eventEmitter.emit("event", true), calls), []);
    });
  });
});
