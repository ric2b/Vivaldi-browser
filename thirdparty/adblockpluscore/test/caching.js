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

describe("Cache", function()
{
  let Cache = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Cache} = sandboxedRequire("../lib/caching")
    );
  });

  describe("#constructor()", function()
  {
    // A capacity must be specificed and it must be coercable to a positive
    // number greater than or equal to one.
    it("should throw when capacity is missing", function()
    {
      assert.throws(() => new Cache(), Error);
    });

    it("should throw when capacity is zero", function()
    {
      assert.throws(() => new Cache(0), Error);
    });

    it("should throw when capacity is negative", function()
    {
      assert.throws(() => new Cache(-1), Error);
    });

    it("should throw when capacity is fractional less than one", function()
    {
      assert.throws(() => new Cache(0.1), Error);
    });

    it("should throw when capacity is Number.MIN_VALUE", function()
    {
      assert.throws(() => new Cache(Number.MIN_VALUE), Error);
    });

    it("should throw when capacity is -Infinity", function()
    {
      assert.throws(() => new Cache(-Infinity), Error);
    });

    it("should throw when capacity is a non-numerical string", function()
    {
      assert.throws(() => new Cache("ten"), Error);
    });

    it("should not throw when capacity is one", function()
    {
      assert.doesNotThrow(() => new Cache(1));
    });

    it("should not throw when capacity is fractional greater than one", function()
    {
      assert.doesNotThrow(() => new Cache(1.1));
    });

    it("should not throw when capacity is ten", function()
    {
      assert.doesNotThrow(() => new Cache(10));
    });

    it("should not throw when capacity is Number.MAX_VALUE", function()
    {
      assert.doesNotThrow(() => new Cache(Number.MAX_VALUE));
    });

    it("should not throw when capacity is Infinity", function()
    {
      assert.doesNotThrow(() => new Cache(Infinity));
    });

    it("should not throw when capacity is a numerical string", function()
    {
      assert.doesNotThrow(() => new Cache("10"));
    });
  });

  describe("#get()", function()
  {
    const CAPACITY = 100;

    let cache = null;

    beforeEach(function()
    {
      cache = new Cache(CAPACITY);
    });

    it("should not throw when key does not exist", function()
    {
      assert.doesNotThrow(() => cache.get("key"));
    });

    it("should not throw when key is undefined", function()
    {
      assert.doesNotThrow(() => cache.get(undefined));
    });

    it("should return undefined when key does not exist", function()
    {
      assert.strictEqual(cache.get("key"), undefined);
    });

    it("should return value when key exists and is null", function()
    {
      cache.set(null, "value");

      assert.strictEqual(cache.get(null), "value");
    });

    it("should return value when key exists and is a boolean", function()
    {
      cache.set(true, "value");

      assert.strictEqual(cache.get(true), "value");
    });

    it("should return value when key exists and is a number", function()
    {
      cache.set(1, "value");

      assert.strictEqual(cache.get(1), "value");
    });

    it("should return value when key exists and is a string", function()
    {
      cache.set("string", "value");

      assert.strictEqual(cache.get("string"), "value");
    });

    it("should return value when key exists and is an object", function()
    {
      let key = {};

      cache.set(key, "value");

      assert.strictEqual(cache.get(key), "value");
    });

    it("should return value when key exists and value is null", function()
    {
      cache.set("key", null);

      assert.strictEqual(cache.get("key"), null);
    });

    it("should return value when key exists and value is a boolean", function()
    {
      cache.set("key", true);

      assert.strictEqual(cache.get("key"), true);
    });

    it("should return value when key exists and value is a number", function()
    {
      cache.set("key", 1);

      assert.strictEqual(cache.get("key"), 1);
    });

    it("should return value when key exists and value is a string", function()
    {
      cache.set("key", "string");

      assert.strictEqual(cache.get("key"), "string");
    });

    it("should return value when key exists and value is an object", function()
    {
      let value = {};

      cache.set("key", value);

      assert.strictEqual(cache.get("key"), value);
    });

    it("should return any set value until capacity is exceeded", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      // All entries exist.
      for (let i = 0; i < CAPACITY; i++)
        assert.strictEqual(cache.get(i), i);
    });

    it("should return only last set value once capacity is exceeded", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      cache.set(CAPACITY, CAPACITY);

      // Only the last entry exists.
      for (let i = 0; i < CAPACITY; i++)
        assert.strictEqual(cache.get(i), i == CAPACITY ? i : undefined);
    });

    it("should return undefined for any key once cache is cleared", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      cache.clear();

      for (let i = 0; i < CAPACITY; i++)
        assert.strictEqual(cache.get(i), undefined);
    });

    it("should return set value after cache is cleared", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      cache.clear();

      cache.set("key", "value");

      assert.strictEqual(cache.get("key"), "value");
    });
  });

  describe("#set()", function()
  {
    const CAPACITY = 100;

    let cache = null;

    beforeEach(function()
    {
      cache = new Cache(CAPACITY);
    });

    // Neither key nor value can be undefined.
    it("should throw when key is undefined", function()
    {
      assert.throws(() => cache.set(undefined, "value"), Error);
    });

    it("should throw when value is undefined", function()
    {
      assert.throws(() => cache.set("key", undefined), Error);
    });

    // Keys and values can be null.
    it("should not throw when key is null", function()
    {
      assert.doesNotThrow(() => cache.set(null, "value"));
    });

    it("should not throw when value is null", function()
    {
      assert.doesNotThrow(() => cache.set("key", null));
    });

    it("should not throw when key is a boolean", function()
    {
      assert.doesNotThrow(() => cache.set(true, "value"));
    });

    it("should not throw when value is a boolean", function()
    {
      assert.doesNotThrow(() => cache.set("key", true));
    });

    it("should not throw when key is a number", function()
    {
      assert.doesNotThrow(() => cache.set(1, "value"));
    });

    it("should not throw when value is a number", function()
    {
      assert.doesNotThrow(() => cache.set("key", 1));
    });

    it("should not throw when key is a string", function()
    {
      assert.doesNotThrow(() => cache.set("string", "value"));
    });

    it("should not throw when value is a string", function()
    {
      assert.doesNotThrow(() => cache.set("key", "string"));
    });

    it("should not throw when key is an object", function()
    {
      assert.doesNotThrow(() => cache.set({}, "value"));
    });

    it("should not throw when value is an object", function()
    {
      assert.doesNotThrow(() => cache.set("key", {}));
    });

    it("should not throw when capacity is exceeded", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      assert.doesNotThrow(() => cache.set(CAPACITY, CAPACITY));
    });

    it("should not throw when a key is set twice to the same value", function()
    {
      cache.set("key", true);

      assert.doesNotThrow(() => cache.set("key", true));
    });

    it("should not throw when a key is set twice to different values", function()
    {
      cache.set("key", true);

      assert.doesNotThrow(() => cache.set("key", false));
    });

    it("should not throw when a key is set again after being got", function()
    {
      cache.set("key", true);
      cache.get("key");

      assert.doesNotThrow(() => cache.set("key", true));
    });

    it("should not throw when a key is set again after being cleared", function()
    {
      cache.set("key", true);
      cache.clear();

      assert.doesNotThrow(() => cache.set("key", true));
    });
  });

  describe("#clear()", function()
  {
    const CAPACITY = 100;

    let cache = null;

    beforeEach(function()
    {
      cache = new Cache(CAPACITY);
    });

    it("should not throw on empty cache", function()
    {
      assert.doesNotThrow(() => cache.clear());
    });

    it("should not throw on non-empty cache", function()
    {
      cache.set("key", "value");

      assert.doesNotThrow(() => cache.clear());
    });

    it("should not throw on full cache", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      assert.doesNotThrow(() => cache.clear());
    });

    it("should not throw after cache exceeds capacity", function()
    {
      for (let i = 0; i < CAPACITY; i++)
        cache.set(i, i);

      cache.set(CAPACITY, CAPACITY);

      assert.doesNotThrow(() => cache.clear());
    });
  });
});
