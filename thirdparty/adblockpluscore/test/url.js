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

const publicSuffixes = require("../data/publicSuffixList.json");

describe("parseURL()", function()
{
  let parseURL = null;

  function testURLParsing(url)
  {
    // Note: The function expects a normalized URL.
    // e.g. "http:example.com:80?foo" should already be normalized to
    // "http://example.com/?foo". If not, the tests will fail.
    let urlInfo = parseURL(url);

    // We need to ensure only that our implementation matches that of the URL
    // object.
    let urlObject = new URL(url);

    assert.equal(urlInfo.href, urlObject.href);
    assert.equal(urlInfo.protocol, urlObject.protocol);
    assert.equal(urlInfo.hostname, urlObject.hostname);

    assert.equal(urlInfo.toString(), urlObject.toString());
    assert.equal(String(urlInfo), String(urlObject));
    assert.equal(urlInfo + "", urlObject + "");
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {parseURL} = sandboxedRequire("../lib/url")
    );
  });

  it("should parse https://example.com/", function()
  {
    testURLParsing("https://example.com/");
  });

  it("should parse https://example.com/foo", function()
  {
    testURLParsing("https://example.com/foo");
  });

  it("should parse https://example.com/foo/bar", function()
  {
    testURLParsing("https://example.com/foo/bar");
  });

  it("should parse https://example.com/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://example.com/foo/bar?https://random/foo/bar");
  });

  it("should parse https://example.com:8080/", function()
  {
    testURLParsing("https://example.com:8080/");
  });

  it("should parse https://example.com:8080/foo", function()
  {
    testURLParsing("https://example.com:8080/foo");
  });

  it("should parse https://example.com:8080/foo/bar", function()
  {
    testURLParsing("https://example.com:8080/foo/bar");
  });

  it("should parse https://example.com:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://example.com:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://localhost/", function()
  {
    testURLParsing("http://localhost/");
  });

  it("should parse http://localhost/foo", function()
  {
    testURLParsing("http://localhost/foo");
  });

  it("should parse http://localhost/foo/bar", function()
  {
    testURLParsing("http://localhost/foo/bar");
  });

  it("should parse http://localhost/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://localhost/foo/bar?https://random/foo/bar");
  });

  it("should parse https://user@example.com/", function()
  {
    testURLParsing("https://user@example.com/");
  });

  it("should parse https://user@example.com/foo", function()
  {
    testURLParsing("https://user@example.com/foo");
  });

  it("should parse https://user@example.com/foo/bar", function()
  {
    testURLParsing("https://user@example.com/foo/bar");
  });

  it("should parse https://user@example.com/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://user@example.com/foo/bar?https://random/foo/bar");
  });

  it("should parse https://user@example.com:8080/", function()
  {
    testURLParsing("https://user@example.com:8080/");
  });

  it("should parse https://user@example.com:8080/foo", function()
  {
    testURLParsing("https://user@example.com:8080/foo");
  });

  it("should parse https://user@example.com:8080/foo/bar", function()
  {
    testURLParsing("https://user@example.com:8080/foo/bar");
  });

  it("should parse https://user@example.com:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://user@example.com:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse https://user:pass@example.com/", function()
  {
    testURLParsing("https://user:pass@example.com/");
  });

  it("should parse https://user:pass@example.com/foo", function()
  {
    testURLParsing("https://user:pass@example.com/foo");
  });

  it("should parse https://user:pass@example.com/foo/bar", function()
  {
    testURLParsing("https://user:pass@example.com/foo/bar");
  });

  it("should parse https://user:pass@example.com/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://user:pass@example.com/foo/bar?https://random/foo/bar");
  });

  it("should parse https://user:pass@example.com:8080/", function()
  {
    testURLParsing("https://user:pass@example.com:8080/");
  });

  it("should parse https://user:pass@example.com:8080/foo", function()
  {
    testURLParsing("https://user:pass@example.com:8080/foo");
  });

  it("should parse https://user:pass@example.com:8080/foo/bar", function()
  {
    testURLParsing("https://user:pass@example.com:8080/foo/bar");
  });

  it("should parse https://user:pass@example.com:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://user:pass@example.com:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse https://us%40er:pa%40ss@example.com/", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com/");
  });

  it("should parse https://us%40er:pa%40ss@example.com/foo", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com/foo");
  });

  it("should parse https://us%40er:pa%40ss@example.com/foo/bar", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com/foo/bar");
  });

  it("should parse https://us%40er:pa%40ss@example.com/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com/foo/bar?https://random/foo/bar");
  });

  it("should parse https://us%40er:pa%40ss@example.com:8080/", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com:8080/");
  });

  it("should parse https://us%40er:pa%40ss@example.com:8080/foo", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com:8080/foo");
  });

  it("should parse https://us%40er:pa%40ss@example.com:8080/foo/bar", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com:8080/foo/bar");
  });

  it("should parse https://us%40er:pa%40ss@example.com:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("https://us%40er:pa%40ss@example.com:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://192.168.1.1/", function()
  {
    testURLParsing("http://192.168.1.1/");
  });

  it("should parse http://192.168.1.1/foo", function()
  {
    testURLParsing("http://192.168.1.1/foo");
  });

  it("should parse http://192.168.1.1/foo/bar", function()
  {
    testURLParsing("http://192.168.1.1/foo/bar");
  });

  it("should parse http://192.168.1.1/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://192.168.1.1/foo/bar?https://random/foo/bar");
  });

  it("should parse http://192.168.1.1:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://192.168.1.1:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://user@192.168.1.1:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://user@192.168.1.1:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://user:pass@192.168.1.1:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://user:pass@192.168.1.1:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://[2001:db8:0:42:0:8a2e:370:7334]/", function()
  {
    testURLParsing("http://[2001:db8:0:42:0:8a2e:370:7334]/");
  });

  it("should parse http://[2001:db8:0:42:0:8a2e:370:7334]/foo", function()
  {
    testURLParsing("http://[2001:db8:0:42:0:8a2e:370:7334]/foo");
  });

  it("should parse http://[2001:db8:0:42:0:8a2e:370:7334]/foo/bar", function()
  {
    testURLParsing("http://[2001:db8:0:42:0:8a2e:370:7334]/foo/bar");
  });

  it("should parse http://[2001:db8:0:42:0:8a2e:370:7334]/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://[2001:db8:0:42:0:8a2e:370:7334]/foo/bar?https://random/foo/bar");
  });

  it("should parse http://[2001:db8:0:42:0:8a2e:370:7334]:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://[2001:db8:0:42:0:8a2e:370:7334]:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://user@[2001:db8:0:42:0:8a2e:370:7334]:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://user@[2001:db8:0:42:0:8a2e:370:7334]:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse http://user:pass@[2001:db8:0:42:0:8a2e:370:7334]:8080/foo/bar?https://random/foo/bar", function()
  {
    testURLParsing("http://user:pass@[2001:db8:0:42:0:8a2e:370:7334]:8080/foo/bar?https://random/foo/bar");
  });

  it("should parse ftp://user:pass@example.com:8021/", function()
  {
    testURLParsing("ftp://user:pass@example.com:8021/");
  });

  it("should parse ftp://user:pass@example.com:8021/foo", function()
  {
    testURLParsing("ftp://user:pass@example.com:8021/foo");
  });

  it("should parse ftp://user:pass@example.com:8021/foo/bar", function()
  {
    testURLParsing("ftp://user:pass@example.com:8021/foo/bar");
  });

  it("should parse about:blank", function()
  {
    testURLParsing("about:blank");
  });

  it("should parse chrome://extensions", function()
  {
    testURLParsing("chrome://extensions");
  });

  it("should parse chrome-extension://bhignfpcigccnlfapldlodmhlidjaion/options.html", function()
  {
    testURLParsing("chrome-extension://bhignfpcigccnlfapldlodmhlidjaion/options.html");
  });

  it("should parse mailto:john.doe@mail.example.com", function()
  {
    testURLParsing("mailto:john.doe@mail.example.com");
  });

  it("should parse news:newsgroup", function()
  {
    testURLParsing("news:newsgroup");
  });

  it("should parse news:message-id", function()
  {
    testURLParsing("news:message-id");
  });

  it("should parse nntp://example.com:8119/newsgroup", function()
  {
    testURLParsing("nntp://example.com:8119/newsgroup");
  });

  it("should parse nntp://example.com:8119/message-id", function()
  {
    testURLParsing("nntp://example.com:8119/message-id");
  });

  it("should parse data:,", function()
  {
    testURLParsing("data:,");
  });

  it("should parse data:text/vnd-example+xyz;foo=bar;base64,R0lGODdh", function()
  {
    testURLParsing("data:text/vnd-example+xyz;foo=bar;base64,R0lGODdh");
  });

  it("should parse data:text/plain;charset=UTF-8;page=21,the%20data:1234,5678", function()
  {
    testURLParsing("data:text/plain;charset=UTF-8;page=21,the%20data:1234,5678");
  });

  it("should parse javascript:", function()
  {
    testURLParsing("javascript:");
  });

  it("should parse javascript:alert();", function()
  {
    testURLParsing("javascript:alert();");
  });

  it("should parse javascript:foo/bar/", function()
  {
    testURLParsing("javascript:foo/bar/");
  });

  it("should parse javascript://foo/bar/", function()
  {
    testURLParsing("javascript://foo/bar/");
  });

  it("should parse file:///dev/random", function()
  {
    testURLParsing("file:///dev/random");
  });

  it("should parse wss://example.com/", function()
  {
    testURLParsing("wss://example.com/");
  });

  it("should parse wss://example.com:8080/", function()
  {
    testURLParsing("wss://example.com:8080/");
  });

  it("should parse wss://user@example.com:8080/", function()
  {
    testURLParsing("wss://user@example.com:8080/");
  });

  it("should parse wss://user:pass@example.com:8080/", function()
  {
    testURLParsing("wss://user:pass@example.com:8080/");
  });

  it("should parse stuns:stuns.example.com/", function()
  {
    testURLParsing("stuns:stuns.example.com/");
  });

  it("should parse stuns:stuns.example.com:8080/", function()
  {
    testURLParsing("stuns:stuns.example.com:8080/");
  });

  it("should parse stuns:user@stuns.example.com:8080/", function()
  {
    testURLParsing("stuns:user@stuns.example.com:8080/");
  });

  it("should parse stuns:user:pass@stuns.example.com:8080/", function()
  {
    testURLParsing("stuns:user:pass@stuns.example.com:8080/");
  });

  // The following tests are based on
  // https://cs.chromium.org/chromium/src/url/gurl_unittest.cc?rcl=9ec7bc85e0f6a0bf28eff6b2eca678067da547e9
  // Note: We do not check for "canonicalization" (normalization). parseURL()
  // should be used with normalized URLs only.

  it("should parse something:///example.com/", function()
  {
    testURLParsing("something:///example.com/");
  });

  it("should parse something://example.com/", function()
  {
    testURLParsing("something://example.com/");
  });

  it("should parse file:///C:/foo.txt", function()
  {
    testURLParsing("file:///C:/foo.txt");
  });

  it("should parse file://server/foo.txt", function()
  {
    testURLParsing("file://server/foo.txt");
  });

  it("should parse http://user:pass@example.com:99/foo;bar?q=a#ref", function()
  {
    testURLParsing("http://user:pass@example.com:99/foo;bar?q=a#ref");
  });

  it("should parse http://user:%40!$&'()*+,%3B%3D%3A@example.com:12345/", function()
  {
    testURLParsing("http://user:%40!$&'()*+,%3B%3D%3A@example.com:12345/");
  });

  it("should parse filesystem:http://example.com/temporary/", function()
  {
    testURLParsing("filesystem:http://example.com/temporary/");
  });

  it("should parse filesystem:http://user:%40!$&'()*+,%3B%3D%3A@example.com:12345/", function()
  {
    testURLParsing("filesystem:http://user:%40!$&'()*+,%3B%3D%3A@example.com:12345/");
  });

  it("should parse javascript:window.alert('hello, world');", function()
  {
    testURLParsing("javascript:window.alert('hello, world');");
  });

  it("should parse javascript:#", function()
  {
    testURLParsing("javascript:#");
  });

  it("should parse blob:https://example.com/7ce70a1e-9681-4148-87a8-43cb9171b994", function()
  {
    testURLParsing("blob:https://example.com/7ce70a1e-9681-4148-87a8-43cb9171b994");
  });

  it("should parse http://[2001:db8::1]/", function()
  {
    testURLParsing("http://[2001:db8::1]/");
  });

  it("should parse http://[2001:db8::1]:8080/", function()
  {
    testURLParsing("http://[2001:db8::1]:8080/");
  });

  it("should parse http://[::]:8080/", function()
  {
    testURLParsing("http://[::]:8080/");
  });

  it("should parse not-a-standard-scheme:this is arbitrary content", function()
  {
    testURLParsing("not-a-standard-scheme:this is arbitrary content");
  });

  it("should parse view-source:http://example.com/path", function()
  {
    testURLParsing("view-source:http://example.com/path");
  });

  it("should parse data:text/html,Question?%3Cdiv%20style=%22color:%20#bad%22%3Eidea%3C/div%3E", function()
  {
    testURLParsing("data:text/html,Question?%3Cdiv%20style=%22color:%20#bad%22%3Eidea%3C/div%3E");
  });
});

