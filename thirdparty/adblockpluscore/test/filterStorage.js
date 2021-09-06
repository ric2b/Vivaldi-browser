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

let Filter = null;
let filterNotifier = null;
let filterStorage = null;
let filterEngine = null;
let Subscription = null;

describe("Filter storage", function()
{
  beforeEach(async function()
  {
    let sandboxedRequire = createSandbox();

    (
      {Filter} = sandboxedRequire("../lib/filterClasses"),
      {filterNotifier} = sandboxedRequire("../lib/filterNotifier"),
      {filterStorage} = sandboxedRequire("../lib/filterStorage"),
      {filterEngine} = sandboxedRequire("../lib/filterEngine"),
      {Subscription} = sandboxedRequire("../lib/subscriptionClasses")
    );

    await filterEngine.initialize();
  });

  function addListener(listener)
  {
    let makeWrapper = name => (...args) => listener(name, ...args);

    filterNotifier.on("subscription.added", makeWrapper("subscription.added"));
    filterNotifier.on("subscription.removed",
                      makeWrapper("subscription.removed"));

    filterNotifier.on("filter.added", makeWrapper("filter.added"));
    filterNotifier.on("filter.removed", makeWrapper("filter.removed"));
    filterNotifier.on("filter.moved", makeWrapper("filter.moved"));

    filterNotifier.on("filterState.hitCount", makeWrapper("filterState.hitCount"));
    filterNotifier.on("filterState.lastHit", makeWrapper("filterState.lastHit"));
  }

  function compareSubscriptionList(testMessage, list,
                                   knownSubscriptions = null)
  {
    let result = [...filterStorage._knownSubscriptions.keys()];
    let expected = list.map(subscription => subscription.url);
    assert.deepEqual(result, expected, testMessage);

    if (knownSubscriptions)
    {
      assert.deepEqual([...Subscription.knownSubscriptions.values()],
                       knownSubscriptions, testMessage);
    }
  }

  function compareFiltersList(testMessage, list)
  {
    let result = [...filterStorage.subscriptions()].map(
      subscription => [...subscription.filterText()]);
    assert.deepEqual(result, list, testMessage);
  }

  function compareFilterSubscriptions(testMessage, filter, list)
  {
    let result = [...filterStorage.subscriptions(filter.text)].map(subscription => subscription.url);
    let expected = list.map(subscription => subscription.url);
    assert.deepEqual(result, expected, testMessage);
    assert.equal(filterStorage.getSubscriptionCount(filter.text),
                 expected.length, testMessage + " (getSubscriptionCount)");
  }

  it("Adding subscriptions", function()
  {
    let subscription1 = Subscription.fromURL("https://test1/");
    let subscription2 = Subscription.fromURL("https://test2/");

    let changes = [];
    function listener(action, subscription)
    {
      if (action.indexOf("subscription.") == 0)
        changes.push(action + " " + subscription.url);
    }
    addListener(listener);

    compareSubscriptionList("Initial state", []);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.addSubscription(subscription1);
    compareSubscriptionList("Adding first subscription", [subscription1]);
    assert.deepEqual(changes, ["subscription.added https://test1/"], "Received changes");

    changes = [];
    filterStorage.addSubscription(subscription1);
    compareSubscriptionList("Adding already added subscription", [subscription1]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.addSubscription(subscription2);
    compareSubscriptionList("Adding second", [subscription1, subscription2]);
    assert.deepEqual(changes, ["subscription.added https://test2/"], "Received changes");

    filterStorage.removeSubscription(subscription1);
    compareSubscriptionList("Remove", [subscription2]);

    changes = [];
    filterStorage.addSubscription(subscription1);
    compareSubscriptionList("Re-adding previously removed subscription", [subscription2, subscription1]);
    assert.deepEqual(changes, ["subscription.added https://test1/"], "Received changes");
  });

  it("Removing subscriptions", function()
  {
    let subscription1 = Subscription.fromURL("https://test1/");
    let subscription2 = Subscription.fromURL("https://test2/");

    assert.equal(Subscription.fromURL(subscription1.url), subscription1,
                 "Subscription known before addition");

    filterStorage.addSubscription(subscription1);
    filterStorage.addSubscription(subscription2);

    let changes = [];
    function listener(action, subscription)
    {
      if (action.indexOf("subscription.") == 0)
        changes.push(action + " " + subscription.url);
    }
    addListener(listener);

    compareSubscriptionList("Initial state", [subscription1, subscription2],
                            [subscription1, subscription2]);
    assert.deepEqual(changes, [], "Received changes");

    assert.equal(Subscription.fromURL(subscription1.url), subscription1,
                 "Subscription known after addition");

    changes = [];
    filterStorage.removeSubscription(subscription1);
    compareSubscriptionList("Removing first subscription", [subscription2],
                            [subscription2]);
    assert.deepEqual(changes, ["subscription.removed https://test1/"], "Received changes");

    // Once a subscription has been removed, it is forgotten; a new object is
    // created for the previously known subscription URL.
    assert.notEqual(Subscription.fromURL(subscription1.url), subscription1,
                    "Subscription forgotten upon removal");
    Subscription.knownSubscriptions.delete(subscription1.url);

    changes = [];
    filterStorage.removeSubscription(subscription1);
    compareSubscriptionList("Removing already removed subscription", [subscription2],
                            [subscription2]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.removeSubscription(subscription2);
    compareSubscriptionList("Removing remaining subscription", [], []);
    assert.deepEqual(changes, ["subscription.removed https://test2/"], "Received changes");

    filterStorage.addSubscription(subscription1);
    compareSubscriptionList("Add", [subscription1], []);

    changes = [];
    filterStorage.removeSubscription(subscription1);
    compareSubscriptionList("Re-removing previously added subscription", [], []);
    assert.deepEqual(changes, ["subscription.removed https://test1/"], "Received changes");
  });

  it("Moving subscriptions", function()
  {
    let subscription1 = Subscription.fromURL("https://test1/");
    let subscription2 = Subscription.fromURL("https://test2/");
    let subscription3 = Subscription.fromURL("https://test3/");

    filterStorage.addSubscription(subscription1);
    filterStorage.addSubscription(subscription2);
    filterStorage.addSubscription(subscription3);

    let changes = [];
    function listener(action, subscription)
    {
      if (action.indexOf("subscription.") == 0)
        changes.push(action + " " + subscription.url);
    }
    addListener(listener);

    compareSubscriptionList("Initial state", [subscription1, subscription2, subscription3]);
    assert.deepEqual(changes, [], "Received changes");

    filterStorage.removeSubscription(subscription2);
    compareSubscriptionList("Remove", [subscription1, subscription3]);
  });

  it("Adding filters", function()
  {
    let subscription1 = Subscription.fromURL("~blocking");
    subscription1.defaults = ["blocking"];

    let subscription2 = Subscription.fromURL("~exceptions");
    subscription2.defaults = ["allowing", "elemhide"];

    let subscription3 = Subscription.fromURL("~other");

    filterStorage.addSubscription(subscription1);
    filterStorage.addSubscription(subscription2);
    filterStorage.addSubscription(subscription3);

    let changes = [];
    function listener(action, filter)
    {
      if (action.indexOf("filter.") == 0)
        changes.push(action + " " + filter.text);
    }
    addListener(listener);

    compareFiltersList("Initial state", [[], [], []]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("foo"));
    compareFiltersList("Adding blocking filter", [["foo"], [], []]);
    assert.deepEqual(changes, ["filter.added foo"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("@@bar"));
    compareFiltersList("Adding exception rule", [["foo"], ["@@bar"], []]);
    assert.deepEqual(changes, ["filter.added @@bar"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("foo##bar"));
    compareFiltersList("Adding hiding rule", [["foo"], ["@@bar", "foo##bar"], []]);
    assert.deepEqual(changes, ["filter.added foo##bar"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("foo#@#bar"));
    compareFiltersList("Adding hiding exception", [["foo"], ["@@bar", "foo##bar", "foo#@#bar"], []]);
    assert.deepEqual(changes, ["filter.added foo#@#bar"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("example.com#$#foobar"));
    compareFiltersList("Adding snippet filter", [["foo"], ["@@bar", "foo##bar", "foo#@#bar"], ["example.com#$#foobar"]]);
    assert.deepEqual(changes, ["filter.added example.com#$#foobar"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("!foobar"));
    compareFiltersList("Adding comment", [["foo", "!foobar"], ["@@bar", "foo##bar", "foo#@#bar"], ["example.com#$#foobar"]]);
    assert.deepEqual(changes, ["filter.added !foobar"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("foo"));
    compareFiltersList("Adding already added filter", [["foo", "!foobar"], ["@@bar", "foo##bar", "foo#@#bar"], ["example.com#$#foobar"]]);
    assert.deepEqual(changes, [], "Received changes");

    subscription1.disabled = true;

    changes = [];
    filterStorage.addFilter(Filter.fromText("foo"));
    compareFiltersList("Adding filter already in a disabled subscription", [["foo", "!foobar"], ["@@bar", "foo##bar", "foo#@#bar"], ["example.com#$#foobar", "foo"]]);
    assert.deepEqual(changes, ["filter.added foo"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("foo"), subscription1);
    compareFiltersList("Adding filter to an explicit subscription", [["foo", "!foobar", "foo"], ["@@bar", "foo##bar", "foo#@#bar"], ["example.com#$#foobar", "foo"]]);
    assert.deepEqual(changes, ["filter.added foo"], "Received changes");

    changes = [];
    filterStorage.addFilter(Filter.fromText("example.com#$#foobar"), subscription2, 0);
    compareFiltersList("Adding filter to an explicit subscription with position", [["foo", "!foobar", "foo"], ["example.com#$#foobar", "@@bar", "foo##bar", "foo#@#bar"], ["example.com#$#foobar", "foo"]]);
    assert.deepEqual(changes, ["filter.added example.com#$#foobar"], "Received changes");
  });

  it("Removing filters", function()
  {
    let subscription1 = Subscription.fromURL("~foo");
    subscription1.addFilter(Filter.fromText("foo"));
    subscription1.addFilter(Filter.fromText("foo"));
    subscription1.addFilter(Filter.fromText("bar"));

    let subscription2 = Subscription.fromURL("~bar");
    subscription2.addFilter(Filter.fromText("foo"));
    subscription2.addFilter(Filter.fromText("bar"));
    subscription2.addFilter(Filter.fromText("foo"));

    let subscription3 = Subscription.fromURL("https://test/");
    subscription3.addFilter(Filter.fromText("foo"));
    subscription3.addFilter(Filter.fromText("bar"));

    filterStorage.addSubscription(subscription1);
    filterStorage.addSubscription(subscription2);
    filterStorage.addSubscription(subscription3);

    let changes = [];
    function listener(action, filter)
    {
      if (action.indexOf("filter.") == 0)
        changes.push(action + " " + filter.text);
    }
    addListener(listener);

    compareFiltersList("Initial state", [["foo", "foo", "bar"], ["foo", "bar", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.removeFilter(Filter.fromText("foo"), subscription2, 0);
    compareFiltersList("Remove with explicit subscription and position", [["foo", "foo", "bar"], ["bar", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, ["filter.removed foo"], "Received changes");

    changes = [];
    filterStorage.removeFilter(Filter.fromText("foo"), subscription2, 0);
    compareFiltersList("Remove with explicit subscription and wrong position", [["foo", "foo", "bar"], ["bar", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.removeFilter(Filter.fromText("foo"), subscription1);
    compareFiltersList("Remove with explicit subscription", [["bar"], ["bar", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, ["filter.removed foo", "filter.removed foo"], "Received changes");

    changes = [];
    filterStorage.removeFilter(Filter.fromText("foo"), subscription1);
    compareFiltersList("Remove from subscription not having the filter", [["bar"], ["bar", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.removeFilter(Filter.fromText("bar"));
    compareFiltersList("Remove everywhere", [[], ["foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, ["filter.removed bar", "filter.removed bar"], "Received changes");

    changes = [];
    filterStorage.removeFilter(Filter.fromText("bar"));
    compareFiltersList("Remove of unknown filter", [[], ["foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");
  });

  it("Moving filters", function()
  {
    let subscription1 = Subscription.fromURL("~foo");
    subscription1.addFilter(Filter.fromText("foo"));
    subscription1.addFilter(Filter.fromText("bar"));
    subscription1.addFilter(Filter.fromText("bas"));
    subscription1.addFilter(Filter.fromText("foo"));

    let subscription2 = Subscription.fromURL("https://test/");
    subscription2.addFilter(Filter.fromText("foo"));
    subscription2.addFilter(Filter.fromText("bar"));

    filterStorage.addSubscription(subscription1);
    filterStorage.addSubscription(subscription2);

    let changes = [];
    function listener(action, filter)
    {
      if (action.indexOf("filter.") == 0)
        changes.push(action + " " + filter.text);
    }
    addListener(listener);

    compareFiltersList("Initial state", [["foo", "bar", "bas", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.moveFilter(Filter.fromText("foo"), subscription1, 0, 1);
    compareFiltersList("Regular move", [["bar", "foo", "bas", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, ["filter.moved foo"], "Received changes");

    changes = [];
    filterStorage.moveFilter(Filter.fromText("foo"), subscription1, 0, 3);
    compareFiltersList("Invalid move", [["bar", "foo", "bas", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.moveFilter(Filter.fromText("foo"), subscription2, 0, 1);
    compareFiltersList("Invalid subscription", [["bar", "foo", "bas", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.moveFilter(Filter.fromText("foo"), subscription1, 1, 1);
    compareFiltersList("Move to current position", [["bar", "foo", "bas", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, [], "Received changes");

    changes = [];
    filterStorage.moveFilter(Filter.fromText("bar"), subscription1, 0, 1);
    compareFiltersList("Regular move", [["foo", "bar", "bas", "foo"], ["foo", "bar"]]);
    assert.deepEqual(changes, ["filter.moved bar"], "Received changes");
  });

  it("Hit counts", function()
  {
    let changes = [];
    function listener(action, filter)
    {
      if (action.indexOf("filterState." == 0))
        changes.push(action + " " + filter);
    }
    addListener(listener);

    let filter1 = Filter.fromText("filter1");
    let filter2 = Filter.fromText("filter2");

    filterStorage.addFilter(filter1);

    assert.equal(filter1.hitCount, 0, "filter1 initial hit count");
    assert.equal(filter2.hitCount, 0, "filter2 initial hit count");
    assert.equal(filter1.lastHit, 0, "filter1 initial last hit");
    assert.equal(filter2.lastHit, 0, "filter2 initial last hit");

    changes = [];
    filterStorage.increaseHitCount(filter1);
    assert.equal(filter1.hitCount, 1, "Hit count after increase (filter in list)");
    assert.ok(filter1.lastHit > 0, "Last hit changed after increase");
    assert.deepEqual(changes, ["filterState.hitCount filter1", "filterState.lastHit filter1"], "Received changes");

    changes = [];
    filterStorage.increaseHitCount(filter2);
    assert.equal(filter2.hitCount, 1, "Hit count after increase (filter not in list)");
    assert.ok(filter2.lastHit > 0, "Last hit changed after increase");
    assert.deepEqual(changes, ["filterState.hitCount filter2", "filterState.lastHit filter2"], "Received changes");

    changes = [];
    filterStorage.resetHitCounts([filter1]);
    assert.equal(filter1.hitCount, 0, "Hit count after reset");
    assert.equal(filter1.lastHit, 0, "Last hit after reset");
    assert.deepEqual(changes, ["filterState.hitCount filter1", "filterState.lastHit filter1"], "Received changes");

    changes = [];
    filterStorage.resetHitCounts(null);
    assert.equal(filter2.hitCount, 0, "Hit count after complete reset");
    assert.equal(filter2.lastHit, 0, "Last hit after complete reset");
    assert.deepEqual(changes, ["filterState.hitCount filter2", "filterState.lastHit filter2"], "Received changes");
  });

  it("Filter-subscription relationship", function()
  {
    let filter1 = Filter.fromText("filter1");
    let filter2 = Filter.fromText("filter2");
    let filter3 = Filter.fromText("filter3");

    let subscription1 = Subscription.fromURL("https://test1/");
    subscription1.addFilter(filter1);
    subscription1.addFilter(filter2);

    let subscription2 = Subscription.fromURL("https://test2/");
    subscription2.addFilter(filter2);
    subscription2.addFilter(filter3);

    let subscription3 = Subscription.fromURL("https://test3/");
    subscription3.addFilter(filter1);
    subscription3.addFilter(filter2);
    subscription3.addFilter(filter3);

    compareFilterSubscriptions("Initial filter1 subscriptions", filter1, []);
    compareFilterSubscriptions("Initial filter2 subscriptions", filter2, []);
    compareFilterSubscriptions("Initial filter3 subscriptions", filter3, []);

    filterStorage.addSubscription(subscription1);

    compareFilterSubscriptions("filter1 subscriptions after adding https://test1/", filter1, [subscription1]);
    compareFilterSubscriptions("filter2 subscriptions after adding https://test1/", filter2, [subscription1]);
    compareFilterSubscriptions("filter3 subscriptions after adding https://test1/", filter3, []);

    filterStorage.addSubscription(subscription2);

    compareFilterSubscriptions("filter1 subscriptions after adding https://test2/", filter1, [subscription1]);
    compareFilterSubscriptions("filter2 subscriptions after adding https://test2/", filter2, [subscription1, subscription2]);
    compareFilterSubscriptions("filter3 subscriptions after adding https://test2/", filter3, [subscription2]);

    filterStorage.removeSubscription(subscription1);

    compareFilterSubscriptions("filter1 subscriptions after removing https://test1/", filter1, []);
    compareFilterSubscriptions("filter2 subscriptions after removing https://test1/", filter2, [subscription2]);
    compareFilterSubscriptions("filter3 subscriptions after removing https://test1/", filter3, [subscription2]);

    filterStorage.updateSubscriptionFilters(subscription3, [filter3.text]);

    compareFilterSubscriptions("filter1 subscriptions after updating https://test3/ filters", filter1, []);
    compareFilterSubscriptions("filter2 subscriptions after updating https://test3/ filters", filter2, [subscription2]);
    compareFilterSubscriptions("filter3 subscriptions after updating https://test3/ filters", filter3, [subscription2]);

    filterStorage.addSubscription(subscription3);

    compareFilterSubscriptions("filter1 subscriptions after adding https://test3/", filter1, []);
    compareFilterSubscriptions("filter2 subscriptions after adding https://test3/", filter2, [subscription2]);
    compareFilterSubscriptions("filter3 subscriptions after adding https://test3/", filter3, [subscription2, subscription3]);

    filterStorage.updateSubscriptionFilters(subscription3, [filter1.text, filter2.text]);

    compareFilterSubscriptions("filter1 subscriptions after updating https://test3/ filters", filter1, [subscription3]);
    compareFilterSubscriptions("filter2 subscriptions after updating https://test3/ filters", filter2, [subscription2, subscription3]);
    compareFilterSubscriptions("filter3 subscriptions after updating https://test3/ filters", filter3, [subscription2]);

    filterStorage.removeSubscription(subscription3);

    compareFilterSubscriptions("filter1 subscriptions after removing https://test3/", filter1, []);
    compareFilterSubscriptions("filter2 subscriptions after removing https://test3/", filter2, [subscription2]);
    compareFilterSubscriptions("filter3 subscriptions after removing https://test3/", filter3, [subscription2]);
  });
});
