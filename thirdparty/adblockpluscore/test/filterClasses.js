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
let RESOURCE_TYPES = null;
let Filter = null;
let InvalidFilter = null;
let CommentFilter = null;
let ActiveFilter = null;
let URLFilter = null;
let BlockingFilter = null;
let ContentFilter = null;
let AllowingFilter = null;
let ElemHideBase = null;
let ElemHideFilter = null;
let ElemHideException = null;
let ElemHideEmulationFilter = null;
let SnippetFilter = null;

describe("Filter classes", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {contentTypes, RESOURCE_TYPES} = sandboxedRequire("../lib/contentTypes"),
      {Filter, InvalidFilter, CommentFilter, ActiveFilter, URLFilter,
       BlockingFilter, AllowingFilter, ContentFilter, ElemHideBase,
       ElemHideFilter, ElemHideException, ElemHideEmulationFilter,
       SnippetFilter} = sandboxedRequire("../lib/filterClasses")
    );
  });

  function serializeFilter(filter)
  {
    // Filter serialization only writes out essential properties, need to do a full serialization here
    let result = [];
    result.push("text=" + filter.text);
    result.push("type=" + filter.type);
    if (filter instanceof InvalidFilter)
    {
      result.push("reason=" + filter.reason);
    }
    else if (filter instanceof CommentFilter)
    {
    }
    else if (filter instanceof ActiveFilter)
    {
      result.push("disabled=" + filter.disabled);
      result.push("lastHit=" + filter.lastHit);
      result.push("hitCount=" + filter.hitCount);

      let domains = [];
      if (filter.domains)
      {
        for (let [domain, isIncluded] of filter.domains)
        {
          if (domain != "")
            domains.push(isIncluded ? domain : "~" + domain);
        }
      }
      result.push("domains=" + domains.sort().join("|"));

      if (filter instanceof URLFilter)
      {
        result.push("regexp=" + (filter.regexp ? filter.regexp.source : null));
        result.push("contentType=" + filter.contentType);
        result.push("matchCase=" + filter.matchCase);

        let sitekeys = filter.sitekeys || [];
        result.push("sitekeys=" + sitekeys.slice().sort().join("|"));

        result.push("thirdParty=" + filter.thirdParty);
        if (filter instanceof BlockingFilter)
        {
          result.push("csp=" + filter.csp);
          result.push("rewrite=" + filter.rewrite);
        }
        else if (filter instanceof AllowingFilter)
        {
        }
      }
      else if (filter instanceof ElemHideBase)
      {
        result.push("selectorDomains=" +
                    [...filter.domains || []]
                    .filter(([domain, isIncluded]) => isIncluded)
                    .map(([domain]) => domain.toLowerCase()));
        result.push("selector=" + filter.selector);
      }
      else if (filter instanceof SnippetFilter)
      {
        result.push("scriptDomains=" +
                    [...filter.domains || []]
                    .filter(([domain, isIncluded]) => isIncluded)
                    .map(([domain]) => domain.toLowerCase()));
        result.push("script=" + filter.script);
      }
    }
    return result;
  }

  function addDefaults(expected)
  {
    let type = null;
    let hasProperty = {};
    for (let entry of expected)
    {
      if (/^type=(.*)/.test(entry))
        type = RegExp.$1;
      else if (/^(\w+)/.test(entry))
        hasProperty[RegExp.$1] = true;
    }

    function addProperty(prop, value)
    {
      if (!(prop in hasProperty))
        expected.push(prop + "=" + value);
    }

    if (type == "allowing" || type == "blocking" || type == "elemhide" ||
        type == "elemhideexception" || type == "elemhideemulation" ||
        type == "snippet")
    {
      addProperty("disabled", "false");
      addProperty("lastHit", "0");
      addProperty("hitCount", "0");
    }
    if (type == "allowing" || type == "blocking")
    {
      addProperty("contentType", RESOURCE_TYPES);
      addProperty("regexp", "null");
      addProperty("matchCase", "false");
      addProperty("thirdParty", "null");
      addProperty("domains", "");
      addProperty("sitekeys", "");
    }
    if (type == "blocking")
    {
      addProperty("csp", "null");
      addProperty("rewrite", "null");
    }
    if (type == "elemhide" || type == "elemhideexception" ||
        type == "elemhideemulation")
    {
      addProperty("selectorDomains", "");
      addProperty("domains", "");
    }
    if (type == "snippet")
    {
      addProperty("scriptDomains", "");
      addProperty("domains", "");
    }
  }

  function compareFilter(text, expected, postInit)
  {
    addDefaults(expected);

    let filter = Filter.fromText(text);
    if (postInit)
      postInit(filter);
    let result = serializeFilter(filter);
    assert.equal(result.sort().join("\n"), expected.sort().join("\n"), text);

    // Test round-trip
    let filter2;
    let buffer = [...filter.serialize()];
    if (buffer.length)
    {
      let map = Object.create(null);
      for (let line of buffer.slice(1))
      {
        if (/(.*?)=(.*)/.test(line))
          map[RegExp.$1] = RegExp.$2;
      }
      filter2 = Filter.fromObject(map);
    }
    else
    {
      filter2 = Filter.fromText(filter.text);
    }

    assert.equal(serializeFilter(filter).join("\n"), serializeFilter(filter2).join("\n"), text + " deserialization");
  }

  it("Definitions", function()
  {
    assert.equal(typeof Filter, "function", "typeof Filter");
    assert.equal(typeof InvalidFilter, "function", "typeof InvalidFilter");
    assert.equal(typeof CommentFilter, "function", "typeof CommentFilter");
    assert.equal(typeof ActiveFilter, "function", "typeof ActiveFilter");
    assert.equal(typeof URLFilter, "function", "typeof URLFilter");
    assert.equal(typeof BlockingFilter, "function", "typeof BlockingFilter");
    assert.equal(typeof ContentFilter, "function", "typeof ContentFilter");
    assert.equal(typeof AllowingFilter, "function", "typeof AllowingFilter");
    assert.equal(typeof ElemHideBase, "function", "typeof ElemHideBase");
    assert.equal(typeof ElemHideFilter, "function", "typeof ElemHideFilter");
    assert.equal(typeof ElemHideException, "function", "typeof ElemHideException");
    assert.equal(typeof ElemHideEmulationFilter, "function",
                 "typeof ElemHideEmulationFilter");
    assert.equal(typeof SnippetFilter, "function", "typeof SnippetFilter");
  });

  it("Comments", function()
  {
    compareFilter("!asdf", ["type=comment", "text=!asdf"]);
    compareFilter("!foo#bar", ["type=comment", "text=!foo#bar"]);
    compareFilter("!foo##bar", ["type=comment", "text=!foo##bar"]);
  });

  it("Invalid filters", function()
  {
    compareFilter("/??/", ["type=invalid", "text=/??/", "reason=filter_invalid_regexp"]);
    compareFilter("asd$foobar", ["type=invalid", "text=asd$foobar", "reason=filter_unknown_option"]);

    // No $domain or $~third-party
    compareFilter("||example.com/ad.js$rewrite=abp-resource:noopjs", ["type=invalid", "text=||example.com/ad.js$rewrite=abp-resource:noopjs", "reason=filter_invalid_rewrite"]);
    compareFilter("*example.com/ad.js$rewrite=abp-resource:noopjs", ["type=invalid", "text=*example.com/ad.js$rewrite=abp-resource:noopjs", "reason=filter_invalid_rewrite"]);
    compareFilter("example.com/ad.js$rewrite=abp-resource:noopjs", ["type=invalid", "text=example.com/ad.js$rewrite=abp-resource:noopjs", "reason=filter_invalid_rewrite"]);
    // Patterns not starting with || or *
    compareFilter("example.com/ad.js$rewrite=abp-resource:noopjs,domain=foo.com", ["type=invalid", "text=example.com/ad.js$rewrite=abp-resource:noopjs,domain=foo.com", "reason=filter_invalid_rewrite"]);
    compareFilter("example.com/ad.js$rewrite=abp-resource:noopjs,~third-party", ["type=invalid", "text=example.com/ad.js$rewrite=abp-resource:noopjs,~third-party", "reason=filter_invalid_rewrite"]);
    // $~third-party requires ||
    compareFilter("*example.com/ad.js$rewrite=abp-resource:noopjs,~third-party", ["type=invalid", "text=*example.com/ad.js$rewrite=abp-resource:noopjs,~third-party", "reason=filter_invalid_rewrite"]);

    function checkElemHideEmulationFilterInvalid(domains)
    {
      let filterText = domains + "#?#:-abp-properties(abc)";
      compareFilter(
        filterText, [
          "type=invalid", "text=" + filterText,
          "reason=filter_elemhideemulation_nodomain"
        ]
      );
    }
    checkElemHideEmulationFilterInvalid("");
    checkElemHideEmulationFilterInvalid("~foo.com");
    checkElemHideEmulationFilterInvalid("~foo.com,~bar.com");
    checkElemHideEmulationFilterInvalid("foo");
    checkElemHideEmulationFilterInvalid("~foo.com,bar");
  });

  it("Filters with state", function()
  {
    compareFilter("blabla", ["type=blocking", "text=blabla"]);
    compareFilter(
      "blabla_default", ["type=blocking", "text=blabla_default"],
      filter =>
      {
        filter.disabled = false;
        filter.hitCount = 0;
        filter.lastHit = 0;
      }
    );
    compareFilter(
      "blabla_non_default",
      ["type=blocking", "text=blabla_non_default", "disabled=true", "hitCount=12", "lastHit=20"],
      filter =>
      {
        filter.disabled = true;
        filter.hitCount = 12;
        filter.lastHit = 20;
      }
    );
  });

  it("Special characters", function()
  {
    compareFilter("/ddd|f?a[s]d/", ["type=blocking", "text=/ddd|f?a[s]d/", "regexp=ddd|f?a[s]d"]);
    compareFilter("*asdf*d**dd*", ["type=blocking", "text=*asdf*d**dd*", "regexp=asdf.*d.*dd"]);
    compareFilter("|*asd|f*d**dd*|", ["type=blocking", "text=|*asd|f*d**dd*|", "regexp=^.*asd\\|f.*d.*dd.*$"]);
    compareFilter("dd[]{}$%<>&()*d", ["type=blocking", "text=dd[]{}$%<>&()*d", "regexp=dd\\[\\]\\{\\}\\$\\%\\<\\>\\&\\(\\).*d"]);

    compareFilter("@@/ddd|f?a[s]d/", ["type=allowing", "text=@@/ddd|f?a[s]d/", "regexp=ddd|f?a[s]d", "contentType=" + RESOURCE_TYPES]);
    compareFilter("@@*asdf*d**dd*", ["type=allowing", "text=@@*asdf*d**dd*", "regexp=asdf.*d.*dd", "contentType=" + RESOURCE_TYPES]);
    compareFilter("@@|*asd|f*d**dd*|", ["type=allowing", "text=@@|*asd|f*d**dd*|", "regexp=^.*asd\\|f.*d.*dd.*$", "contentType=" + RESOURCE_TYPES]);
    compareFilter("@@dd[]{}$%<>&()*d", ["type=allowing", "text=@@dd[]{}$%<>&()*d", "regexp=dd\\[\\]\\{\\}\\$\\%\\<\\>\\&\\(\\).*d", "contentType=" + RESOURCE_TYPES]);
  });

  it("Filter options", function()
  {
    compareFilter("bla$match-case,csp=first csp,script,other,third-party,domain=FOO.cOm,sitekey=foo", ["type=blocking", "text=bla$match-case,csp=first csp,script,other,third-party,domain=FOO.cOm,sitekey=foo", "matchCase=true", "contentType=" + (contentTypes.SCRIPT | contentTypes.OTHER | contentTypes.CSP), "thirdParty=true", "domains=foo.com", "sitekeys=FOO", "csp=first csp"]);
    compareFilter("bla$~match-case,~csp=csp,~script,~other,~third-party,domain=~bAr.coM", ["type=blocking", "text=bla$~match-case,~csp=csp,~script,~other,~third-party,domain=~bAr.coM", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER)), "thirdParty=false", "domains=~bar.com"]);
    compareFilter("@@bla$match-case,script,other,third-party,domain=foo.com|bar.com|~bAR.foO.Com|~Foo.Bar.com,csp=c s p,sitekey=foo|bar", ["type=allowing", "text=@@bla$match-case,script,other,third-party,domain=foo.com|bar.com|~bAR.foO.Com|~Foo.Bar.com,csp=c s p,sitekey=foo|bar", "matchCase=true", "contentType=" + (contentTypes.SCRIPT | contentTypes.OTHER | contentTypes.CSP), "thirdParty=true", "domains=bar.com|foo.com|~bar.foo.com|~foo.bar.com", "sitekeys=BAR|FOO"]);
    compareFilter("@@bla$match-case,script,other,third-party,domain=foo.com|bar.com|~bar.foo.com|~foo.bar.com,sitekey=foo|bar", ["type=allowing", "text=@@bla$match-case,script,other,third-party,domain=foo.com|bar.com|~bar.foo.com|~foo.bar.com,sitekey=foo|bar", "matchCase=true", "contentType=" + (contentTypes.SCRIPT | contentTypes.OTHER), "thirdParty=true", "domains=bar.com|foo.com|~bar.foo.com|~foo.bar.com", "sitekeys=BAR|FOO"]);

    compareFilter("||example.com/ad.js$rewrite=abp-resource:noopjs,domain=foo.com|bar.com", ["type=blocking", "text=||example.com/ad.js$rewrite=abp-resource:noopjs,domain=foo.com|bar.com", "regexp=null", "matchCase=false", "rewrite=noopjs", "contentType=" + (RESOURCE_TYPES), "domains=bar.com|foo.com"]);
    compareFilter("*example.com/ad.js$rewrite=abp-resource:noopjs,domain=foo.com|bar.com", ["type=blocking", "text=*example.com/ad.js$rewrite=abp-resource:noopjs,domain=foo.com|bar.com", "regexp=null", "matchCase=false", "rewrite=noopjs", "contentType=" + (RESOURCE_TYPES), "domains=bar.com|foo.com"]);
    compareFilter("||example.com/ad.js$rewrite=abp-resource:noopjs,~third-party", ["type=blocking", "text=||example.com/ad.js$rewrite=abp-resource:noopjs,~third-party", "regexp=null", "matchCase=false", "rewrite=noopjs", "thirdParty=false", "contentType=" + (RESOURCE_TYPES)]);
    compareFilter("||content.server.com/files/*.php$rewrite=$1", ["type=invalid", "reason=filter_invalid_rewrite", "text=||content.server.com/files/*.php$rewrite=$1"]);
    compareFilter("||content.server.com/files/*.php$rewrite=", ["type=invalid", "reason=filter_invalid_rewrite", "text=||content.server.com/files/*.php$rewrite="]);

    // background and image should be the same for backwards compatibility
    compareFilter("bla$image", ["type=blocking", "text=bla$image", "contentType=" + (contentTypes.IMAGE)]);
    compareFilter("bla$background", ["type=blocking", "text=bla$background", "contentType=" + (contentTypes.IMAGE)]);
    compareFilter("bla$~image", ["type=blocking", "text=bla$~image", "contentType=" + (RESOURCE_TYPES & ~contentTypes.IMAGE)]);
    compareFilter("bla$~background", ["type=blocking", "text=bla$~background", "contentType=" + (RESOURCE_TYPES & ~contentTypes.IMAGE)]);

    compareFilter("@@bla$~script,~other", ["type=allowing", "text=@@bla$~script,~other", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER))]);
    compareFilter("@@http://bla$~script,~other", ["type=allowing", "text=@@http://bla$~script,~other", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER))]);
    compareFilter("@@ftp://bla$~script,~other", ["type=allowing", "text=@@ftp://bla$~script,~other", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER))]);
    compareFilter("@@bla$~script,~other,document", ["type=allowing", "text=@@bla$~script,~other,document", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER) | contentTypes.DOCUMENT)]);
    compareFilter("@@bla$~script,~other,~document", ["type=allowing", "text=@@bla$~script,~other,~document", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER))]);
    compareFilter("@@bla$document", ["type=allowing", "text=@@bla$document", "contentType=" + contentTypes.DOCUMENT]);
    compareFilter("@@bla$~script,~other,elemhide", ["type=allowing", "text=@@bla$~script,~other,elemhide", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER) | contentTypes.ELEMHIDE)]);
    compareFilter("@@bla$~script,~other,~elemhide", ["type=allowing", "text=@@bla$~script,~other,~elemhide", "contentType=" + (RESOURCE_TYPES & ~(contentTypes.SCRIPT | contentTypes.OTHER))]);
    compareFilter("@@bla$elemhide", ["type=allowing", "text=@@bla$elemhide", "contentType=" + contentTypes.ELEMHIDE]);

    compareFilter("@@bla$~script,~other,donottrack", ["type=invalid", "text=@@bla$~script,~other,donottrack", "reason=filter_unknown_option"]);
    compareFilter("@@bla$~script,~other,~donottrack", ["type=invalid", "text=@@bla$~script,~other,~donottrack", "reason=filter_unknown_option"]);
    compareFilter("@@bla$donottrack", ["type=invalid", "text=@@bla$donottrack", "reason=filter_unknown_option"]);
    compareFilter("@@bla$foobar", ["type=invalid", "text=@@bla$foobar", "reason=filter_unknown_option"]);
    compareFilter("@@bla$image,foobar", ["type=invalid", "text=@@bla$image,foobar", "reason=filter_unknown_option"]);
    compareFilter("@@bla$foobar,image", ["type=invalid", "text=@@bla$foobar,image", "reason=filter_unknown_option"]);

    compareFilter("bla$csp", ["type=invalid", "text=bla$csp", "reason=filter_invalid_csp"]);
    compareFilter("bla$csp=", ["type=invalid", "text=bla$csp=", "reason=filter_invalid_csp"]);

    // Blank CSP values are allowed for allowing filters.
    compareFilter("@@bla$csp", ["type=allowing", "text=@@bla$csp", "contentType=" + contentTypes.CSP]);
    compareFilter("@@bla$csp=", ["type=allowing", "text=@@bla$csp=", "contentType=" + contentTypes.CSP]);

    compareFilter("bla$csp=report-uri", ["type=invalid", "text=bla$csp=report-uri", "reason=filter_invalid_csp"]);
    compareFilter("bla$csp=foo,csp=report-to", ["type=invalid", "text=bla$csp=foo,csp=report-to", "reason=filter_invalid_csp"]);
    compareFilter("bla$csp=foo,csp=referrer foo", ["type=invalid", "text=bla$csp=foo,csp=referrer foo", "reason=filter_invalid_csp"]);
    compareFilter("bla$csp=foo,csp=base-uri", ["type=invalid", "text=bla$csp=foo,csp=base-uri", "reason=filter_invalid_csp"]);
    compareFilter("bla$csp=foo,csp=upgrade-insecure-requests", ["type=invalid", "text=bla$csp=foo,csp=upgrade-insecure-requests", "reason=filter_invalid_csp"]);
    compareFilter("bla$csp=foo,csp=ReFeRReR", ["type=invalid", "text=bla$csp=foo,csp=ReFeRReR", "reason=filter_invalid_csp"]);
  });

  it("Element hiding rules", function()
  {
    compareFilter("##ddd", ["type=elemhide", "text=##ddd", "selector=ddd"]);
    compareFilter("##body > div:first-child", ["type=elemhide", "text=##body > div:first-child", "selector=body > div:first-child"]);
    compareFilter("fOO##ddd", ["type=elemhide", "text=fOO##ddd", "selectorDomains=foo", "selector=ddd", "domains=foo"]);
    compareFilter("Foo,bAr##ddd", ["type=elemhide", "text=Foo,bAr##ddd", "selectorDomains=foo,bar", "selector=ddd", "domains=bar|foo"]);
    compareFilter("foo,~baR##ddd", ["type=elemhide", "text=foo,~baR##ddd", "selectorDomains=foo", "selector=ddd", "domains=foo|~bar"]);
    compareFilter("foo,~baz,bar##ddd", ["type=elemhide", "text=foo,~baz,bar##ddd", "selectorDomains=foo,bar", "selector=ddd", "domains=bar|foo|~baz"]);
  });

  it("Element hiding exceptions", function()
  {
    compareFilter("#@#ddd", ["type=elemhideexception", "text=#@#ddd", "selector=ddd"]);
    compareFilter("#@#body > div:first-child", ["type=elemhideexception", "text=#@#body > div:first-child", "selector=body > div:first-child"]);
    compareFilter("fOO#@#ddd", ["type=elemhideexception", "text=fOO#@#ddd", "selectorDomains=foo", "selector=ddd", "domains=foo"]);
    compareFilter("Foo,bAr#@#ddd", ["type=elemhideexception", "text=Foo,bAr#@#ddd", "selectorDomains=foo,bar", "selector=ddd", "domains=bar|foo"]);
    compareFilter("foo,~baR#@#ddd", ["type=elemhideexception", "text=foo,~baR#@#ddd", "selectorDomains=foo", "selector=ddd", "domains=foo|~bar"]);
    compareFilter("foo,~baz,bar#@#ddd", ["type=elemhideexception", "text=foo,~baz,bar#@#ddd", "selectorDomains=foo,bar", "selector=ddd", "domains=bar|foo|~baz"]);
  });

  it("Element hiding emulation filters", function()
  {
    // Check valid domain combinations
    compareFilter("fOO.cOm#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=fOO.cOm#?#:-abp-properties(abc)", "selectorDomains=foo.com", "selector=:-abp-properties(abc)", "domains=foo.com"]);
    compareFilter("Foo.com,~bAr.com#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=Foo.com,~bAr.com#?#:-abp-properties(abc)", "selectorDomains=foo.com", "selector=:-abp-properties(abc)", "domains=foo.com|~bar.com"]);
    compareFilter("foo.com,~baR#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=foo.com,~baR#?#:-abp-properties(abc)", "selectorDomains=foo.com", "selector=:-abp-properties(abc)", "domains=foo.com|~bar"]);
    compareFilter("~foo.com,bar.com#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=~foo.com,bar.com#?#:-abp-properties(abc)", "selectorDomains=bar.com", "selector=:-abp-properties(abc)", "domains=bar.com|~foo.com"]);

    // Check some special cases
    compareFilter("#?#:-abp-properties(abc)", ["type=invalid", "text=#?#:-abp-properties(abc)", "reason=filter_elemhideemulation_nodomain"]);
    compareFilter("foo.com#?#abc", ["type=elemhideemulation", "text=foo.com#?#abc", "selectorDomains=foo.com", "selector=abc", "domains=foo.com"]);
    compareFilter("foo.com#?#:-abp-foobar(abc)", ["type=elemhideemulation", "text=foo.com#?#:-abp-foobar(abc)", "selectorDomains=foo.com", "selector=:-abp-foobar(abc)", "domains=foo.com"]);
    compareFilter("foo.com#?#aaa :-abp-properties(abc) bbb", ["type=elemhideemulation", "text=foo.com#?#aaa :-abp-properties(abc) bbb", "selectorDomains=foo.com", "selector=aaa :-abp-properties(abc) bbb", "domains=foo.com"]);
    compareFilter("foo.com#?#:-abp-properties(|background-image: url(data:*))", ["type=elemhideemulation", "text=foo.com#?#:-abp-properties(|background-image: url(data:*))", "selectorDomains=foo.com", "selector=:-abp-properties(|background-image: url(data:*))", "domains=foo.com"]);

    // Support element hiding emulation filters for localhost (#6931).
    compareFilter("localhost#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=localhost#?#:-abp-properties(abc)", "selectorDomains=localhost", "selector=:-abp-properties(abc)", "domains=localhost"]);
    compareFilter("localhost,~www.localhost#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=localhost,~www.localhost#?#:-abp-properties(abc)", "selectorDomains=localhost", "selector=:-abp-properties(abc)", "domains=localhost|~www.localhost"]);
    compareFilter("~www.localhost,localhost#?#:-abp-properties(abc)", ["type=elemhideemulation", "text=~www.localhost,localhost#?#:-abp-properties(abc)", "selectorDomains=localhost", "selector=:-abp-properties(abc)", "domains=localhost|~www.localhost"]);
  });

  it("Empty element hiding domains", function()
  {
    let emptyDomainFilters = [
      ",##selector", ",,,##selector", "~,foo.com##selector", "foo.com,##selector",
      ",foo.com##selector", "foo.com,~##selector",
      "foo.com,,bar.com##selector", "foo.com,~,bar.com##selector"
    ];

    for (let filterText of emptyDomainFilters)
    {
      let filter = Filter.fromText(filterText);
      assert.ok(filter instanceof InvalidFilter);
      assert.equal(filter.reason, "filter_invalid_domain");
    }
  });

  it("Element hiding rules with braces", function()
  {
    compareFilter(
      "###foo{color: red}", [
        "type=elemhide",
        "text=###foo{color: red}",
        "selectorDomains=",
        "selector=#foo{color: red}",
        "domains="
      ]
    );
    compareFilter(
      "foo.com#?#:-abp-properties(/margin: [3-4]{2}/)", [
        "type=elemhideemulation",
        "text=foo.com#?#:-abp-properties(/margin: [3-4]{2}/)",
        "selectorDomains=foo.com",
        "selector=:-abp-properties(/margin: [3-4]{2}/)",
        "domains=foo.com"
      ]
    );
  });

  it("Snippet filters", function()
  {
    compareFilter("foo.com#$#abc", ["type=snippet", "text=foo.com#$#abc", "scriptDomains=foo.com", "script=abc", "domains=foo.com"]);
    compareFilter("foo.com,~bar.com#$#abc", ["type=snippet", "text=foo.com,~bar.com#$#abc", "scriptDomains=foo.com", "script=abc", "domains=foo.com|~bar.com"]);
    compareFilter("foo.com,~bar#$#abc", ["type=snippet", "text=foo.com,~bar#$#abc", "scriptDomains=foo.com", "script=abc", "domains=foo.com|~bar"]);
    compareFilter("~foo.com,bar.com#$#abc", ["type=snippet", "text=~foo.com,bar.com#$#abc", "scriptDomains=bar.com", "script=abc", "domains=bar.com|~foo.com"]);
  });

  it("Filter normalization", function()
  {
    // Line breaks etc
    assert.equal(Filter.normalize("\n\t\nad\ns"),
                 "ads");

    // Comment filters
    assert.equal(Filter.normalize("   !  fo  o##  bar   "),
                 "!  fo  o##  bar");

    // Element hiding filters
    assert.equal(Filter.normalize("   domain.c  om## # sele ctor   "),
                 "domain.com### sele ctor");

    // Wildcard: "*" is allowed, though not supported (yet).
    assert.equal(Filter.normalize("   domain.*## # sele ctor   "),
                 "domain.*### sele ctor");

    // Element hiding emulation filters
    assert.equal(Filter.normalize("   domain.c  om#?# # sele ctor   "),
                 "domain.com#?## sele ctor");

    // Wildcard: "*" is allowed, though not supported (yet).
    assert.equal(Filter.normalize("   domain.*#?# # sele ctor   "),
                 "domain.*#?## sele ctor");

    // Incorrect syntax: the separator "#?#" cannot contain spaces; treated as a
    // regular filter instead
    assert.equal(Filter.normalize("   domain.c  om# ?#. sele ctor   "),
                 "domain.com#?#.selector");
    // Incorrect syntax: the separator "#?#" cannot contain spaces; treated as an
    // element hiding filter instead, because the "##" following the "?" is taken
    // to be the separator instead
    assert.equal(Filter.normalize("   domain.c  om# ?##sele ctor   "),
                 "domain.com#?##sele ctor");

    // Element hiding exception filters
    assert.equal(Filter.normalize("   domain.c  om#@# # sele ctor   "),
                 "domain.com#@## sele ctor");

    // Wildcard: "*" is allowed, though not supported (yet).
    assert.equal(Filter.normalize("   domain.*#@# # sele ctor   "),
                 "domain.*#@## sele ctor");

    // Incorrect syntax: the separator "#@#" cannot contain spaces; treated as a
    // regular filter instead (not an element hiding filter either!), because
    // unlike the case with "# ?##" the "##" following the "@" is not considered
    // to be a separator
    assert.equal(Filter.normalize("   domain.c  om# @## sele ctor   "),
                 "domain.com#@##selector");

    // Snippet filters
    assert.equal(Filter.normalize("   domain.c  om#$#  sni pp  et   "),
                 "domain.com#$#sni pp  et");

    // Wildcard: "*" is allowed, though not supported (yet).
    assert.equal(Filter.normalize("   domain.*#$#  sni pp  et   "),
                 "domain.*#$#sni pp  et");

    // Regular filters
    let normalized = Filter.normalize(
      "    b$l 	 a$sitekey=  foo  ,domain= do main.com |foo   .com,c sp= c   s p  "
    );
    assert.equal(
      normalized,
      "b$la$sitekey=foo,domain=domain.com|foo.com,csp=c s p"
    );
    compareFilter(
      normalized, [
        "type=blocking",
        "text=" + normalized,
        "csp=c s p",
        "domains=domain.com|foo.com",
        "sitekeys=FOO",
        "contentType=" + contentTypes.CSP
      ]
    );

    // Some $csp edge cases
    assert.equal(Filter.normalize("$csp=  "),
                 "$csp=");
    assert.equal(Filter.normalize("$csp= c s p"),
                 "$csp=c s p");
    assert.equal(Filter.normalize("$$csp= c s p"),
                 "$$csp=c s p");
    assert.equal(Filter.normalize("$$$csp= c s p"),
                 "$$$csp=c s p");
    assert.equal(Filter.normalize("foo?csp=b a r$csp=script-src  'self'"),
                 "foo?csp=bar$csp=script-src 'self'");
    assert.equal(Filter.normalize("foo$bar=c s p = ba z,cs p = script-src  'self'"),
                 "foo$bar=csp=baz,csp=script-src 'self'");
    assert.equal(Filter.normalize("foo$csp=c s p csp= ba z,cs p  = script-src  'self'"),
                 "foo$csp=c s p csp= ba z,csp=script-src 'self'");
    assert.equal(Filter.normalize("foo$csp=bar,$c sp=c s p"),
                 "foo$csp=bar,$csp=c s p");
    assert.equal(Filter.normalize(" f o   o   $      bar   $csp=ba r"),
                 "foo$bar$csp=ba r");
    assert.equal(Filter.normalize("f    $    o    $    o    $    csp=f o o "),
                 "f$o$o$csp=f o o");
    assert.equal(Filter.normalize("/foo$/$ csp = script-src  http://example.com/?$1=1&$2=2&$3=3"),
                 "/foo$/$csp=script-src http://example.com/?$1=1&$2=2&$3=3");
    assert.equal(Filter.normalize("||content.server.com/files/*.php$rewrite= $1"),
                 "||content.server.com/files/*.php$rewrite=$1");
  });

  it("Filter rewrite option", function()
  {
    let text = "/(content\\.server\\/file\\/.*\\.txt)\\?.*$/$rewrite=$1";
    let filter = Filter.fromText(text);
    assert.ok(filter instanceof InvalidFilter);
    assert.equal(filter.type, "invalid");
    assert.equal(filter.reason, "filter_invalid_rewrite");

    text = "||/(content\\.server\\/file\\/.*\\.txt)\\?.*$/$rewrite=blank-text,domains=content.server";
    filter = Filter.fromText(text);
    assert.ok(filter instanceof InvalidFilter);
    assert.equal(filter.type, "invalid");
    assert.equal(filter.reason, "filter_invalid_rewrite");

    const rewriteTestCases = require("./data/rewrite.json");
    for (let {resource, expected} of rewriteTestCases)
    {
      text = `||/(content\\.server\\/file\\/.*\\.txt)\\?.*$/$rewrite=abp-resource:${resource},domain=content.server`;
      filter = Filter.fromText(text);
      assert.equal(filter.rewriteUrl("http://content.server/file/foo.txt"),
                   expected);
      assert.equal(filter.rewriteUrl("http://content.server/file/foo.txt?bar"),
                   expected);
    }
  });

  it("Domain map deduplication", function()
  {
    let filter1 = Filter.fromText("foo$domain=blocking.example.com");
    let filter2 = Filter.fromText("bar$domain=blocking.example.com");
    let filter3 = Filter.fromText("elemhide.example.com##.foo");
    let filter4 = Filter.fromText("elemhide.example.com##.bar");

    // This compares the references to make sure that both refer to the same
    // object (#6815).

    assert.equal(filter1.domains, filter2.domains);
    assert.equal(filter3.domains, filter4.domains);

    let filter5 = Filter.fromText("bar$domain=www.example.com");
    let filter6 = Filter.fromText("www.example.com##.bar");

    assert.notEqual(filter2.domains, filter5.domains);
    assert.notEqual(filter4.domains, filter6.domains);
  });

  it("Filters with wildcard domains", function()
  {
    // Blocking filters
    compareFilter("||example.com^$domain=example.*", [
      "type=blocking",
      "text=||example.com^$domain=example.*",
      "domains=example.*"
    ]);

    compareFilter("||example.com^$domain=example.*|example.net", [
      "type=blocking",
      "text=||example.com^$domain=example.*|example.net",
      "domains=example.*|example.net"
    ]);

    compareFilter("||example.com^$domain=example.net|example.*", [
      "type=blocking",
      "text=||example.com^$domain=example.net|example.*",
      "domains=example.*|example.net"
    ]);

    compareFilter("||example.com^$domain=~example.net|example.*", [
      "type=blocking",
      "text=||example.com^$domain=~example.net|example.*",
      "domains=example.*|~example.net"
    ]);

    compareFilter("||example.com^$domain=example.*|~example.net", [
      "type=blocking",
      "text=||example.com^$domain=example.*|~example.net",
      "domains=example.*|~example.net"
    ]);

    // Allowing filters
    compareFilter("@@||example.com^$domain=example.*", [
      "type=allowing",
      "text=@@||example.com^$domain=example.*",
      "domains=example.*"
    ]);

    compareFilter("@@||example.com^$domain=example.*|example.net", [
      "type=allowing",
      "text=@@||example.com^$domain=example.*|example.net",
      "domains=example.*|example.net"
    ]);

    compareFilter("@@||example.com^$domain=example.net|example.*", [
      "type=allowing",
      "text=@@||example.com^$domain=example.net|example.*",
      "domains=example.*|example.net"
    ]);

    compareFilter("@@||example.com^$domain=~example.net|example.*", [
      "type=allowing",
      "text=@@||example.com^$domain=~example.net|example.*",
      "domains=example.*|~example.net"
    ]);

    compareFilter("@@||example.com^$domain=example.*|~example.net", [
      "type=allowing",
      "text=@@||example.com^$domain=example.*|~example.net",
      "domains=example.*|~example.net"
    ]);

    // Element hiding filters
    compareFilter("example.*##abc", [
      "type=elemhide",
      "text=example.*##abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*"
    ]);

    compareFilter("example.*,example.net##abc", [
      "type=elemhide",
      "text=example.*,example.net##abc",
      "selectorDomains=example.*,example.net",
      "selector=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("example.net,example.*##abc", [
      "type=elemhide",
      "text=example.net,example.*##abc",
      "selectorDomains=example.net,example.*",
      "selector=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("~example.net,example.*##abc", [
      "type=elemhide",
      "text=~example.net,example.*##abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*|~example.net"
    ]);

    compareFilter("example.*,~example.net##abc", [
      "type=elemhide",
      "text=example.*,~example.net##abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*|~example.net"
    ]);

    // Element hiding emulation filters
    compareFilter("example.*#?#abc", [
      "type=elemhideemulation",
      "text=example.*#?#abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*"
    ]);

    compareFilter("example.*,example.net#?#abc", [
      "type=elemhideemulation",
      "text=example.*,example.net#?#abc",
      "selectorDomains=example.*,example.net",
      "selector=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("example.net,example.*#?#abc", [
      "type=elemhideemulation",
      "text=example.net,example.*#?#abc",
      "selectorDomains=example.net,example.*",
      "selector=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("~example.net,example.*#?#abc", [
      "type=elemhideemulation",
      "text=~example.net,example.*#?#abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*|~example.net"
    ]);

    compareFilter("example.*,~example.net#?#abc", [
      "type=elemhideemulation",
      "text=example.*,~example.net#?#abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*|~example.net"
    ]);

    // Element hiding exception filters
    compareFilter("example.*#@#abc", [
      "type=elemhideexception",
      "text=example.*#@#abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*"
    ]);

    compareFilter("example.*,example.net#@#abc", [
      "type=elemhideexception",
      "text=example.*,example.net#@#abc",
      "selectorDomains=example.*,example.net",
      "selector=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("example.net,example.*#@#abc", [
      "type=elemhideexception",
      "text=example.net,example.*#@#abc",
      "selectorDomains=example.net,example.*",
      "selector=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("~example.net,example.*#@#abc", [
      "type=elemhideexception",
      "text=~example.net,example.*#@#abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*|~example.net"
    ]);

    compareFilter("example.*,~example.net#@#abc", [
      "type=elemhideexception",
      "text=example.*,~example.net#@#abc",
      "selectorDomains=example.*",
      "selector=abc",
      "domains=example.*|~example.net"
    ]);

    // Snippet filters
    compareFilter("example.*#$#abc", [
      "type=snippet",
      "text=example.*#$#abc",
      "scriptDomains=example.*",
      "script=abc",
      "domains=example.*"
    ]);

    compareFilter("example.*,example.net#$#abc", [
      "type=snippet",
      "text=example.*,example.net#$#abc",
      "scriptDomains=example.*,example.net",
      "script=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("example.net,example.*#$#abc", [
      "type=snippet",
      "text=example.net,example.*#$#abc",
      "scriptDomains=example.net,example.*",
      "script=abc",
      "domains=example.*|example.net"
    ]);

    compareFilter("~example.net,example.*#$#abc", [
      "type=snippet",
      "text=~example.net,example.*#$#abc",
      "scriptDomains=example.*",
      "script=abc",
      "domains=example.*|~example.net"
    ]);

    compareFilter("example.*,~example.net#$#abc", [
      "type=snippet",
      "text=example.*,~example.net#$#abc",
      "scriptDomains=example.*",
      "script=abc",
      "domains=example.*|~example.net"
    ]);
  });
});