describe("isLocalhost()", function()
{
  let isLocalhost = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {isLocalhost} = sandboxedRequire("../lib/url")
    );
  });

  it("should return true for localhost", function()
  {
    assert.strictEqual(isLocalhost("localhost"), true);
  });

  it("should return true for 127.0.0.1", function()
  {
    assert.strictEqual(isLocalhost("127.0.0.1"), true);
  });

  it("should return true for [::1]", function()
  {
    assert.strictEqual(isLocalhost("[::1]"), true);
  });

  it("should return false for example.com", function()
  {
    assert.strictEqual(isLocalhost("example.com"), false);
  });
});

describe("*domainSuffixes()", function()
{
  let domainSuffixes = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {domainSuffixes} = sandboxedRequire("../lib/url")
    );
  });

  it("should yield localhost for localhost", function()
  {
    assert.deepEqual([...domainSuffixes("localhost")], ["localhost"]);
  });

  it("should yield example.com, com for example.com", function()
  {
    assert.deepEqual([...domainSuffixes("example.com")], ["example.com", "com"]);
  });

  it("should yield www.example.com, example.com, com for www.example.com", function()
  {
    assert.deepEqual([...domainSuffixes("www.example.com")],
                     ["www.example.com", "example.com", "com"]);
  });

  it("should yield www.example.co.in, example.co.in, co.in, in for www.example.co.in", function()
  {
    assert.deepEqual([...domainSuffixes("www.example.co.in")],
                     ["www.example.co.in", "example.co.in", "co.in", "in"]);
  });

  it("should yield 192.168.1.1 for 192.168.1.1", function()
  {
    assert.deepEqual([...domainSuffixes("192.168.1.1")], ["192.168.1.1"]);
  });

  it("should yield [2001:db8:0:42:0:8a2e:370:7334] for [2001:db8:0:42:0:8a2e:370:7334]", function()
  {
    assert.deepEqual([...domainSuffixes("[2001:db8:0:42:0:8a2e:370:7334]")],
                     ["[2001:db8:0:42:0:8a2e:370:7334]"]);
  });

  // With blank.
  it("should yield localhost and a blank string for localhost, true", function()
  {
    assert.deepEqual([...domainSuffixes("localhost", true)], ["localhost", ""]);
  });

  it("should yield example.com, com, and a blank string for example.com, true", function()
  {
    assert.deepEqual([...domainSuffixes("example.com", true)],
                     ["example.com", "com", ""]);
  });

  it("should yield www.example.com, example.com, com, and a blank string for www.example.com, true", function()
  {
    assert.deepEqual([...domainSuffixes("www.example.com", true)],
                     ["www.example.com", "example.com", "com", ""]);
  });

  it("should yield www.example.co.in, example.co.in, co.in, in, and a blank string for www.example.co.in, true", function()
  {
    assert.deepEqual([...domainSuffixes("www.example.co.in", true)],
                     ["www.example.co.in", "example.co.in", "co.in", "in", ""]);
  });

  it("should yield 192.168.1.1 and a blank string for 192.168.1.1, true", function()
  {
    assert.deepEqual([...domainSuffixes("192.168.1.1", true)], ["192.168.1.1", ""]);
  });

  it("should yield [2001:db8:0:42:0:8a2e:370:7334] and a blank string for [2001:db8:0:42:0:8a2e:370:7334], true", function()
  {
    assert.deepEqual([...domainSuffixes("[2001:db8:0:42:0:8a2e:370:7334]", true)],
                     ["[2001:db8:0:42:0:8a2e:370:7334]", ""]);
  });

  // With a trailing dot.
  it("should yield nothing for .", function()
  {
    assert.deepEqual([...domainSuffixes(".")], []);
  });

  it("should yield localhost for localhost.", function()
  {
    assert.deepEqual([...domainSuffixes("localhost.")],
                     ["localhost"]);
  });

  it("should yield example.com, com for example.com.", function()
  {
    assert.deepEqual([...domainSuffixes("example.com.")],
                     ["example.com", "com"]);
  });

  // Quirks and edge cases.
  it("should yield nothing for blank domain", function()
  {
    assert.deepEqual([...domainSuffixes("")], []);
  });

  it("should yield .localhost, localhost for .localhost", function()
  {
    assert.deepEqual([...domainSuffixes(".localhost")],
                     [".localhost", "localhost"]);
  });

  it("should yield .example.com, example.com, com for .example.com", function()
  {
    assert.deepEqual([...domainSuffixes(".example.com")],
                     [".example.com", "example.com", "com"]);
  });

  it("should yield ..localhost, .localhost, localhost for ..localhost", function()
  {
    assert.deepEqual([...domainSuffixes("..localhost")],
                     ["..localhost", ".localhost", "localhost"]);
  });

  it("should yield ..example..com, .example..com, example..com, .com, com for ..example..com", function()
  {
    assert.deepEqual([...domainSuffixes("..example..com")],
                     ["..example..com", ".example..com", "example..com", ".com", "com"]);
  });

  it("should yield localhost. for localhost..", function()
  {
    assert.deepEqual([...domainSuffixes("localhost..")], ["localhost."]);
  });

  it("should yield example..com., .com., com. for example..com..", function()
  {
    assert.deepEqual([...domainSuffixes("example..com..")],
                     ["example..com.", ".com.", "com."]);
  });
});

