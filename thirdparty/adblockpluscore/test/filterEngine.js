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

let filterEngine = null;
let defaultMatcher = null;
let contentTypes = null;
let elemHide = null;
let elemHideEmulation = null;
let snippets = null;
let Filter = null;

function checkFilters(...details)
{
  for (let detail of details)
  {
    let {type, expected} = detail;

    switch (type)
    {
      case "blocking":
        let {resource} = detail;
        let url = new URL(`https://example.com${resource}`);
        let filter = defaultMatcher.match(url,
                                          contentTypes.SCRIPT,
                                          "example.com");
        assert.equal(filter ? filter.text : null, expected);
        break;

      case "elemhide":
        let {selectors} = elemHide.getStyleSheet("example.com", false, true);
        assert.deepEqual(selectors, expected);
        break;

      case "elemhideemulation":
        let rules = elemHideEmulation.getFilters("example.com");
        assert.deepEqual(rules.map(({selector}) => selector), expected);
        break;

      case "snippet":
        let filters = snippets.getFilters("example.com");
        assert.deepEqual(filters.map(({script}) => script), expected);
        break;

      default:
        assert.fail(`Unknown filter type "${type}"`);
    }
  }
}

beforeEach(function()
{
  let sandboxedRequire = createSandbox();
  (
    {filterEngine} = sandboxedRequire("../lib/filterEngine.js"),
    {defaultMatcher} = sandboxedRequire("../lib/matcher.js"),
    {contentTypes} = sandboxedRequire("../lib/contentTypes.js"),
    {elemHide} = sandboxedRequire("../lib/elemHide.js"),
    {elemHideEmulation} = sandboxedRequire("../lib/elemHideEmulation.js"),
    {snippets} = sandboxedRequire("../lib/snippets.js"),
    {Filter} = sandboxedRequire("../lib/filterClasses.js")
  );
});

describe("filterEngine.initialize()", function()
{
  context("Using filters loaded from disk", function()
  {
    it("should return promise", function()
    {
      assert.ok(filterEngine.initialize().constructor.name, "Promise");
    });

    it("should return same promise if already invoked", function()
    {
      let promise = filterEngine.initialize();

      assert.strictEqual(filterEngine.initialize(), promise);
    });

    it("should return same promise if already invoked and promise fulfilled", async function()
    {
      let promise = filterEngine.initialize();
      await promise;

      assert.strictEqual(filterEngine.initialize(), promise);
    });
  });

  context("Using static filter text", function()
  {
    let filterText = [
      "^foo.",
      "^bar.",
      "##.foo",
      "example.com#?#div:abp-has(> .foo)",
      "example.com#$#snippet-1"
    ];

    it("should return promise", function()
    {
      assert.ok(filterEngine.initialize(filterText).constructor.name, "Promise");
    });

    it("should return same promise if already invoked", function()
    {
      let promise = filterEngine.initialize(filterText);

      assert.strictEqual(filterEngine.initialize(filterText), promise);
    });

    it("should return same promise if already invoked and promise fulfilled", async function()
    {
      let promise = filterEngine.initialize(filterText);
      await promise;

      assert.strictEqual(filterEngine.initialize(filterText), promise);
    });

    it("should reject returned promise if filter text contains illegal values", async function()
    {
      let rejected = false;

      let promise = filterEngine.initialize(filterText.concat(null));

      try
      {
        await promise;
      }
      catch (error)
      {
        rejected = true;
      }

      assert.ok(rejected);
    });
  });

  context("Using dynamic filter source", function()
  {
    function* filterSource()
    {
      for (let i = 0; i < 100; i++)
        yield `/track?timestamp=${Date.now()}`;
    }

    it("should return promise", function()
    {
      assert.ok(filterEngine.initialize(filterSource()).constructor.name, "Promise");
    });

    it("should return same promise if already invoked", function()
    {
      let promise = filterEngine.initialize(filterSource());

      assert.strictEqual(filterEngine.initialize(filterSource()), promise);
    });

    it("should return same promise if already invoked and promise fulfilled", async function()
    {
      let promise = filterEngine.initialize(filterSource());
      await promise;

      assert.strictEqual(filterEngine.initialize(filterSource()), promise);
    });

    it("should reject returned promise if filter source yields illegal values", async function()
    {
      let rejected = false;

      function* badFilterSource()
      {
        yield* filterSource();
        yield null;
      }

      // Note: The call should not throw. Any processing should happen
      // asynchronously.
      let promise = filterEngine.initialize(badFilterSource());

      try
      {
        await promise;
      }
      catch (error)
      {
        rejected = true;
      }

      assert.ok(rejected);
    });

    it("should reject returned promise if filter source throws", async function()
    {
      let rejected = false;

      function* badFilterSource()
      {
        yield* filterSource();
        throw new Error();
      }

      let promise = filterEngine.initialize(badFilterSource());

      try
      {
        await promise;
      }
      catch (error)
      {
        rejected = true;
      }

      assert.ok(rejected);
    });
  });
});

