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

let Subscription = null;
let SpecialSubscription = null;
let DownloadableSubscription = null;
let RegularSubscription = null;
let Filter = null;

describe("Subscription classes", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Subscription, SpecialSubscription,
       DownloadableSubscription,
       RegularSubscription} = sandboxedRequire("../lib/subscriptionClasses"),
      {Filter} = sandboxedRequire("../lib/filterClasses")
    );
  });

  function compareSubscription(url, expected, postInit)
  {
    expected.push("[Subscription]");
    let subscription = Subscription.fromURL(url);
    if (postInit)
      postInit(subscription);
    let result = [...subscription.serialize()];
    assert.equal(result.sort().join("\n"), expected.sort().join("\n"), url);

    let map = Object.create(null);
    for (let line of result.slice(1))
    {
      if (/(.*?)=(.*)/.test(line))
        map[RegExp.$1] = RegExp.$2;
    }
    let subscription2 = Subscription.fromObject(map);
    assert.equal(subscription.toString(), subscription2.toString(), url + " deserialization");
  }

  function compareSubscriptionFilters(subscription, expected)
  {
    assert.deepEqual([...subscription.filterText()], expected);

    assert.equal(subscription.filterCount, expected.length);

    for (let i = 0; i < subscription.filterCount; i++)
      assert.equal(subscription.filterTextAt(i), expected[i]);

    assert.ok(!subscription.filterTextAt(subscription.filterCount));
    assert.ok(!subscription.filterTextAt(-1));
  }

  it("Definitions", function()
  {
    assert.equal(typeof Subscription, "function", "typeof Subscription");
    assert.equal(typeof SpecialSubscription, "function", "typeof SpecialSubscription");
    assert.equal(typeof RegularSubscription, "function", "typeof RegularSubscription");
    assert.equal(typeof DownloadableSubscription, "function", "typeof DownloadableSubscription");
  });

  it("Subscriptions with state", function()
  {
    compareSubscription("~fl~", ["url=~fl~"]);
    compareSubscription("https://test/default", ["url=https://test/default", "title=https://test/default"]);
    compareSubscription(
      "https://test/default_titled", ["url=https://test/default_titled", "title=test"],
      subscription =>
      {
        subscription.title = "test";
      }
    );
    compareSubscription(
      "https://test/non_default",
      [
        "url=https://test/non_default", "title=test", "disabled=true",
        "lastSuccess=8", "lastDownload=12", "lastCheck=16", "softExpiration=18",
        "expires=20", "downloadStatus=foo", "errors=3", "version=24",
        "requiredVersion=0.6"
      ],
      subscription =>
      {
        subscription.title = "test";
        subscription.disabled = true;
        subscription.lastSuccess = 8;
        subscription.lastDownload = 12;
        subscription.lastCheck = 16;
        subscription.softExpiration = 18;
        subscription.expires = 20;
        subscription.downloadStatus = "foo";
        subscription.errors = 3;
        subscription.version = 24;
        subscription.requiredVersion = "0.6";
      }
    );
    compareSubscription(
      "~wl~", ["url=~wl~", "disabled=true", "title=Test group"],
      subscription =>
      {
        subscription.title = "Test group";
        subscription.disabled = true;
      }
    );
  });

  it("Filter management", function()
  {
    let subscription = Subscription.fromURL("https://example.com/");

    compareSubscriptionFilters(subscription, []);

    subscription.addFilter(Filter.fromText("##.foo"));
    compareSubscriptionFilters(subscription, ["##.foo"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.foo")), 0);

    subscription.addFilter(Filter.fromText("##.bar"));
    compareSubscriptionFilters(subscription, ["##.foo", "##.bar"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 1);

    // Repeat filter.
    subscription.addFilter(Filter.fromText("##.bar"));
    compareSubscriptionFilters(subscription, ["##.foo", "##.bar",
                                              "##.bar"]);

    // The first occurrence is found.
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 1);

    subscription.deleteFilterAt(0);
    compareSubscriptionFilters(subscription, ["##.bar", "##.bar"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 0);

    subscription.insertFilterAt(Filter.fromText("##.foo"), 0);
    compareSubscriptionFilters(subscription, ["##.foo", "##.bar",
                                              "##.bar"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 1);

    subscription.deleteFilterAt(1);
    compareSubscriptionFilters(subscription, ["##.foo", "##.bar"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 1);

    subscription.deleteFilterAt(1);
    compareSubscriptionFilters(subscription, ["##.foo"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), -1);

    subscription.addFilter(Filter.fromText("##.bar"));
    compareSubscriptionFilters(subscription, ["##.foo", "##.bar"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 1);

    subscription.clearFilters();
    compareSubscriptionFilters(subscription, []);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.foo")), -1);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), -1);

    subscription.addFilter(Filter.fromText("##.bar"));
    compareSubscriptionFilters(subscription, ["##.bar"]);

    subscription.addFilter(Filter.fromText("##.foo"));
    compareSubscriptionFilters(subscription, ["##.bar", "##.foo"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.bar")), 0);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.foo")), 1);

    // Insert outside of bounds.
    subscription.insertFilterAt(Filter.fromText("##.lambda"), 1000);
    compareSubscriptionFilters(subscription, ["##.bar", "##.foo",
                                              "##.lambda"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.lambda")), 2);

    // Delete outside of bounds.
    subscription.deleteFilterAt(1000);
    compareSubscriptionFilters(subscription, ["##.bar", "##.foo",
                                              "##.lambda"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.lambda")), 2);

    // Insert outside of bounds (negative).
    subscription.insertFilterAt(Filter.fromText("##.lambda"), -1000);
    compareSubscriptionFilters(subscription, ["##.lambda", "##.bar",
                                              "##.foo", "##.lambda"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.lambda")), 0);

    // Delete outside of bounds (negative).
    subscription.deleteFilterAt(-1000);
    compareSubscriptionFilters(subscription, ["##.lambda", "##.bar",
                                              "##.foo", "##.lambda"]);
    assert.equal(subscription.findFilterIndex(Filter.fromText("##.lambda")), 0);
  });

  it("Subscrition delta", function()
  {
    let subscription = Subscription.fromURL("https://example.com/");

    subscription.addFilter(Filter.fromText("##.foo"));
    subscription.addFilter(Filter.fromText("##.bar"));

    compareSubscriptionFilters(subscription, ["##.foo", "##.bar"]);

    let delta = subscription.updateFilterText(["##.lambda", "##.foo"]);

    // The filters should be in the same order in which they were in the
    // argument to updateFilterText()
    compareSubscriptionFilters(subscription, ["##.lambda", "##.foo"]);

    assert.deepEqual(delta, {added: ["##.lambda"], removed: ["##.bar"]});

    // Add ##.lambda a second time.
    subscription.addFilter(Filter.fromText("##.lambda"));
    compareSubscriptionFilters(subscription, ["##.lambda", "##.foo",
                                              "##.lambda"]);

    delta = subscription.updateFilterText(["##.bar", "##.bar"]);

    // Duplicate filters should be allowed.
    compareSubscriptionFilters(subscription, ["##.bar", "##.bar"]);

    // If there are duplicates in the text, there should be duplicates in the
    // delta.
    assert.deepEqual(delta, {
      added: ["##.bar", "##.bar"],
      removed: ["##.lambda", "##.foo", "##.lambda"]
    });
  });
});

describe("Subscription.isValidURL()", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Subscription} = sandboxedRequire("../lib/subscriptionClasses")
    );
  });

  it("should return true for ~user~982682", function()
  {
    assert.strictEqual(Subscription.isValidURL("~user~982682"), true);
  });

  it("should return false for ~invalid~135692", function()
  {
    assert.strictEqual(Subscription.isValidURL("~invalid~135692"), false);
  });

  it("should return true for https://example.com/", function()
  {
    assert.strictEqual(Subscription.isValidURL("https://example.com/"), true);
  });

  it("should return true for https://example.com/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("https://example.com/list.txt"), true);
  });

  it("should return true for https://example.com:8080/", function()
  {
    assert.strictEqual(Subscription.isValidURL("https://example.com:8080/"), true);
  });

  it("should return true for https://example.com:8080/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("https://example.com:8080/list.txt"), true);
  });

  it("should return false for http://example.com/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://example.com/"), false);
  });

  it("should return false for http://example.com/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://example.com/list.txt"), false);
  });

  it("should return false for http://example.com:8080/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://example.com:8080/"), false);
  });

  it("should return false for http://example.com:8080/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://example.com:8080/list.txt"), false);
  });

  it("should return true for https:example.com/", function()
  {
    assert.strictEqual(Subscription.isValidURL("https:example.com/"), true);
  });

  it("should return true for https:example.com/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("https:example.com/list.txt"), true);
  });

  it("should return false for http:example.com/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http:example.com/"), false);
  });

  it("should return false for http:example.com/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http:example.com/list.txt"), false);
  });

  it("should return true for http://localhost/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://localhost/"), true);
  });

  it("should return true for http://localhost/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://localhost/list.txt"), true);
  });

  it("should return true for http://127.0.0.1/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://127.0.0.1/"), true);
  });

  it("should return true for http://127.0.0.1/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://127.0.0.1/list.txt"), true);
  });

  it("should return true for http://[::1]/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://[::1]/"), true);
  });

  it("should return true for http://[::1]/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://[::1]/list.txt"), true);
  });

  it("should return true for http://0x7f000001/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://0x7f000001/"), true);
  });

  it("should return true for http://0x7f000001/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://0x7f000001/list.txt"), true);
  });

  it("should return true for http://[0:0:0:0:0:0:0:1]/", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://[0:0:0:0:0:0:0:1]/"), true);
  });

  it("should return true for http://[0:0:0:0:0:0:0:1]/list.txt", function()
  {
    assert.strictEqual(Subscription.isValidURL("http://[0:0:0:0:0:0:0:1]/list.txt"), true);
  });

  it("should return true for data:,Hello%2C%20World!", function()
  {
    assert.strictEqual(Subscription.isValidURL("data:,Hello%2C%20World!"), true);
  });

  it("should return true for data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==", function()
  {
    assert.strictEqual(Subscription.isValidURL("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="), true);
  });
});