describe("URLRequest", function()
{
  let URLRequest = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {URLRequest} = sandboxedRequire("../lib/url")
    );
  });

  describe("#get thirdParty()", function()
  {
    function testThirdParty(url, documentHostname, expected, message)
    {
      let {thirdParty} = URLRequest.from(url, documentHostname);
      assert.equal(thirdParty, expected, message);
    }

    it("should return false for http://foo/ and foo", function()
    {
      testThirdParty("http://foo/", "foo", false, "same domain isn't third-party");
    });

    it("should return true for http://foo/ and bar", function()
    {
      testThirdParty("http://foo/", "bar", true, "different domain is third-party");
    });

    it("should return false for http://foo.com/ and foo.com", function()
    {
      testThirdParty("http://foo.com/", "foo.com", false,
                     "same domain with TLD (.com) isn't third-party");
    });

    it("should return true for http://foo.com/ and bar.com", function()
    {
      testThirdParty("http://foo.com/", "bar.com", true,
                     "same TLD (.com) but different domain is third-party");
    });

    it("should return false for http://foo.com/ and www.foo.com", function()
    {
      testThirdParty("http://foo.com/", "www.foo.com", false,
                     "same domain but differend subdomain isn't third-party");
    });

    it("should return false for http://foo.example.com/ and bar.example.com", function()
    {
      testThirdParty("http://foo.example.com/", "bar.example.com", false,
                     "same basedomain (example.com) isn't third-party");
    });

    it("should return true for http://foo.uk/ and bar.uk", function()
    {
      testThirdParty("http://foo.uk/", "bar.uk", true,
                     "same TLD (.uk) but different domain is third-party");
    });

    it("should return true for http://foo.co.uk/ and bar.co.uk", function()
    {
      testThirdParty("http://foo.co.uk/", "bar.co.uk", true,
                     "same TLD (.co.uk) but different domain is third-party");
    });

    it("should return false for http://foo.example.co.uk/ and bar.example.co.uk", function()
    {
      testThirdParty("http://foo.example.co.uk/", "bar.example.co.uk", false,
                     "same basedomain (example.co.uk) isn't third-party");
    });

    it("should return false for http://1.2.3.4/ and 1.2.3.4", function()
    {
      testThirdParty("http://1.2.3.4/", "1.2.3.4", false,
                     "same IPv4 address isn't third-party");
    });

    it("should return true for http://1.1.1.1/ and 2.1.1.1", function()
    {
      testThirdParty("http://1.1.1.1/", "2.1.1.1", true,
                     "different IPv4 address is third-party");
    });

    it("should return false for http://[2001:db8:85a3::8a2e:370:7334]/ and [2001:db8:85a3::8a2e:370:7334]", function()
    {
      testThirdParty("http://[2001:db8:85a3::8a2e:370:7334]/", "[2001:db8:85a3::8a2e:370:7334]", false,
                     "same IPv6 address isn't third-party");
    });

    it("should return true for http://[2001:db8:85a3::8a2e:370:7334]/ and [5001:db8:85a3::8a2e:370:7334]", function()
    {
      testThirdParty("http://[2001:db8:85a3::8a2e:370:7334]/", "[5001:db8:85a3::8a2e:370:7334]", true,
                     "different IPv6 address is third-party");
    });

    it("should return false for http://example.com/ and example.com.", function()
    {
      testThirdParty("http://example.com/", "example.com.", false,
                     "traling dots are ignored");
    });
  });
});