describe("filterEngine.add()", function()
{
  it("should add a filter", function()
  {
    checkFilters({type: "blocking", resource: "/foo.js", expected: null});

    filterEngine.add(Filter.fromText("^foo."));
    checkFilters({
      type: "blocking",
      resource: "/foo.js",
      expected: "^foo."
    });
  });

  it("should add multiple filters", function()
  {
    checkFilters({type: "blocking", resource: "/foo.js", expected: null},
                 {type: "blocking", resource: "/bar.js", expected: null});

    filterEngine.add(Filter.fromText("^foo."));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "blocking",
        resource: "/bar.js",
        expected: null
      }
    );

    filterEngine.add(Filter.fromText("^bar."));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "blocking",
        resource: "/bar.js",
        expected: "^bar."
      }
    );
  });

  it("should add filters of different types", function()
  {
    checkFilters({type: "blocking", resource: "/foo.js", expected: null},
                 {type: "elemhide", expected: []},
                 {type: "elemhideemulation", expected: []},
                 {type: "snippet", expected: []});

    filterEngine.add(Filter.fromText("^foo."));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo." // added
      },
      {
        type: "elemhide",
        expected: []
      },
      {
        type: "elemhideemulation",
        expected: []
      },
      {
        type: "snippet",
        expected: []
      }
    );

    filterEngine.add(Filter.fromText("##.foo"));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "elemhide",
        expected: [".foo"] // added
      },
      {
        type: "elemhideemulation",
        expected: []
      },
      {
        type: "snippet",
        expected: []
      }
    );

    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "elemhide",
        expected: [".foo"]
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"] // added
      },
      {
        type: "snippet",
        expected: []
      }
    );

    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "elemhide",
        expected: [".foo"]
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"]
      },
      {
        type: "snippet",
        expected: ["snippet-1"] // added
      }
    );
  });

  it("should do nothing for an existing filter", function()
  {
    checkFilters({type: "blocking", resource: "/foo.js", expected: null});

    filterEngine.add(Filter.fromText("^foo."));
    checkFilters({
      type: "blocking",
      resource: "/foo.js",
      expected: "^foo."
    });

    // Repeat.
    filterEngine.add(Filter.fromText("^foo."));
    checkFilters({
      type: "blocking",
      resource: "/foo.js",
      expected: "^foo."
    });

    filterEngine.remove(Filter.fromText("^foo."));
    checkFilters({type: "blocking", resource: "/foo.js", expected: null});
  });
});

