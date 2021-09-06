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

let contentTypes = null;
let Filter = null;
let CombinedMatcher = null;
let defaultMatcher = null;
let Matcher = null;
let parseURL = null;


describe("Matcher", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {contentTypes} = sandboxedRequire("../lib/contentTypes"),
      {Filter} = sandboxedRequire("../lib/filterClasses"),
      {CombinedMatcher, defaultMatcher, Matcher} = sandboxedRequire("../lib/matcher"),
      {parseURL} = sandboxedRequire("../lib/url")
    );
  });

  function compareKeywords(text, expected)
  {
    for (let filter of [Filter.fromText(text), Filter.fromText("@@" + text)])
    {
      let matcher = new Matcher();
      let result = [];
      for (let i = 0; i < expected.length; i++)
      {
        let keyword = matcher.findKeyword(filter);
        result.push(keyword);
        if (keyword)
        {
          let dummyFilter = Filter.fromText("^" + keyword + "^");
          dummyFilter.filterCount = Infinity;
          matcher.add(dummyFilter);
        }
      }

      assert.equal(result.join(", "), expected.join(", "), "Keyword candidates for " + filter.text);
    }
  }

  function checkMatch(filters, location, contentType, docDomain, sitekey, specificOnly, expected, expectedFirstMatch = expected)
  {
    let url = parseURL(location);

    let matcher = new Matcher();
    for (let filter of filters)
      matcher.add(Filter.fromText(filter));

    let result = matcher.match(url, contentTypes[contentType], docDomain, sitekey, specificOnly);
    if (result)
      result = result.text;

    assert.equal(result, expectedFirstMatch, "match(" + location + ", " + contentType + ", " + docDomain + ", " + (sitekey || "no-sitekey") + ", " + (specificOnly ? "specificOnly" : "not-specificOnly") + ") with:\n" + filters.join("\n"));

    let combinedMatcher = new CombinedMatcher();
    for (let i = 0; i < 2; i++)
    {
      for (let filter of filters)
        combinedMatcher.add(Filter.fromText(filter));

      result = combinedMatcher.match(url, contentTypes[contentType], docDomain, sitekey, specificOnly);
      if (result)
        result = result.text;

      assert.equal(result, expected, "combinedMatch(" + location + ", " + contentType + ", " + docDomain + ", " + (sitekey || "no-sitekey") + ", " + (specificOnly ? "specificOnly" : "not-specificOnly") + ") with:\n" + filters.join("\n"));

      // Generic allowing rules can match for specificOnly searches, so we
      // can't easily know which rule will match for these allowlisting tests
      if (specificOnly)
        continue;

      // For next run: add allowing filters for filters that aren't already
      filters = filters.map(text => text.substring(0, 2) == "@@" ? text : "@@" + text);
      if (expected && expected.substring(0, 2) != "@@")
        expected = "@@" + expected;
    }
  }

  function checkSearch(filters, location, contentType, docDomain,
                       sitekey, specificOnly, filterType, expected)
  {
    let url = parseURL(location);

    let matcher = new CombinedMatcher();
    for (let filter of filters)
      matcher.add(Filter.fromText(filter));

    let result = matcher.search(url, contentTypes[contentType],
                                docDomain, sitekey, specificOnly, filterType);
    for (let key in result)
      result[key] = result[key].map(filter => filter.text);

    assert.deepEqual(result, expected, "search(" + location + ", " +
                     contentType + ", " + docDomain + ", " +
                     (sitekey || "no-sitekey") + ", " +
                     (specificOnly ? "specificOnly" : "not-specificOnly") + ", " +
                     filterType + ") with:\n" + filters.join("\n"));
  }

  it("Class definitions", function()
  {
    assert.equal(typeof Matcher, "function", "typeof Matcher");
    assert.equal(typeof CombinedMatcher, "function", "typeof CombinedMatcher");
    assert.equal(typeof defaultMatcher, "object", "typeof defaultMatcher");
    assert.ok(defaultMatcher instanceof CombinedMatcher, "defaultMatcher is a CombinedMatcher instance");
  });

  it("Keyword extraction", function()
  {
    compareKeywords("*", []);
    compareKeywords("asdf", []);
    compareKeywords("/asdf/", []);
    compareKeywords("/asdf1234", []);
    compareKeywords("/asdf/1234", ["asdf"]);
    compareKeywords("/asdf/1234^", ["asdf", "1234"]);
    compareKeywords("/asdf/123456^", ["123456", "asdf"]);
    compareKeywords("^asdf^1234^56as^", ["asdf", "1234", "56as"]);
    compareKeywords("*asdf/1234^", ["1234"]);
    compareKeywords("|asdf,1234*", ["asdf"]);
    compareKeywords("||domain.example^", ["example", "domain"]);
    compareKeywords("&asdf=1234|", ["asdf", "1234"]);
    compareKeywords("^foo%2Ebar^", ["foo%2ebar"]);
    compareKeywords("^aSdF^1234", ["asdf"]);
    compareKeywords("_asdf_1234_", ["asdf", "1234"]);
    compareKeywords("+asdf-1234=", ["asdf", "1234"]);
    compareKeywords("/123^ad2&ad&", ["123", "ad2"]);
    compareKeywords("/123^ad2&ad$script,domain=example.com", ["123", "ad2"]);
  });

  it("Filter matching", function()
  {
    checkMatch([], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["abc"], "http://abc/def", "IMAGE", null, null, false, "abc");
    checkMatch(["abc", "ddd"], "http://abc/def", "IMAGE", null, null, false, "abc");
    checkMatch(["ddd", "abc"], "http://abc/def", "IMAGE", null, null, false, "abc");
    checkMatch(["ddd", "abd"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["abc", "://abc/d"], "http://abc/def", "IMAGE", null, null, false, "://abc/d");
    checkMatch(["://abc/d", "abc"], "http://abc/def", "IMAGE", null, null, false, "://abc/d");
    checkMatch(["|http://"], "http://abc/def", "IMAGE", null, null, false, "|http://");
    checkMatch(["|http://abc"], "http://abc/def", "IMAGE", null, null, false, "|http://abc");
    checkMatch(["|abc"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["|/abc/def"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["/def|"], "http://abc/def", "IMAGE", null, null, false, "/def|");
    checkMatch(["/abc/def|"], "http://abc/def", "IMAGE", null, null, false, "/abc/def|");
    checkMatch(["/abc/|"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["http://abc/|"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["|http://abc/def|"], "http://abc/def", "IMAGE", null, null, false, "|http://abc/def|");
    checkMatch(["|/abc/def|"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["|http://abc/|"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["|/abc/|"], "http://abc/def", "IMAGE", null, null, false, null);
    checkMatch(["||example.com/abc"], "http://example.com/abc/def", "IMAGE", null, null, false, "||example.com/abc");
    checkMatch(["||com/abc/def"], "http://example.com/abc/def", "IMAGE", null, null, false, "||com/abc/def");
    checkMatch(["||com/abc"], "http://example.com/abc/def", "IMAGE", null, null, false, "||com/abc");
    checkMatch(["||com^"], "http://example.com/abc/def", "IMAGE", null, null, false, "||com^");
    checkMatch(["||com^"], "http://com-example.com/abc/def", "IMAGE", null, null, false, "||com^");
    checkMatch(["||mple.com/abc"], "http://example.com/abc/def", "IMAGE", null, null, false, null);
    checkMatch(["||.com/abc/def"], "http://example.com/abc/def", "IMAGE", null, null, false, null);
    checkMatch(["||http://example.com/"], "http://example.com/abc/def", "IMAGE", null, null, false, null);
    checkMatch(["||example.com/abc/def|"], "http://example.com/abc/def", "IMAGE", null, null, false, "||example.com/abc/def|");
    checkMatch(["||com/abc/def|"], "http://example.com/abc/def", "IMAGE", null, null, false, "||com/abc/def|");
    checkMatch(["||example.com/abc|"], "http://example.com/abc/def", "IMAGE", null, null, false, null);
    checkMatch(["abc", "://abc/d", "asdf1234"], "http://abc/def", "IMAGE", null, null, false, "://abc/d");
    checkMatch(["foo*://abc/d", "foo*//abc/de", "://abc/de", "asdf1234"], "http://abc/def", "IMAGE", null, null, false, "://abc/de");
    checkMatch(["abc$third-party", "abc$~third-party", "ddd"], "http://abc/def", "IMAGE", null, null, false, "abc$~third-party");
    checkMatch(["abc$third-party", "abc$~third-party", "ddd"], "http://abc/def", "IMAGE", "other-domain", null, false, "abc$third-party");
    checkMatch(["//abc/def$third-party", "//abc/def$~third-party", "//abc_def"], "http://abc/def", "IMAGE", null, null, false, "//abc/def$~third-party");
    checkMatch(["//abc/def$third-party", "//abc/def$~third-party", "//abc_def"], "http://abc/def", "IMAGE", "other-domain", null, false, "//abc/def$third-party");
    checkMatch(["abc$third-party", "abc$~third-party", "//abc/def"], "http://abc/def", "IMAGE", "other-domain", null, false, "//abc/def");
    checkMatch(["//abc/def", "abc$third-party", "abc$~third-party"], "http://abc/def", "IMAGE", "other-domain", null, false, "//abc/def");
    checkMatch(["abc$third-party", "abc$~third-party", "//abc/def$third-party"], "http://abc/def", "IMAGE", "other-domain", null, false, "//abc/def$third-party");
    checkMatch(["abc$third-party", "abc$~third-party", "//abc/def$third-party"], "http://abc/def", "IMAGE", null, null, false, "abc$~third-party");
    checkMatch(["abc$third-party", "abc$~third-party", "//abc/def$~third-party"], "http://abc/def", "IMAGE", "other-domain", null, false, "abc$third-party");
    checkMatch(["abc$image", "abc$script", "abc$~image"], "http://abc/def", "IMAGE", null, null, false, "abc$image");
    checkMatch(["abc$image", "abc$script", "abc$~script"], "http://abc/def", "SCRIPT", null, null, false, "abc$script");
    checkMatch(["abc$image", "abc$script", "abc$~image"], "http://abc/def", "OTHER", null, null, false, "abc$~image");
    checkMatch(["//abc/def$image", "//abc/def$script", "//abc/def$~image"], "http://abc/def", "IMAGE", null, null, false, "//abc/def$image");
    checkMatch(["//abc/def$image", "//abc/def$script", "//abc/def$~script"], "http://abc/def", "SCRIPT", null, null, false, "//abc/def$script");
    checkMatch(["//abc/def$image", "//abc/def$script", "//abc/def$~image"], "http://abc/def", "OTHER", null, null, false, "//abc/def$~image");
    checkMatch(["abc$image", "abc$~image", "//abc/def"], "http://abc/def", "IMAGE", null, null, false, "//abc/def");
    checkMatch(["//abc/def", "abc$image", "abc$~image"], "http://abc/def", "IMAGE", null, null, false, "//abc/def");
    checkMatch(["abc$image", "abc$~image", "//abc/def$image"], "http://abc/def", "IMAGE", null, null, false, "//abc/def$image");
    checkMatch(["abc$image", "abc$~image", "//abc/def$script"], "http://abc/def", "IMAGE", null, null, false, "abc$image");
    checkMatch(["abc$domain=foo.com", "abc$domain=bar.com", "abc$domain=~foo.com|~bar.com"], "http://foo.com/abc/def", "IMAGE", "foo.com", null, false, "abc$domain=foo.com");
    checkMatch(["abc$domain=foo.com", "abc$domain=bar.com", "abc$domain=~foo.com|~bar.com"], "http://bar.com/abc/def", "IMAGE", "bar.com", null, false, "abc$domain=bar.com");
    checkMatch(["abc$domain=foo.com", "abc$domain=bar.com", "abc$domain=~foo.com|~bar.com"], "http://baz.com/abc/def", "IMAGE", "baz.com", null, false, "abc$domain=~foo.com|~bar.com");
    checkMatch(["abc$domain=foo.com", "cba$domain=bar.com", "ccc$domain=~foo.com|~bar.com"], "http://foo.com/abc/def", "IMAGE", "foo.com", null, false, "abc$domain=foo.com");
    checkMatch(["abc$domain=foo.com", "cba$domain=bar.com", "ccc$domain=~foo.com|~bar.com"], "http://bar.com/abc/def", "IMAGE", "bar.com", null, false, null);
    checkMatch(["abc$domain=foo.com", "cba$domain=bar.com", "ccc$domain=~foo.com|~bar.com"], "http://baz.com/abc/def", "IMAGE", "baz.com", null, false, null);
    checkMatch(["abc$domain=foo.com", "cba$domain=bar.com", "ccc$domain=~foo.com|~bar.com"], "http://baz.com/ccc/def", "IMAGE", "baz.com", null, false, "ccc$domain=~foo.com|~bar.com");
    checkMatch(["abc$sitekey=foo-publickey", "abc$sitekey=bar-publickey"], "http://foo.com/abc/def", "IMAGE", "foo.com", "foo-publickey", false, "abc$sitekey=foo-publickey");
    checkMatch(["abc$sitekey=foo-publickey", "abc$sitekey=bar-publickey"], "http://bar.com/abc/def", "IMAGE", "bar.com", "bar-publickey", false, "abc$sitekey=bar-publickey");
    checkMatch(["abc$sitekey=foo-publickey", "cba$sitekey=bar-publickey"], "http://bar.com/abc/def", "IMAGE", "bar.com", "bar-publickey", false, null);
    checkMatch(["abc$sitekey=foo-publickey", "cba$sitekey=bar-publickey"], "http://baz.com/abc/def", "IMAGE", "baz.com", null, false, null);
    checkMatch(["abc$sitekey=foo-publickey,domain=foo.com", "abc$sitekey=bar-publickey,domain=bar.com"], "http://foo.com/abc/def", "IMAGE", "foo.com", "foo-publickey", false, "abc$sitekey=foo-publickey,domain=foo.com");
    checkMatch(["abc$sitekey=foo-publickey,domain=foo.com", "abc$sitekey=bar-publickey,domain=bar.com"], "http://foo.com/abc/def", "IMAGE", "foo.com", "bar-publickey", false, null);
    checkMatch(["abc$sitekey=foo-publickey,domain=foo.com", "abc$sitekey=bar-publickey,domain=bar.com"], "http://bar.com/abc/def", "IMAGE", "bar.com", "foo-publickey", false, null);
    checkMatch(["abc$sitekey=foo-publickey,domain=foo.com", "abc$sitekey=bar-publickey,domain=bar.com"], "http://bar.com/abc/def", "IMAGE", "bar.com", "bar-publickey", false, "abc$sitekey=bar-publickey,domain=bar.com");
    checkMatch(["@@foo.com$document"], "http://foo.com/bar", "DOCUMENT", "foo.com", null, false, "@@foo.com$document");
    checkMatch(["@@foo.com$elemhide"], "http://foo.com/bar", "ELEMHIDE", "foo.com", null, false, "@@foo.com$elemhide");
    checkMatch(["@@foo.com$generichide"], "http://foo.com/bar", "GENERICHIDE", "foo.com", null, false, "@@foo.com$generichide");
    checkMatch(["@@foo.com$genericblock"], "http://foo.com/bar", "GENERICBLOCK", "foo.com", null, false, "@@foo.com$genericblock");
    checkMatch(["@@bar.com$document"], "http://foo.com/bar", "DOCUMENT", "foo.com", null, false, null);
    checkMatch(["@@bar.com$elemhide"], "http://foo.com/bar", "ELEMHIDE", "foo.com", null, false, null);
    checkMatch(["@@bar.com$generichide"], "http://foo.com/bar", "GENERICHIDE", "foo.com", null, false, null);
    checkMatch(["@@bar.com$genericblock"], "http://foo.com/bar", "GENERICBLOCK", "foo.com", null, false, null);
    checkMatch(["/bar"], "http://foo.com/bar", "IMAGE", "foo.com", null, true, null);
    checkMatch(["/bar$domain=foo.com"], "http://foo.com/bar", "IMAGE", "foo.com", null, true, "/bar$domain=foo.com");
    checkMatch(["@@||foo.com^"], "http://foo.com/bar", "IMAGE", "foo.com", null, false, null, "@@||foo.com^");
    checkMatch(["/bar", "@@||foo.com^"], "http://foo.com/bar", "IMAGE", "foo.com", null, false, "@@||foo.com^");
    checkMatch(["/bar", "@@||foo.com^"], "http://foo.com/foo", "IMAGE", "foo.com", null, false, null, "@@||foo.com^");
    checkMatch(["||foo.com^$popup"], "http://foo.com/bar", "POPUP", "foo.com", null, false, "||foo.com^$popup");
    checkMatch(["@@||foo.com^$popup"], "http://foo.com/bar", "POPUP", "foo.com", null, false, null, "@@||foo.com^$popup");
    checkMatch(["||foo.com^$popup", "@@||foo.com^$popup"], "http://foo.com/bar", "POPUP", "foo.com", null, false, "@@||foo.com^$popup", "||foo.com^$popup");
    checkMatch(["||foo.com^$csp=script-src 'none'"], "http://foo.com/bar", "CSP", "foo.com", null, false, "||foo.com^$csp=script-src 'none'");
    checkMatch(["@@||foo.com^$csp"], "http://foo.com/bar", "CSP", "foo.com", null, false, null, "@@||foo.com^$csp");
    checkMatch(["||foo.com^$csp=script-src 'none'", "@@||foo.com^$csp"], "http://foo.com/bar", "CSP", "foo.com", null, false, "@@||foo.com^$csp", "||foo.com^$csp=script-src 'none'");

    // See #7312.
    checkMatch(["^foo/bar/$script"], "http://foo/bar/", "SCRIPT", "example.com", null, true, null);
    checkMatch(["^foo/bar/$script"], "http://foo/bar/", "SCRIPT", "example.com", null, false, "^foo/bar/$script");
    checkMatch(["^foo/bar/$script,domain=example.com", "@@^foo/bar/$script"], "http://foo/bar/", "SCRIPT", "example.com", null, true, "@@^foo/bar/$script", "^foo/bar/$script,domain=example.com");
    checkMatch(["@@^foo/bar/$script", "^foo/bar/$script,domain=example.com"], "http://foo/bar/", "SCRIPT", "example.com", null, true, "@@^foo/bar/$script", "^foo/bar/$script,domain=example.com");
    checkMatch(["@@^foo/bar/$script", "^foo/bar/$script,domain=example.com"], "http://foo/bar/", "SCRIPT", "example.com", null, false, "@@^foo/bar/$script");

    // See https://gitlab.com/eyeo/adblockplus/adblockpluscore/-/issues/230
    checkMatch(["@@||*$document"], "http://foo/bar/", "DOCUMENT", "example.com", null, false, "@@||*$document");
  });

  it("Filter search", function()
  {
    // Start with three filters: foo, bar$domain=example.com, and @@foo
    let filters = ["foo", "bar$domain=example.com", "@@foo"];

    checkSearch(filters, "http://example.com/foo", "IMAGE", "example.com",
                null, false, "all",
                {blocking: ["foo"], allowing: ["@@foo"]});

    // Blocking only.
    checkSearch(filters, "http://example.com/foo", "IMAGE", "example.com",
                null, false, "blocking", {blocking: ["foo"]});

    // Allowing only.
    checkSearch(filters, "http://example.com/foo", "IMAGE", "example.com",
                null, false, "allowing", {allowing: ["@@foo"]});

    // Different URLs.
    checkSearch(filters, "http://example.com/bar", "IMAGE", "example.com",
                null, false, "all",
                {blocking: ["bar$domain=example.com"], allowing: []});
    checkSearch(filters, "http://example.com/foo/bar", "IMAGE",
                "example.com", null, false, "all", {
                  blocking: ["foo", "bar$domain=example.com"],
                  allowing: ["@@foo"]
                });

    // Non-matching content type.
    checkSearch(filters, "http://example.com/foo", "CSP", "example.com",
                null, false, "all", {blocking: [], allowing: []});

    // Non-matching specificity.
    checkSearch(filters, "http://example.com/foo", "IMAGE", "example.com",
                null, true, "all", {blocking: [], allowing: ["@@foo"]});
  });

  it("Allowlisted", function()
  {
    let matcher = new CombinedMatcher();

    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/foo"),
                                     contentTypes.IMAGE));
    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/bar"),
                                     contentTypes.IMAGE));
    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/foo"),
                                     contentTypes.SUBDOCUMENT));

    matcher.add(Filter.fromText("@@/foo^$image"));

    assert.ok(matcher.isAllowlisted(parseURL("https://example.com/foo"),
                                    contentTypes.IMAGE));
    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/bar"),
                                     contentTypes.IMAGE));
    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/foo"),
                                     contentTypes.SUBDOCUMENT));

    matcher.remove(Filter.fromText("@@/foo^$image"));

    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/foo"),
                                     contentTypes.IMAGE));
    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/bar"),
                                     contentTypes.IMAGE));
    assert.ok(!matcher.isAllowlisted(parseURL("https://example.com/foo"),
                                     contentTypes.SUBDOCUMENT));
  });

  it("Add/remove by keyword", function()
  {
    let matcher = new CombinedMatcher();

    matcher.add(Filter.fromText("||example.com/foo/bar/ad.jpg^"));

    // Add the same filter a second time to make sure it doesn't get added again
    // by a different keyword.
    matcher.add(Filter.fromText("||example.com/foo/bar/ad.jpg^"));

    assert.ok(!!matcher.match(parseURL("https://example.com/foo/bar/ad.jpg"),
                              contentTypes.IMAGE));

    matcher.remove(Filter.fromText("||example.com/foo/bar/ad.jpg^"));

    // Make sure the filter got removed so there is no match.
    assert.ok(!matcher.match(parseURL("https://example.com/foo/bar/ad.jpg"),
                             contentTypes.IMAGE));

    // Map { "example" => { text: "||example.com^$~third-party,image" } }
    matcher.add(Filter.fromText("||example.com^$~third-party,image"));

    assert.equal(matcher._blocking._filterDomainMapsByKeyword.size, 1);

    for (let [key, value] of matcher._blocking._filterDomainMapsByKeyword)
    {
      assert.equal(key, "example");
      assert.deepEqual(value, Filter.fromText("||example.com^$~third-party,image"));
      break;
    }

    assert.ok(!!matcher.match(parseURL("https://example.com/example/ad.jpg"),
                              contentTypes.IMAGE, "example.com"));

    // Map {
    //   "example" => Map {
    //     "" => Map {
    //       { text: "||example.com^$~third-party,image" } => true,
    //       { text: "/example/*$~third-party,image" } => true
    //     }
    //   }
    // }
    matcher.add(Filter.fromText("/example/*$~third-party,image"));

    assert.equal(matcher._blocking._filterDomainMapsByKeyword.size, 1);

    for (let [key, value] of matcher._blocking._filterDomainMapsByKeyword)
    {
      assert.equal(key, "example");
      assert.equal(value.size, 1);

      let map = value.get("");
      assert.equal(map.size, 2);
      assert.equal(map.get(Filter.fromText("||example.com^$~third-party,image")), true);
      assert.equal(map.get(Filter.fromText("/example/*$~third-party,image")), true);

      break;
    }

    assert.ok(!!matcher.match(parseURL("https://example.com/example/ad.jpg"),
                              contentTypes.IMAGE, "example.com"));

    // Map { "example" => { text: "/example/*$~third-party,image" } }
    matcher.remove(Filter.fromText("||example.com^$~third-party,image"));

    assert.equal(matcher._blocking._filterDomainMapsByKeyword.size, 1);

    for (let [key, value] of matcher._blocking._filterDomainMapsByKeyword)
    {
      assert.equal(key, "example");
      assert.deepEqual(value, Filter.fromText("/example/*$~third-party,image"));
      break;
    }

    assert.ok(!!matcher.match(parseURL("https://example.com/example/ad.jpg"),
                              contentTypes.IMAGE, "example.com"));

    // Map {}
    matcher.remove(Filter.fromText("/example/*$~third-party,image"));

    assert.equal(matcher._blocking._filterDomainMapsByKeyword.size, 0);

    assert.ok(!matcher.match(parseURL("https://example.com/example/ad.jpg"),
                             contentTypes.IMAGE, "example.com"));
  });

  it("Quick check", function()
  {
    let matcher = new CombinedMatcher();

    // Keywords "example" and "foo".
    matcher.add(Filter.fromText("||example/foo^"));
    matcher.add(Filter.fromText("||example/foo."));
    matcher.add(Filter.fromText("||example/foo/"));

    // Blank keyword.
    matcher.add(Filter.fromText("/ad."));
    matcher.add(Filter.fromText("/\\bbar\\b/$match-case"));

    assert.ok(!!matcher.match(parseURL("https://example/foo/"),
                              contentTypes.IMAGE, "example"));

    // The request URL contains neither "example" nor "foo", therefore it should
    // get to the filters associated with the blank keyword.
    // https://gitlab.com/eyeo/adblockplus/adblockpluscore/issues/13
    assert.ok(!!matcher.match(parseURL("https://adblockplus/bar/"),
                              contentTypes.IMAGE, "adblockplus"));
    assert.ok(!matcher.match(parseURL("https://adblockplus/Bar/"),
                             contentTypes.IMAGE, "adblockplus"));
  });
});