describe("getBaseDomain()", function()
{
  let getBaseDomain = null;

  before(function()
  {
    assert.equal(typeof publicSuffixes["localhost"], "undefined");
    assert.equal(typeof publicSuffixes["localhost.localdomain"], "undefined");

    assert.equal(typeof publicSuffixes["example.com"], "undefined");
    assert.equal(typeof publicSuffixes["africa.com"], "number");
    assert.equal(typeof publicSuffixes["compute.amazonaws.com"], "number");

    // If these sanity checks fail, look for other examles of cascading offsets
    // from the public suffix list.
    assert.equal(typeof publicSuffixes["images.example.s3.dualstack.us-east-1.amazonaws.com"],
                 "undefined");
    assert.equal(typeof publicSuffixes["example.s3.dualstack.us-east-1.amazonaws.com"],
                 "undefined");
    assert.equal(publicSuffixes["s3.dualstack.us-east-1.amazonaws.com"], 1);
    assert.equal(typeof publicSuffixes["dualstack.us-east-1.amazonaws.com"],
                 "undefined");
    assert.equal(typeof publicSuffixes["example.us-east-1.amazonaws.com"],
                 "undefined");
    assert.equal(publicSuffixes["us-east-1.amazonaws.com"], 1);
    assert.equal(typeof publicSuffixes["example.amazonaws.com"], "undefined");
    assert.equal(typeof publicSuffixes["amazonaws.com"], "undefined");
  });

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {getBaseDomain} = sandboxedRequire("../lib/url")
    );
  });

  it("should return correct values for all known suffixes", function()
  {
    this.timeout(10000);

    let parts = ["aaa", "bbb", "ccc", "ddd", "eee"];
    let levels = 3;

    for (let suffix in publicSuffixes)
    {
      let offset = publicSuffixes[suffix];

      // If this fails, add more parts.
      assert.ok(offset <= parts.length - levels,
                "Not enough domain parts for testing");

      for (let i = 0; i < offset + levels; i++)
      {
        let hostname = parts.slice(0, i).join(".");
        hostname += (hostname ? "." : "") + suffix;

        let expected = parts.slice(Math.max(0, i - offset), i).join(".");
        expected += (expected ? "." : "") + suffix;

        assert.equal(getBaseDomain(hostname), expected,
                     `getBaseDomain("${hostname}") == "${expected}"` +
                     ` with {suffix: "${suffix}", offset: ${offset}}`);
      }
    }
  });

  // Unknown suffixes.
  it("should return localhost for localhost", function()
  {
    assert.equal(getBaseDomain("localhost"), "localhost");
  });

  it("should return localhost.localdomain for localhost.localdomain", function()
  {
    assert.equal(getBaseDomain("localhost.localdomain"), "localhost.localdomain");
  });

  it("should return localhost.localdomain for mail.localhost.localdomain", function()
  {
    assert.equal(getBaseDomain("mail.localhost.localdomain"),
                 "localhost.localdomain");
  });

  it("should return localhost.localdomain for www.example.localhost.localdomain", function()
  {
    assert.equal(getBaseDomain("www.example.localhost.localdomain"),
                 "localhost.localdomain");
  });

  // Unknown suffixes that overlap partly with known suffixes.
  it("should return example.com for example.com", function()
  {
    assert.equal(getBaseDomain("example.com"), "example.com");
  });

  it("should return example.com for mail.example.com", function()
  {
    assert.equal(getBaseDomain("mail.example.com"), "example.com");
  });

  it("should return example.com for secure.mail.example.com", function()
  {
    assert.equal(getBaseDomain("secure.mail.example.com"), "example.com");
  });

  // Cascading offsets.
  it("should return example.s3.dualstack.us-east-1.amazonaws.com for images.example.s3.dualstack.us-east-1.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("images.example.s3.dualstack.us-east-1.amazonaws.com"),
                 "example.s3.dualstack.us-east-1.amazonaws.com");
  });

  it("should return example.s3.dualstack.us-east-1.amazonaws.com for example.s3.dualstack.us-east-1.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("example.s3.dualstack.us-east-1.amazonaws.com"),
                 "example.s3.dualstack.us-east-1.amazonaws.com");
  });

  it("should return s3.dualstack.us-east-1.amazonaws.com for s3.dualstack.us-east-1.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("s3.dualstack.us-east-1.amazonaws.com"),
                 "s3.dualstack.us-east-1.amazonaws.com");
  });

  it("should return dualstack.us-east-1.amazonaws.com for dualstack.us-east-1.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("dualstack.us-east-1.amazonaws.com"),
                 "dualstack.us-east-1.amazonaws.com");
  });

  it("should return example.us-east-1.amazonaws.com for example.us-east-1.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("example.us-east-1.amazonaws.com"),
                 "example.us-east-1.amazonaws.com");
  });

  it("should return us-east-1.amazonaws.com for us-east-1.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("us-east-1.amazonaws.com"),
                 "us-east-1.amazonaws.com");
  });

  it("should return amazonaws.com for example.amazonaws.com", function()
  {
    assert.equal(getBaseDomain("example.amazonaws.com"), "amazonaws.com");
  });

  it("should return amazonaws.com for amazonaws.com", function()
  {
    assert.equal(getBaseDomain("amazonaws.com"), "amazonaws.com");
  });

  // Edge case.
  it("should return blank domain for blank hostname", function()
  {
    assert.equal(getBaseDomain(""), "");
  });
});