describe("filterEngine.remove()", function()
{
  beforeEach(function()
  {
    filterEngine.add(Filter.fromText("^foo."));
    filterEngine.add(Filter.fromText("^bar."));
    filterEngine.add(Filter.fromText("##.foo"));
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
  });

  it("should remove a filter", function()
  {
    checkFilters({
      type: "blocking",
      resource: "/foo.js",
      expected: "^foo."
    });

    filterEngine.remove(Filter.fromText("^foo."));
    checkFilters({type: "blocking", resource: "/foo.js", expected: null});
  });

  it("should remove multiple filters", function()
  {
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "blocking",
        resource: "/bar.js",
        expected: "^bar."
      }
    );

    filterEngine.remove(Filter.fromText("^foo."));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: null
      },
      {
        type: "blocking",
        resource: "/bar.js",
        expected: "^bar."
      }
    );

    filterEngine.remove(Filter.fromText("^bar."));
    checkFilters({type: "blocking", resource: "/foo.js", expected: null},
                 {type: "blocking", resource: "/bar.js", expected: null});
  });

  it("should remove filters of different types", function()
  {
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "elemhide",
        expected: [".foo"]
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"]
      },
      {
        type: "snippet",
        expected: ["snippet-1"]
      }
    );

    filterEngine.remove(Filter.fromText("^foo."));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: null // removed
      },
      {
        type: "elemhide",
        expected: [".foo"]
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"]
      },
      {
        type: "snippet",
        expected: ["snippet-1"]
      }
    );

    filterEngine.remove(Filter.fromText("##.foo"));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: null
      },
      {
        type: "elemhide",
        expected: [] // removed
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"]
      },
      {
        type: "snippet",
        expected: ["snippet-1"]
      }
    );

    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: null
      },
      {
        type: "elemhide",
        expected: []
      },
      {
        type: "elemhideemulation",
        expected: [] // removed
      },
      {
        type: "snippet",
        expected: ["snippet-1"]
      }
    );

    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: null
      },
      {
        type: "elemhide",
        expected: []
      },
      {
        type: "elemhideemulation",
        expected: []
      },
      {
        type: "snippet",
        expected: [] // removed
      }
    );
  });

  it("should do nothing for a non-existing filter", function()
  {
    checkFilters({
      type: "blocking",
      resource: "/foo.js",
      expected: "^foo."
    });

    filterEngine.remove(Filter.fromText("^foo."));
    checkFilters({type: "blocking", resource: "/foo.js", expected: null});

    // Repeat.
    filterEngine.remove(Filter.fromText("^foo."));
    checkFilters({type: "blocking", resource: "/foo.js", expected: null});
  });
});

