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
let URLRequest = null;

describe("URL filters matching", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {contentTypes} = sandboxedRequire("../lib/contentTypes"),
      {Filter} = sandboxedRequire("../lib/filterClasses"),
      {URLRequest} = sandboxedRequire("../lib/url")
    );
  });

  function testMatch(text, location, contentType, docDomain, thirdParty, sitekey, expected)
  {
    if (thirdParty && docDomain == null)
      docDomain = "some-other-domain";

    function testMatchInternal(filterText)
    {
      let filter = Filter.fromText(filterText);
      let request = URLRequest.from(location, docDomain);
      let result = filter.matches(request, contentTypes[contentType], sitekey);
      assert.equal(!!result, expected, '"' + filterText + '".matches(' + location + ", " + contentType + ", " + docDomain + ", " + (thirdParty ? "third-party" : "first-party") + ", " + (sitekey || "no-sitekey") + ")");
    }

    testMatchInternal(text);
    if (!/^@@/.test(text))
      testMatchInternal("@@" + text);
  }

  it("Basic filters", function()
  {
    testMatch("abc", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc", "http://ABC/adf", "IMAGE", null, false, null, true);
    testMatch("abc", "http://abd/adf", "IMAGE", null, false, null, false);
    testMatch("|abc", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("|http://abc", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc|", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc/adf|", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("||example.com/foo", "http://example.com/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||com/foo", "http://example.com/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||mple.com/foo", "http://example.com/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||/example.com/foo", "http://example.com/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||example.com/foo/bar|", "http://example.com/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||example.com/foo", "http://foo.com/http://example.com/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||example.com/foo|", "http://example.com/foo/bar", "IMAGE", null, false, null, false);
  });

  it("Separator placeholders", function()
  {
    testMatch("abc^d", "http://abc/def", "IMAGE", null, false, null, true);
    testMatch("abc^e", "http://abc/def", "IMAGE", null, false, null, false);
    testMatch("def^", "http://abc/def", "IMAGE", null, false, null, true);
    testMatch("http://abc/d^f", "http://abc/def", "IMAGE", null, false, null, false);
    testMatch("http://abc/def^", "http://abc/def", "IMAGE", null, false, null, true);
    testMatch("^foo=bar^", "http://abc/?foo=bar", "IMAGE", null, false, null, true);
    testMatch("^foo=bar^", "http://abc/?a=b&foo=bar", "IMAGE", null, false, null, true);
    testMatch("^foo=bar^", "http://abc/?foo=bar&a=b", "IMAGE", null, false, null, true);
    testMatch("^foo=bar^", "http://abc/?notfoo=bar", "IMAGE", null, false, null, false);
    testMatch("^foo=bar^", "http://abc/?foo=barnot", "IMAGE", null, false, null, false);
    testMatch("^foo=bar^", "http://abc/?foo=bar%2Enot", "IMAGE", null, false, null, false);
    testMatch("||example.com^", "http://example.com/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||example.com^", "http://example.company.com/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||example.com^", "http://example.com:1234/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||example.com^", "http://example.com.com/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||example.com^", "http://example.com-company.com/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||example.com^foo", "http://example.com/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||пример.ру^", "http://пример.ру/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||пример.ру^", "http://пример.руководитель.ру/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||пример.ру^", "http://пример.ру:1234/foo/bar", "IMAGE", null, false, null, true);
    testMatch("||пример.ру^", "http://пример.ру.ру/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||пример.ру^", "http://пример.ру-ководитель.ру/foo/bar", "IMAGE", null, false, null, false);
    testMatch("||пример.ру^foo", "http://пример.ру/foo/bar", "IMAGE", null, false, null, true);
  });

  it("Wildcard matching", function()
  {
    testMatch("abc*d", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc*d", "http://abcd/af", "IMAGE", null, false, null, true);
    testMatch("abc*d", "http://abc/d/af", "IMAGE", null, false, null, true);
    testMatch("abc*d", "http://dabc/af", "IMAGE", null, false, null, false);
    testMatch("*abc", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc*", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("|*abc", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc*|", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc***d", "http://abc/adf", "IMAGE", null, false, null, true);
  });

  it("Type options", function()
  {
    testMatch("abc$image", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$other", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$other", "http://abc/adf", "OTHER", null, false, null, true);
    testMatch("abc$~other", "http://abc/adf", "OTHER", null, false, null, false);
    testMatch("abc$script", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$script", "http://abc/adf", "SCRIPT", null, false, null, true);
    testMatch("abc$~script", "http://abc/adf", "SCRIPT", null, false, null, false);
    testMatch("abc$stylesheet", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$stylesheet", "http://abc/adf", "STYLESHEET", null, false, null, true);
    testMatch("abc$~stylesheet", "http://abc/adf", "STYLESHEET", null, false, null, false);
    testMatch("abc$object", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$object", "http://abc/adf", "OBJECT", null, false, null, true);
    testMatch("abc$~object", "http://abc/adf", "OBJECT", null, false, null, false);
    testMatch("abc$document", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$document", "http://abc/adf", "DOCUMENT", null, false, null, true);
    testMatch("abc$~document", "http://abc/adf", "DOCUMENT", null, false, null, false);
    testMatch("abc$subdocument", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$subdocument", "http://abc/adf", "SUBDOCUMENT", null, false, null, true);
    testMatch("abc$~subdocument", "http://abc/adf", "SUBDOCUMENT", null, false, null, false);
    testMatch("abc$websocket", "http://abc/adf", "OBJECT", null, false, null, false);
    testMatch("abc$websocket", "http://abc/adf", "WEBSOCKET", null, false, null, true);
    testMatch("abc$~websocket", "http://abc/adf", "WEBSOCKET", null, false, null, false);
    testMatch("abc$webrtc", "http://abc/adf", "OBJECT", null, false, null, false);
    testMatch("abc$webrtc", "http://abc/adf", "WEBRTC", null, false, null, true);
    testMatch("abc$~webrtc", "http://abc/adf", "WEBRTC", null, false, null, false);
    testMatch("abc$background", "http://abc/adf", "OBJECT", null, false, null, false);
    testMatch("abc$background", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$~background", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$xbl", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$xbl", "http://abc/adf", "XBL", null, false, null, true);
    testMatch("abc$~xbl", "http://abc/adf", "XBL", null, false, null, false);
    testMatch("abc$ping", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$ping", "http://abc/adf", "PING", null, false, null, true);
    testMatch("abc$~ping", "http://abc/adf", "PING", null, false, null, false);
    testMatch("abc$xmlhttprequest", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$xmlhttprequest", "http://abc/adf", "XMLHTTPREQUEST", null, false, null, true);
    testMatch("abc$~xmlhttprequest", "http://abc/adf", "XMLHTTPREQUEST", null, false, null, false);
    testMatch("abc$dtd", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$dtd", "http://abc/adf", "DTD", null, false, null, true);
    testMatch("abc$~dtd", "http://abc/adf", "DTD", null, false, null, false);

    testMatch("abc$media", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$media", "http://abc/adf", "MEDIA", null, false, null, true);
    testMatch("abc$~media", "http://abc/adf", "MEDIA", null, false, null, false);

    testMatch("abc$font", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$font", "http://abc/adf", "FONT", null, false, null, true);
    testMatch("abc$~font", "http://abc/adf", "FONT", null, false, null, false);

    testMatch("abc$ping", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$ping", "http://abc/adf", "PING", null, false, null, true);
    testMatch("abc$~ping", "http://abc/adf", "PING", null, false, null, false);

    testMatch("abc$image,script", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$~image", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$~script", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$~image,~script", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$~script,~image", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$~document,~script,~other", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$~image,image", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$image,~image", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$~image,image", "http://abc/adf", "SCRIPT", null, false, null, true);
    testMatch("abc$image,~image", "http://abc/adf", "SCRIPT", null, false, null, false);
    testMatch("abc$match-case", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$match-case", "http://ABC/adf", "IMAGE", null, false, null, false);
    testMatch("abc$~match-case", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$~match-case", "http://ABC/adf", "IMAGE", null, false, null, true);
    testMatch("abc$match-case,image", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$match-case,script", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$match-case,image", "http://ABC/adf", "IMAGE", null, false, null, false);
    testMatch("abc$match-case,script", "http://ABC/adf", "IMAGE", null, false, null, false);
    testMatch("abc$third-party", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$third-party", "http://abc/adf", "IMAGE", null, true, null, true);
    testMatch("abd$third-party", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abd$third-party", "http://abc/adf", "IMAGE", null, true, null, false);
    testMatch("abc$image,third-party", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$image,third-party", "http://abc/adf", "IMAGE", null, true, null, true);
    testMatch("abc$~image,third-party", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abc$~image,third-party", "http://abc/adf", "IMAGE", null, true, null, false);
    testMatch("abc$~third-party", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$~third-party", "http://abc/adf", "IMAGE", null, true, null, false);
    testMatch("abd$~third-party", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("abd$~third-party", "http://abc/adf", "IMAGE", null, true, null, false);
    testMatch("abc$image,~third-party", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("abc$image,~third-party", "http://abc/adf", "IMAGE", null, true, null, false);
    testMatch("abc$~image,~third-party", "http://abc/adf", "IMAGE", null, false, null, false);
  });

  it("Regular expressions", function()
  {
    testMatch("/abc/", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("/abc/", "http://abcd/adf", "IMAGE", null, false, null, true);
    testMatch("*/abc/", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("*/abc/", "http://abcd/adf", "IMAGE", null, false, null, false);
    testMatch("/a\\wc/", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("/a\\wc/", "http://a1c/adf", "IMAGE", null, false, null, true);
    testMatch("/a\\wc/", "http://a_c/adf", "IMAGE", null, false, null, true);
    testMatch("/a\\wc/", "http://a%c/adf", "IMAGE", null, false, null, false);
  });

  it("Regular expressions with type options", function()
  {
    testMatch("/abc/$image", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("/abc/$image", "http://aBc/adf", "IMAGE", null, false, null, true);
    testMatch("/abc/$script", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("/abc/$~image", "http://abcd/adf", "IMAGE", null, false, null, false);
    testMatch("/ab{2}c/$image", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("/ab{2}c/$script", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("/ab{2}c/$~image", "http://abcd/adf", "IMAGE", null, false, null, false);
    testMatch("/abc/$third-party", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("/abc/$third-party", "http://abc/adf", "IMAGE", null, true, null, true);
    testMatch("/abc/$~third-party", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("/abc/$~third-party", "http://abc/adf", "IMAGE", null, true, null, false);
    testMatch("/abc/$match-case", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("/abc/$match-case", "http://aBc/adf", "IMAGE", null, true, null, false);
    testMatch("/ab{2}c/$match-case", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("/ab{2}c/$match-case", "http://aBc/adf", "IMAGE", null, true, null, false);
    testMatch("/abc/$~match-case", "http://abc/adf", "IMAGE", null, false, null, true);
    testMatch("/abc/$~match-case", "http://aBc/adf", "IMAGE", null, true, null, true);
    testMatch("/ab{2}c/$~match-case", "http://abc/adf", "IMAGE", null, false, null, false);
    testMatch("/ab{2}c/$~match-case", "http://aBc/adf", "IMAGE", null, true, null, false);
  });

  it("Domain restrictions", function()
  {
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", "foo.com.", true, null, true);
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, true);
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", "www.foo.com.", true, null, true);
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", "abc.def.foo.com", true, null, true);
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, false);
    testMatch("abc$domain=foo.com", "http://abc/def", "IMAGE", null, true, null, false);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", "foo.com.", true, null, true);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, true);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", "www.foo.com.", true, null, true);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", "abc.def.foo.com", true, null, true);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, false);
    testMatch("abc$domain=foo.com|bar.com", "http://abc/def", "IMAGE", null, true, null, false);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", "foo.com.", true, null, true);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, true);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", "www.foo.com.", true, null, true);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", "abc.def.foo.com", true, null, true);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, false);
    testMatch("abc$domain=bar.com|foo.com", "http://abc/def", "IMAGE", null, true, null, false);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", "foo.com.", true, null, false);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, false);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", "www.foo.com.", true, null, false);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", "abc.def.foo.com", true, null, false);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, true);
    testMatch("abc$domain=~foo.com", "http://abc/def", "IMAGE", null, true, null, true);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", "foo.com.", true, null, false);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, false);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", "www.foo.com.", true, null, false);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", "abc.def.foo.com", true, null, false);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, true);
    testMatch("abc$domain=~foo.com|~bar.com", "http://abc/def", "IMAGE", null, true, null, true);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", "foo.com.", true, null, false);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, false);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", "www.foo.com.", true, null, false);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", "abc.def.foo.com", true, null, false);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, true);
    testMatch("abc$domain=~bar.com|~foo.com", "http://abc/def", "IMAGE", null, true, null, true);
    testMatch("abc$domain=foo.com|~bar.com", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$domain=foo.com|~bar.com", "http://abc/def", "IMAGE", "bar.com", true, null, false);
    testMatch("abc$domain=foo.com|~bar.com", "http://abc/def", "IMAGE", "baz.com", true, null, false);
    testMatch("abc$domain=foo.com|~bar.foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$domain=foo.com|~bar.foo.com", "http://abc/def", "IMAGE", "www.foo.com", true, null, true);
    testMatch("abc$domain=foo.com|~bar.foo.com", "http://abc/def", "IMAGE", "bar.foo.com", true, null, false);
    testMatch("abc$domain=foo.com|~bar.foo.com", "http://abc/def", "IMAGE", "www.bar.foo.com", true, null, false);
    testMatch("abc$domain=foo.com|~bar.foo.com", "http://abc/def", "IMAGE", "baz.com", true, null, false);
    testMatch("abc$domain=foo.com|~bar.foo.com", "http://abc/def", "IMAGE", "www.baz.com", true, null, false);
    testMatch("abc$domain=com|~foo.com", "http://abc/def", "IMAGE", "bar.com", true, null, true);
    testMatch("abc$domain=com|~foo.com", "http://abc/def", "IMAGE", "bar.net", true, null, false);
    testMatch("abc$domain=com|~foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$domain=com|~foo.com", "http://abc/def", "IMAGE", "foo.net", true, null, false);
    testMatch("abc$domain=com|~foo.com", "http://abc/def", "IMAGE", "com", true, null, true);
    testMatch("abc$domain=foo.com", "http://ccc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$domain=foo.com", "http://ccc/def", "IMAGE", "bar.com", true, null, false);
    testMatch("abc$image,domain=foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$image,domain=foo.com", "http://abc/def", "IMAGE", "bar.com", true, null, false);
    testMatch("abc$image,domain=foo.com", "http://abc/def", "OBJECT", "foo.com", true, null, false);
    testMatch("abc$image,domain=foo.com", "http://abc/def", "OBJECT", "bar.com", true, null, false);
    testMatch("abc$~image,domain=foo.com", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$~image,domain=foo.com", "http://abc/def", "IMAGE", "bar.com", true, null, false);
    testMatch("abc$~image,domain=foo.com", "http://abc/def", "OBJECT", "foo.com", true, null, true);
    testMatch("abc$~image,domain=foo.com", "http://abc/def", "OBJECT", "bar.com", true, null, false);
    testMatch("abc$domain=foo.com,image", "http://abc/def", "IMAGE", "foo.com", true, null, true);
    testMatch("abc$domain=foo.com,image", "http://abc/def", "IMAGE", "bar.com", true, null, false);
    testMatch("abc$domain=foo.com,image", "http://abc/def", "OBJECT", "foo.com", true, null, false);
    testMatch("abc$domain=foo.com,image", "http://abc/def", "OBJECT", "bar.com", true, null, false);
    testMatch("abc$domain=foo.com,~image", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$domain=foo.com,~image", "http://abc/def", "IMAGE", "bar.com", true, null, false);
    testMatch("abc$domain=foo.com,~image", "http://abc/def", "OBJECT", "foo.com", true, null, true);
    testMatch("abc$domain=foo.com,~image", "http://abc/def", "OBJECT", "bar.com", true, null, false);
  });

  it("Sitekey restrictions", function()
  {
    testMatch("abc$sitekey=foo-publickey", "http://abc/def", "IMAGE", "foo.com", true, "foo-publickey", true);
    testMatch("abc$sitekey=foo-publickey", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$sitekey=foo-publickey", "http://abc/def", "IMAGE", "foo.com", true, "bar-publickey", false);
    testMatch("abc$sitekey=foo-publickey|bar-publickey", "http://abc/def", "IMAGE", "foo.com", true, "foo-publickey", true);
    testMatch("abc$sitekey=foo-publickey|bar-publickey", "http://abc/def", "IMAGE", "foo.com", true, null, false);
    testMatch("abc$sitekey=bar-publickey|foo-publickey", "http://abc/def", "IMAGE", "foo.com", true, "foo-publickey", true);
    testMatch("abc$sitekey=foo-publickey", "http://ccc/def", "IMAGE", "foo.com", true, "foo-publickey", false);
    testMatch("abc$domain=foo.com,sitekey=foo-publickey", "http://abc/def", "IMAGE", "foo.com", true, "foo-publickey", true);
    testMatch("abc$domain=foo.com,sitekey=foo-publickey", "http://abc/def", "IMAGE", "bar.com", true, "foo-publickey", false);
    testMatch("abc$domain=~foo.com,sitekey=foo-publickey", "http://abc/def", "IMAGE", "foo.com", true, "foo-publickey", false);
    testMatch("abc$domain=~foo.com,sitekey=foo-publickey", "http://abc/def", "IMAGE", "bar.com", true, "foo-publickey", true);
  });

  it("Exception rules", function()
  {
    testMatch("@@test", "http://test/", "DOCUMENT", null, false, null, false);
    testMatch("@@http://test*", "http://test/", "DOCUMENT", null, false, null, false);
    testMatch("@@ftp://test*", "ftp://test/", "DOCUMENT", null, false, null, false);
    testMatch("@@test$document", "http://test/", "DOCUMENT", null, false, null, true);
    testMatch("@@test$document,image", "http://test/", "DOCUMENT", null, false, null, true);
    testMatch("@@test$~image", "http://test/", "DOCUMENT", null, false, null, false);
    testMatch("@@test$~image,document", "http://test/", "DOCUMENT", null, false, null, true);
    testMatch("@@test$document,~image", "http://test/", "DOCUMENT", null, false, null, true);
    testMatch("@@test$document,domain=foo.com", "http://test/", "DOCUMENT", "foo.com", false, null, true);
    testMatch("@@test$document,domain=foo.com", "http://test/", "DOCUMENT", "bar.com", false, null, false);
    testMatch("@@test$document,domain=~foo.com", "http://test/", "DOCUMENT", "foo.com", false, null, false);
    testMatch("@@test$document,domain=~foo.com", "http://test/", "DOCUMENT", "bar.com", false, null, true);
    testMatch("@@test$document,sitekey=foo-publickey", "http://test/", "DOCUMENT", "foo.com", false, "foo-publickey", true);
    testMatch("@@test$document,sitekey=foo-publickey", "http://test/", "DOCUMENT", "foo.com", false, null, false);
  });
});