describe("parseDomains()", function()
{
  let parseDomains = null;

  function testFilterDomainsParsing(source, separator, expected,
                                    {repeatingDomains = true} = {})
  {
    let domains = parseDomains(source, separator);

    // Test internal caching.
    assert.equal(parseDomains(source, separator), domains);
    assert.notEqual(parseDomains(`extra.example.com${separator}${source}`,
                                 separator),
                    domains);

    let domainsObj = null;

    if (domains)
    {
      domainsObj = {};
      for (let [domain, include] of domains)
        domainsObj[domain] = +include;
    }

    assert.deepEqual(domainsObj, expected, source);

    // Test repeating domains.
    if (repeatingDomains && source != "")
    {
      testFilterDomainsParsing(`${source}${separator}${source}`, separator,
                               expected, {repeatingDomains: false});
    }
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {parseDomains} = sandboxedRequire("../lib/url")
    );
  });

  it("should return Map {'' => true} for blank string", function()
  {
    testFilterDomainsParsing("", "|", {"": 1});
  });

  it("should return Map {'' => false, 'example.com' => true} for example.com", function()
  {
    testFilterDomainsParsing("example.com", "|", {"": 0, "example.com": 1});
  });

  it("should return Map {'' => false, 'example.com' => true, 'foo.example.com' => false} for example.com|~foo.example.com", function()
  {
    testFilterDomainsParsing("example.com|~foo.example.com", "|", {
      "": 0,
      "example.com": 1,
      "foo.example.com": 0
    });
  });

  it("should return Map {'' => true, 'foo.example.com' => false} for ~foo.example.com", function()
  {
    testFilterDomainsParsing("~foo.example.com", "|", {
      "": 1,
      "foo.example.com": 0
    });
  });

  it("should return Map {'' => false, 'example.com' => true, 'foo.example.com' => true, 'bar.example.com' => true} for example.com|foo.example.com|bar.example.com", function()
  {
    testFilterDomainsParsing("example.com|foo.example.com|bar.example.com", "|", {
      "": 0,
      "example.com": 1,
      "foo.example.com": 1,
      "bar.example.com": 1
    });
  });

  it("should return Map {'' => false, 'example.com' => true, 'foo.example.com' => true, 'bar.example.com' => false} for example.com|foo.example.com|~bar.example.com", function()
  {
    testFilterDomainsParsing("example.com|foo.example.com|~bar.example.com", "|", {
      "": 0,
      "example.com": 1,
      "foo.example.com": 1,
      "bar.example.com": 0
    });
  });

  it("should return Map {'' => false, 'example.com' => true, 'foo.example.com' => false, 'bar.example.com' => false} for example.com|~foo.example.com|~bar.example.com", function()
  {
    testFilterDomainsParsing("example.com|~foo.example.com|~bar.example.com", "|", {
      "": 0,
      "example.com": 1,
      "foo.example.com": 0,
      "bar.example.com": 0
    });
  });

  it("should return Map {'' => true, 'example.com' => false, 'foo.example.com' => false, 'bar.example.com' => false} for ~example.com|~foo.example.com|~bar.example.com", function()
  {
    testFilterDomainsParsing("~example.com|~foo.example.com|~bar.example.com", "|", {
      "": 1,
      "example.com": 0,
      "foo.example.com": 0,
      "bar.example.com": 0
    });
  });
});