describe("filterEngine.has()", function()
{
  // Addition.

  // Blocking filters.
  it("should return false for ||example.com^", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return true for ||example.com^ after ||example.com^ is added", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), true);
  });

  it("should return true for ||example.com^ after ||example.com^ is added twice", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), true);
  });

  // Multiple blocking filters.
  it("should return true for ||example.com^ after ||example.com^ and ||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), true);
  });

  it("should return true for ||example.net^ after ||example.com^ and ||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), true);
  });

  // Allowing filters.
  it("should return false for @@||example.com^", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return true for @@||example.com^ after @@||example.com^ is added", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), true);
  });

  it("should return true for @@||example.com^ after @@||example.com^ is added twice", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), true);
  });

  // Multiple allowing filters.
  it("should return true for @@||example.com^ after @@||example.com^ and @@||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), true);
  });

  it("should return true for @@||example.net^ after @@||example.com^ and @@||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), true);
  });

  // Blocking and allowing filters.
  it("should return true for ||example.com^ after ||example.com^ and @@||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), true);
  });

  it("should return true for @@||example.net^ after ||example.com^ and @@||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), true);
  });

  it("should return true for @@||example.com^ after @@||example.com^ and ||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), true);
  });

  it("should return true for ||example.net^ after @@||example.com^ and ||example.net^ are added", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), true);
  });

  // Element hiding filters.
  it("should return false for example.com##.foo", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return true for example.com##.foo after example.com##.foo is added", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), true);
  });

  it("should return true for example.com##.foo after example.com##.foo is added twice", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), true);
  });

  // Multiple element hiding filters.
  it("should return true for example.com##.foo after example.com##.foo and example.net##.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), true);
  });

  it("should return true for example.net##.bar after example.com##.foo and example.net##.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), true);
  });

  // Element hiding exceptions.
  it("should return false for example.com#@#.foo", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return true for example.com#@#.foo after example.com#@#.foo is added", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), true);
  });

  it("should return true for example.com#@#.foo after example.com#@#.foo is added twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), true);
  });

  // Multiple element hiding exceptions.
  it("should return true for example.com#@#.foo after example.com#@#.foo and example.net#@#.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), true);
  });

  it("should return true for example.net#@#.bar after example.com#@#.foo and example.net#@#.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), true);
  });

  // Element hiding filters and exceptions.
  it("should return true for example.com##.foo after example.com##.foo and example.net#@#.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), true);
  });

  it("should return true for example.net#@#.bar after example.com##.foo and example.net#@#.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), true);
  });

  it("should return true for example.com#@#.foo after example.com#@#.foo and example.net##.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), true);
  });

  it("should return true for example.net##.bar after example.com#@#.foo and example.net##.bar are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), true);
  });

  // Element hiding emulation filters.
  it("should return false for example.com#?#div:abp-has(> .foo)", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return true for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is added", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), true);
  });

  it("should return true for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is added twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), true);
  });

  // Multiple element hiding emulation filters.
  it("should return true for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), true);
  });

  it("should return true for example.net#?#div:abp-has(> .bar) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#?#div:abp-has(> .bar)")), true);
  });

  // Snippet filters.
  it("should return false for example.com#$#snippet-1", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return true for example.com#$#snippet-1 after example.com#$#snippet-1 is added", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), true);
  });

  it("should return true for example.com#$#snippet-1 after example.com#$#snippet-1 is added twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), true);
  });

  // Multiple snippet filters.
  it("should return true for example.com#$#snippet-1 after example.com#$#snippet-1 and example.net#$#snippet-2 are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), true);
  });

  it("should return true for example.net#$#snippet-2 after example.com#$#snippet-1 and example.net#$#snippet-2 are added", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#$#snippet-2")), true);
  });

  // Comment filters.
  it("should return false for ! CDN-based filters", function()
  {
    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  it("should return false for ! CDN-based filters after ! CDN-based filters is added", function()
  {
    filterEngine.add(Filter.fromText("! CDN-based filters"));

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  it("should return false for ! CDN-based filters after ! CDN-based filters is added twice", function()
  {
    filterEngine.add(Filter.fromText("! CDN-based filters"));
    filterEngine.add(Filter.fromText("! CDN-based filters"));

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  // Removal.

  // Blocking filters.
  it("should return false for ||example.com^ after ||example.com^ is removed", function()
  {
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return false for ||example.com^ after ||example.com^ is added and removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return false for ||example.com^ after ||example.com^ is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.remove(Filter.fromText("||example.com^"));
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  // Multiple blocking filters.
  it("should return false for ||example.com^ after ||example.com^ and ||example.net^ are added and ||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return true for ||example.net^ after ||example.com^ and ||example.net^ are added and ||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), true);
  });

  it("should return true for ||example.com^ after ||example.com^ and ||example.net^ are added and ||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), true);
  });

  it("should return false for ||example.net^ after ||example.com^ and ||example.net^ are added and ||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), false);
  });

  // Allowing filters.
  it("should return false for @@||example.com^ after @@||example.com^ is removed", function()
  {
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return false for @@||example.com^ after @@||example.com^ is added and removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return false for @@||example.com^ after @@||example.com^ is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  // Multiple allowing filters.
  it("should return false for @@||example.com^ after @@||example.com^ and @@||example.net^ are added and @@||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return true for @@||example.net^ after @@||example.com^ and @@||example.net^ are added and @@||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), true);
  });

  it("should return true for @@||example.com^ after @@||example.com^ and @@||example.net^ are added and @@||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), true);
  });

  it("should return false for @@||example.net^ after @@||example.com^ and @@||example.net^ are added and @@||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), false);
  });

  // Blocking and allowing filters.
  it("should return false for ||example.com^ after ||example.com^ and @@||example.net^ are added and ||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return true for @@||example.net^ after ||example.com^ and @@||example.net^ are added and ||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), true);
  });

  it("should return true for ||example.com^ after ||example.com^ and @@||example.net^ are added and @@||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), true);
  });

  it("should return false for @@||example.net^ after ||example.com^ and @@||example.net^ are added and @@||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), false);
  });

  it("should return false for @@||example.com^ after @@||example.com^ and ||example.net^ are added and @@||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return true for ||example.net^ after @@||example.com^ and ||example.net^ are added and @@||example.com^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("@@||example.com^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), true);
  });

  it("should return true for @@||example.com^ after @@||example.com^ and ||example.net^ are added and ||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), true);
  });

  it("should return false for ||example.net^ after @@||example.com^ and ||example.net^ are added and ||example.net^ is removed", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.remove(Filter.fromText("||example.net^"));

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), false);
  });

  // Element hiding filters.
  it("should return false for example.com##.foo after example.com##.foo is removed", function()
  {
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return false for example.com##.foo after example.com##.foo is added and removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return false for example.com##.foo after example.com##.foo is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  // Multiple element hiding filters.
  it("should return false for example.com##.foo after example.com##.foo and example.net##.bar are added and example.com##.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return true for example.net##.bar after example.com##.foo and example.net##.bar are added and example.com##.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), true);
  });

  it("should return true for example.com##.foo after example.com##.foo and example.net##.bar are added and example.net##.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), true);
  });

  it("should return false for example.net##.bar after example.com##.foo and example.net##.bar are added and example.net##.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), false);
  });

  // Element hiding exceptions.
  it("should return false for example.com#@#.foo after example.com#@#.foo is removed", function()
  {
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return false for example.com#@#.foo after example.com#@#.foo is added and removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return false for example.com#@#.foo after example.com#@#.foo is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  // Multiple element hiding exceptions.
  it("should return false for example.com#@#.foo after example.com#@#.foo and example.net#@#.bar are added and example.com#@#.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return true for example.net#@#.bar after example.com#@#.foo and example.net#@#.bar are added and example.com#@#.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), true);
  });

  it("should return true for example.com#@#.foo after example.com#@#.foo and example.net#@#.bar are added and example.net#@#.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), true);
  });

  it("should return false for example.net#@#.bar after example.com#@#.foo and example.net#@#.bar are added and example.net#@#.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), false);
  });

  // Element hiding filters and exceptions.
  it("should return false for example.com##.foo after example.com##.foo and example.net#@#.bar are added and example.com##.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return true for example.net#@#.bar after example.com##.foo and example.net#@#.bar are added and example.com##.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.com##.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), true);
  });

  it("should return true for example.com##.foo after example.com##.foo and example.net#@#.bar are added and example.net#@#.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), true);
  });

  it("should return false for example.net#@#.bar after example.com##.foo and example.net#@#.bar are added and example.net#@#.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.remove(Filter.fromText("example.net#@#.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), false);
  });

  it("should return false for example.com#@#.foo after example.com#@#.foo and example.net##.bar are added and example.com#@#.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return true for example.net##.bar after example.com#@#.foo and example.net##.bar are added and example.com#@#.foo is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.com#@#.foo"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), true);
  });

  it("should return true for example.com#@#.foo after example.com#@#.foo and example.net##.bar are added and example.net##.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), true);
  });

  it("should return false for example.net##.bar after example.com#@#.foo and example.net##.bar are added and example.net##.bar is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.remove(Filter.fromText("example.net##.bar"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), false);
  });

  // Element hiding emulation filters.
  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is removed", function()
  {
    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is added and removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  // Multiple element hiding emulation filters.
  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added and example.com#?#div:abp-has(> .foo) is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));
    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return true for example.net#?#div:abp-has(> .bar) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added and example.com#?#div:abp-has(> .foo) is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));
    filterEngine.remove(Filter.fromText("example.com#?#div:abp-has(> .foo)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#?#div:abp-has(> .bar)")), true);
  });

  it("should return true for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added and example.net#?#div:abp-has(> .bar) is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));
    filterEngine.remove(Filter.fromText("example.net#?#div:abp-has(> .bar)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), true);
  });

  it("should return false for example.net#?#div:abp-has(> .bar) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added and example.net#?#div:abp-has(> .bar) is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));
    filterEngine.remove(Filter.fromText("example.net#?#div:abp-has(> .bar)"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#?#div:abp-has(> .bar)")), false);
  });

  // Snippet filters.
  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 is removed", function()
  {
    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 is added and removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  // Multiple snippet filters.
  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 and example.net#$#snippet-2 are added and example.com#$#snippet-1 is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));
    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return true for example.net#$#snippet-2 after example.com#$#snippet-1 and example.net#$#snippet-2 are added and example.com#$#snippet-1 is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));
    filterEngine.remove(Filter.fromText("example.com#$#snippet-1"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#$#snippet-2")), true);
  });

  it("should return true for example.com#$#snippet-1 after example.com#$#snippet-1 and example.net#$#snippet-2 are added and example.net#$#snippet-2 is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));
    filterEngine.remove(Filter.fromText("example.net#$#snippet-2"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), true);
  });

  it("should return false for example.net#$#snippet-2 after example.com#$#snippet-1 and example.net#$#snippet-2 are added and example.net#$#snippet-2 is removed", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));
    filterEngine.remove(Filter.fromText("example.net#$#snippet-2"));

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#$#snippet-2")), false);
  });

  // Comment filters.
  it("should return false for ! CDN-based filters after ! CDN-based filters is removed", function()
  {
    filterEngine.remove(Filter.fromText("! CDN-based filters"));

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  it("should return false for ! CDN-based filters after ! CDN-based filters is added and removed", function()
  {
    filterEngine.add(Filter.fromText("! CDN-based filters"));
    filterEngine.remove(Filter.fromText("! CDN-based filters"));

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  it("should return false for ! CDN-based filters after ! CDN-based filters is added and removed twice", function()
  {
    filterEngine.add(Filter.fromText("! CDN-based filters"));
    filterEngine.remove(Filter.fromText("! CDN-based filters"));
    filterEngine.remove(Filter.fromText("! CDN-based filters"));

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  // Clearance.

  // Blocking filters.
  it("should return false for ||example.com^ after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return false for ||example.com^ after ||example.com^ is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return false for ||example.com^ after ||example.com^ is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  // Multiple blocking filters.
  it("should return false for ||example.com^ after ||example.com^ and ||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return false for ||example.net^ after ||example.com^ and ||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), false);
  });

  // Allowing filters.
  it("should return false for @@||example.com^ after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return false for @@||example.com^ after @@||example.com^ is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return false for @@||example.com^ after @@||example.com^ is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  // Multiple allowing filters.
  it("should return false for @@||example.com^ after @@||example.com^ and @@||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return false for @@||example.net^ after @@||example.com^ and @@||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), false);
  });

  // Blocking and allowing filters.
  it("should return false for ||example.com^ after ||example.com^ and @@||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.com^")), false);
  });

  it("should return false for @@||example.net^ after ||example.com^ and @@||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("||example.com^"));
    filterEngine.add(Filter.fromText("@@||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.net^")), false);
  });

  it("should return false for @@||example.com^ after @@||example.com^ and ||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("@@||example.com^")), false);
  });

  it("should return false for ||example.net^ after @@||example.com^ and ||example.net^ are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("@@||example.com^"));
    filterEngine.add(Filter.fromText("||example.net^"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("||example.net^")), false);
  });

  // Element hiding filters.
  it("should return false for example.com##.foo after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return false for example.com##.foo after example.com##.foo is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return false for example.com##.foo after example.com##.foo is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  // Multiple element hiding filters.
  it("should return false for example.com##.foo after example.com##.foo and example.net##.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return false for example.net##.bar after example.com##.foo and example.net##.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), false);
  });

  // Element hiding exceptions.
  it("should return false for example.com#@#.foo after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return false for example.com#@#.foo after example.com#@#.foo is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return false for example.com#@#.foo after example.com#@#.foo is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  // Multiple element hiding exceptions.
  it("should return false for example.com#@#.foo after example.com#@#.foo and example.net#@#.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return false for example.net#@#.bar after example.com#@#.foo and example.net#@#.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), false);
  });

  // Element hiding filters and exceptions.
  it("should return false for example.com##.foo after example.com##.foo and example.net#@#.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com##.foo")), false);
  });

  it("should return false for example.net#@#.bar after example.com##.foo and example.net#@#.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com##.foo"));
    filterEngine.add(Filter.fromText("example.net#@#.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#@#.bar")), false);
  });

  it("should return false for example.com#@#.foo after example.com#@#.foo and example.net##.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#@#.foo")), false);
  });

  it("should return false for example.net##.bar after example.com#@#.foo and example.net##.bar are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#@#.foo"));
    filterEngine.add(Filter.fromText("example.net##.bar"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net##.bar")), false);
  });

  // Element hiding emulation filters.
  it("should return false for example.com#?#div:abp-has(> .foo) after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  // Multiple element hiding emulation filters.
  it("should return false for example.com#?#div:abp-has(> .foo) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#?#div:abp-has(> .foo)")), false);
  });

  it("should return false for example.net#?#div:abp-has(> .bar) after example.com#?#div:abp-has(> .foo) and example.net#?#div:abp-has(> .bar) are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.net#?#div:abp-has(> .bar)"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#?#div:abp-has(> .bar)")), false);
  });

  // Snippet filters.
  it("should return false for example.com#$#snippet-1 after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  // Multiple snippet filters.
  it("should return false for example.com#$#snippet-1 after example.com#$#snippet-1 and example.net#$#snippet-2 are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.com#$#snippet-1")), false);
  });

  it("should return false for example.net#$#snippet-2 after example.com#$#snippet-1 and example.net#$#snippet-2 are added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
    filterEngine.add(Filter.fromText("example.net#$#snippet-2"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("example.net#$#snippet-2")), false);
  });

  // Comment filters.
  it("should return false for ! CDN-based filters after all filters are cleared", function()
  {
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  it("should return false for ! CDN-based filters after ! CDN-based filters is added and all filters are cleared", function()
  {
    filterEngine.add(Filter.fromText("! CDN-based filters"));
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });

  it("should return false for ! CDN-based filters after ! CDN-based filters is added and all filters are cleared twice", function()
  {
    filterEngine.add(Filter.fromText("! CDN-based filters"));
    filterEngine.clear();
    filterEngine.clear();

    assert.strictEqual(filterEngine.has(Filter.fromText("! CDN-based filters")), false);
  });
});