describe("isActiveFilter()", function()
{
  let isActiveFilter = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {isActiveFilter, Filter} = sandboxedRequire("../lib/filterClasses")
    );
  });

  // Blocking filters.
  it("should return true for example", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("example")), true);
  });

  it("should return true for ||example.com^", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("||example.com^")), true);
  });

  it("should return true for |https://example.com/foo/", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("|https://example.com/foo/")), true);
  });

  it("should return true for ||example.com/foo/$domain=example.net", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("||example.com/foo/$domain=example.net")), true);
  });

  // Allowing filters.
  it("should return true for @@example", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("@@example")), true);
  });

  it("should return true for @@||example.com^", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("@@||example.com^")), true);
  });

  it("should return true for @@|https://example.com/foo/", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("@@|https://example.com/foo/")), true);
  });

  it("should return true for @@||example.com/foo/$domain=example.net", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("@@||example.com/foo/$domain=example.net")), true);
  });

  // Element hiding filters.
  it("should return true for ##.foo", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("##.foo")), true);
  });

  it("should return true for example.com##.foo", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("example.com##.foo")), true);
  });

  // Element hiding exceptions.
  it("should return true for #@#.foo", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("#@#.foo")), true);
  });

  it("should return true for example.com#@#.foo", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("example.com#@#.foo")), true);
  });

  // Element hiding emulation filters.
  it("should return false for #?#.foo", function()
  {
    // Element hiding emulation filters require a domain.
    assert.strictEqual(isActiveFilter(Filter.fromText("#?#.foo")), false);
  });

  it("should return true for example.com#?#.foo", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("example.com#?#.foo")), true);
  });

  // Snippet filters.
  it("should return false for #$#log 'Hello, world'", function()
  {
    // Snippet filters require a domain.
    assert.strictEqual(isActiveFilter(Filter.fromText("#$#log 'Hello, world'")), false);
  });

  it("should return true for example.com#$#log 'Hello, world'", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("example.com#$#log 'Hello, world'")), true);
  });

  // Comment filters.
  it("should return false for ! example.com filters", function()
  {
    assert.strictEqual(isActiveFilter(Filter.fromText("! example.com filters")), false);
  });

  // Invalid filters.
  it("should return false for ||example.com/foo/$domains=example.net|example.org", function()
  {
    // $domain, not $domains
    assert.strictEqual(isActiveFilter(Filter.fromText("||example.com/foo/$domains=example.net|example.org")), false);
  });

  it("should return false for example.com,,example.net##.foo", function()
  {
    // There must be no blank domain in the list.
    assert.strictEqual(isActiveFilter(Filter.fromText("example.com,,example.net##.foo")), false);
  });
});