describe("isValidHostname()", function()
{
  let isValidHostname = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {isValidHostname} = sandboxedRequire("../lib/url")
    );
  });

  it("should return false for blank hostname", function()
  {
    assert.strictEqual(isValidHostname(""), false);
  });

  it("should return true for example.com", function()
  {
    assert.strictEqual(isValidHostname("example.com"), true);
  });

  it("should return true for EXAMPLE.COM (upper case)", function()
  {
    assert.strictEqual(isValidHostname("EXAMPLE.COM"), true);
  });

  it("should return true for eXaMple.COm (mixed case)", function()
  {
    assert.strictEqual(isValidHostname("eXaMple.COm"), true);
  });

  it("should return false for example. com (space)", function()
  {
    assert.strictEqual(isValidHostname("example. com"), false);
  });

  it("should return false for ' example.com' (leading space)", function()
  {
    assert.strictEqual(isValidHostname(" example.com"), false);
  });

  it("should return false for 'example.com ' (trailing space)", function()
  {
    assert.strictEqual(isValidHostname("example.com "), false);
  });

  it("should return true for example.com. (trailing dot)", function()
  {
    assert.strictEqual(isValidHostname("example.com."), true);
  });

  it("should return false for example.com.. (multiple trailing dots)", function()
  {
    assert.strictEqual(isValidHostname("example.com.."), false);
  });

  it("should return false for .example.com (leading dot)", function()
  {
    assert.strictEqual(isValidHostname(".example.com"), false);
  });

  it("should return false for example..com (consecutive dots)", function()
  {
    assert.strictEqual(isValidHostname("example..com"), false);
  });

  it("should return true for foo.example.com", function()
  {
    assert.strictEqual(isValidHostname("foo.example.com"), true);
  });

  it("should return true for bar.foo.example.com", function()
  {
    assert.strictEqual(isValidHostname("bar.foo.example.com"), true);
  });

  it("should return true for foo-bar.example.com", function()
  {
    assert.strictEqual(isValidHostname("foo-bar.example.com"), true);
  });

  it("should return true for -example.com (leading hyphen)", function()
  {
    assert.strictEqual(isValidHostname("-example.com"), false);
  });

  it("should return true for example.com- (trailing hyphen)", function()
  {
    assert.strictEqual(isValidHostname("example.com-"), false);
  });

  it("should return true for example.-com (inner leading hyphen)", function()
  {
    assert.strictEqual(isValidHostname("example.-com"), false);
  });

  it("should return true for example-.com (inner trailing hyphen)", function()
  {
    assert.strictEqual(isValidHostname("example-.com"), false);
  });

  it("should return true for localhost", function()
  {
    assert.strictEqual(isValidHostname("localhost"), true);
  });

  it("should return true for 0.0.0.0 (IPv4 address)", function()
  {
    assert.strictEqual(isValidHostname("0.0.0.0"), true);
  });

  it("should return true for 255.255.255.255 (IPv4 address)", function()
  {
    assert.strictEqual(isValidHostname("255.255.255.255"), true);
  });

  it("should return true for 192.168.1.1 (IPv4 address)", function()
  {
    assert.strictEqual(isValidHostname("192.168.1.1"), true);
  });

  it("should return false for 192.168.257 (IPv4 address, non-normalized)", function()
  {
    // Normalized: 192.168.1.1
    assert.strictEqual(isValidHostname("192.168.257"), false);
  });

  it("should return false for 192.168.000.001 (IPv4 address, non-normalized)", function()
  {
    // Normalized: 192.168.0.1
    assert.strictEqual(isValidHostname("192.168.000.001"), false);
  });

  it("should return false for 192.168.1 (IPv4 address, non-normalized)", function()
  {
    // Normalized: 192.168.0.1
    assert.strictEqual(isValidHostname("192.168.1"), false);
  });

  it("should return false for 127.1 (IPv4 address, non-normalized)", function()
  {
    // Normalized: 127.0.0.1
    assert.strictEqual(isValidHostname("127.1"), false);
  });

  it("should return false for 0 (IPv4 address, non-normalized)", function()
  {
    // Normalized: 0.0.0.0
    assert.strictEqual(isValidHostname("0"), false);
  });

  it("should return false for 255.255.255.256 (IPv4 address, invalid)", function()
  {
    assert.strictEqual(isValidHostname("255.255.255.256"), false);
  });

  it("should return false for 255.255.255.265 (IPv4 address, invalid)", function()
  {
    assert.strictEqual(isValidHostname("255.255.255.265"), false);
  });

  it("should return false for 255.255.255.355 (IPv4 address, invalid)", function()
  {
    assert.strictEqual(isValidHostname("255.255.255.355"), false);
  });

  it("should return true for [2001:db8:0:42:0:8a2e:370:7334] (IPv6 address)", function()
  {
    assert.strictEqual(isValidHostname("[2001:db8:0:42:0:8a2e:370:7334]"), true);
  });

  it("should return false for 1.2.3.4.5 (all numeric)", function()
  {
    assert.strictEqual(isValidHostname("1.2.3.4.5"), false);
  });

  it("should return false for example.1", function()
  {
    assert.strictEqual(isValidHostname("example.1"), false);
  });

  it("should return true for 1.com", function()
  {
    assert.strictEqual(isValidHostname("1.com"), true);
  });

  it("should return true for 2.1.com", function()
  {
    assert.strictEqual(isValidHostname("2.1.com"), true);
  });

  it("should return true for 3.2.1.com", function()
  {
    assert.strictEqual(isValidHostname("3.2.1.com"), true);
  });

  it("should return true for www1.example.com", function()
  {
    assert.strictEqual(isValidHostname("www1.example.com"), true);
  });

  it("should return true for 10x.example.com", function()
  {
    assert.strictEqual(isValidHostname("10x.example.com"), true);
  });

  it("should return true for xn--938h.com (IDNA)", function()
  {
    assert.strictEqual(isValidHostname("xn--938h.com"), true);
  });

  it("should return false for \u{1f642}.com (invalid characters)", function()
  {
    assert.strictEqual(isValidHostname("\u{1f642}.com"), false);
  });

  it("should return false for \u262e.com", function()
  {
    assert.strictEqual(isValidHostname("\u262e.com"), false);
  });

  it("should return false for example.*", function()
  {
    assert.strictEqual(isValidHostname("example.*"), false);
  });

  it("should return false for *.example.com", function()
  {
    assert.strictEqual(isValidHostname("*.example.com"), false);
  });

  it("should return false for example*.com", function()
  {
    assert.strictEqual(isValidHostname("example*.com"), false);
  });

  it("should return false for *example.com", function()
  {
    assert.strictEqual(isValidHostname("*example.com"), false);
  });

  it("should return false for www.example*", function()
  {
    assert.strictEqual(isValidHostname("www.example*"), false);
  });

  it("should return false for www.*example", function()
  {
    assert.strictEqual(isValidHostname("www.*example"), false);
  });

  it("should return false for example.com:443", function()
  {
    assert.strictEqual(isValidHostname("example.com:443"), false);
  });

  it("should return false for example.com:8080", function()
  {
    assert.strictEqual(isValidHostname("example.com:8080"), false);
  });

  it("should return false for hostname with 64-character label", function()
  {
    assert.strictEqual(isValidHostname("abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz01.com"), false);
  });

  it("should return true for hostname with 63-character label", function()
  {
    assert.strictEqual(isValidHostname("abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.com"), true);
  });

  it("should return false for 254-character hostname", function()
  {
    assert.strictEqual(isValidHostname("abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz"), false);
  });

  it("should return true for 253-character hostname", function()
  {
    assert.strictEqual(isValidHostname("abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxy"), true);
  });

  it("should return true for 254-character hostname with trailing dot", function()
  {
    assert.strictEqual(isValidHostname("abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0.abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxy."), true);
  });
});