describe("filterEngine.clear()", function()
{
  beforeEach(function()
  {
    filterEngine.add(Filter.fromText("^foo."));
    filterEngine.add(Filter.fromText("^bar."));
    filterEngine.add(Filter.fromText("##.foo"));
    filterEngine.add(Filter.fromText("example.com#?#div:abp-has(> .foo)"));
    filterEngine.add(Filter.fromText("example.com#$#snippet-1"));
  });

  it("should clear all filters", function()
  {
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "blocking",
        resource: "/bar.js",
        expected: "^bar."
      },
      {
        type: "elemhide",
        expected: [".foo"]
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"]
      },
      {
        type: "snippet",
        expected: ["snippet-1"]
      }
    );

    filterEngine.clear();
    checkFilters({type: "blocking", resource: "/foo.js", expected: null},
                 {type: "blocking", resource: "/bar.js", expected: null},
                 {type: "elemhide", expected: []},
                 {type: "elemhideemulation", expected: []},
                 {type: "snippet", expected: []});
  });

  it("should do nothing if no filters exist", function()
  {
    checkFilters(
      {
        type: "blocking",
        resource: "/foo.js",
        expected: "^foo."
      },
      {
        type: "blocking",
        resource: "/bar.js",
        expected: "^bar."
      },
      {
        type: "elemhide",
        expected: [".foo"]
      },
      {
        type: "elemhideemulation",
        expected: ["div:abp-has(> .foo)"]
      },
      {
        type: "snippet",
        expected: ["snippet-1"]
      }
    );

    filterEngine.clear();
    checkFilters({type: "blocking", resource: "/foo.js", expected: null},
                 {type: "blocking", resource: "/bar.js", expected: null},
                 {type: "elemhide", expected: []},
                 {type: "elemhideemulation", expected: []},
                 {type: "snippet", expected: []});

    // Repeat.
    filterEngine.clear();
    checkFilters({type: "blocking", resource: "/foo.js", expected: null},
                 {type: "blocking", resource: "/bar.js", expected: null},
                 {type: "elemhide", expected: []},
                 {type: "elemhideemulation", expected: []},
                 {type: "snippet", expected: []});
  });
});
