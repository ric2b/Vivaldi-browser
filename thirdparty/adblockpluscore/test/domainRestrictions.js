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

describe("Domain restrictions", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Filter} = sandboxedRequire("../lib/filterClasses")
    );
  });

  function testActive(text, domain, expectedActive, expectedOnlyDomain)
  {
    let filter = Filter.fromText(text);
    assert.equal(filter.isActiveOnDomain(domain), expectedActive,
                 text + " active on " + domain);
    assert.equal(filter.isActiveOnlyOnDomain(domain), expectedOnlyDomain,
                 text + " only active on " + domain);
  }

  describe("Unrestricted", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo", null, true, false);
      testActive("foo", "com", true, false);
      testActive("foo", "example.com", true, false);
      testActive("foo", "example.com.", true, false);
      testActive("foo", "foo.example.com", true, false);
      testActive("foo", "mple.com", true, false);
    });

    it("Hiding rules", function()
    {
      testActive("##foo", null, true, false);
      testActive("##foo", "com", true, false);
      testActive("##foo", "example.com", true, false);
      testActive("##foo", "example.com.", true, false);
      testActive("##foo", "foo.example.com", true, false);
      testActive("##foo", "mple.com", true, false);
    });
  });

  describe("Domain restricted", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo$domain=example.com", null, false, false);
      testActive("foo$domain=example.com", "com", false, true);
      testActive("foo$domain=example.com", "example.com", true, true);
      testActive("foo$domain=example.com", "example.com.", true, true);
      testActive("foo$domain=example.com.", "example.com", false, false);
      testActive("foo$domain=example.com.", "example.com.", false, false);
      testActive("foo$domain=example.com", "foo.example.com", true, false);
      testActive("foo$domain=example.com", "mple.com", false, false);
    });

    it("Hiding rules", function()
    {
      testActive("example.com##foo", null, false, false);
      testActive("example.com##foo", "com", false, true);
      testActive("example.com##foo", "example.com", true, true);
      testActive("example.com##foo", "example.com.", true, true);
      testActive("example.com.##foo", "example.com", false, false);
      testActive("example.com.##foo", "example.com.", false, false);
      testActive("example.com##foo", "foo.example.com", true, false);
      testActive("example.com##foo", "mple.com", false, false);
    });
  });

  describe("Restricted to domain and its subdomain", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo$domain=example.com|foo.example.com", null, false, false);
      testActive("foo$domain=example.com|foo.example.com", "com", false, true);
      testActive("foo$domain=example.com|foo.example.com", "example.com", true, true);
      testActive("foo$domain=example.com|foo.example.com", "example.com.", true, true);
      testActive("foo$domain=example.com|foo.example.com", "foo.example.com", true, false);
      testActive("foo$domain=example.com|foo.example.com", "mple.com", false, false);
    });

    it("Hiding rules", function()
    {
      testActive("example.com,foo.example.com##foo", null, false, false);
      testActive("example.com,foo.example.com##foo", "com", false, true);
      testActive("example.com,foo.example.com##foo", "example.com", true, true);
      testActive("example.com,foo.example.com##foo", "example.com.", true, true);
      testActive("example.com,foo.example.com##foo", "foo.example.com", true, false);
      testActive("example.com,foo.example.com##foo", "mple.com", false, false);
    });
  });

  describe("With exception for a subdomain", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo$domain=~foo.example.com", null, true, false);
      testActive("foo$domain=~foo.example.com", "com", true, false);
      testActive("foo$domain=~foo.example.com", "example.com", true, false);
      testActive("foo$domain=~foo.example.com", "example.com.", true, false);
      testActive("foo$domain=~foo.example.com", "foo.example.com", false, false);
      testActive("foo$domain=~foo.example.com", "mple.com", true, false);
    });

    it("Hiding rules", function()
    {
      testActive("~foo.example.com##foo", null, true, false);
      testActive("~foo.example.com##foo", "com", true, false);
      testActive("~foo.example.com##foo", "example.com", true, false);
      testActive("~foo.example.com##foo", "example.com.", true, false);
      testActive("~foo.example.com##foo", "foo.example.com", false, false);
      testActive("~foo.example.com##foo", "mple.com", true, false);
    });
  });

  describe("For domain but not its subdomain", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo$domain=example.com|~foo.example.com", null, false, false);
      testActive("foo$domain=example.com|~foo.example.com", "com", false, true);
      testActive("foo$domain=example.com|~foo.example.com", "example.com", true, true);
      testActive("foo$domain=example.com|~foo.example.com", "example.com.", true, true);
      testActive("foo$domain=example.com|~foo.example.com", "foo.example.com", false, false);
      testActive("foo$domain=example.com|~foo.example.com", "mple.com", false, false);
    });

    it("Hiding rules", function()
    {
      testActive("example.com,~foo.example.com##foo", null, false, false);
      testActive("example.com,~foo.example.com##foo", "com", false, true);
      testActive("example.com,~foo.example.com##foo", "example.com", true, true);
      testActive("example.com,~foo.example.com##foo", "example.com.", true, true);
      testActive("example.com,~foo.example.com##foo", "foo.example.com", false, false);
      testActive("example.com,~foo.example.com##foo", "mple.com", false, false);
    });
  });

  describe("For domain but not its TLD", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo$domain=example.com|~com", null, false, false);
      testActive("foo$domain=example.com|~com", "com", false, true);
      testActive("foo$domain=example.com|~com", "example.com", true, true);
      testActive("foo$domain=example.com|~com", "example.com.", true, true);
      testActive("foo$domain=example.com|~com", "foo.example.com", true, false);
      testActive("foo$domain=example.com|~com", "mple.com", false, false);
    });

    it("Hiding rules", function()
    {
      testActive("example.com,~com##foo", null, false, false);
      testActive("example.com,~com##foo", "com", false, true);
      testActive("example.com,~com##foo", "example.com", true, true);
      testActive("example.com,~com##foo", "example.com.", true, true);
      testActive("example.com,~com##foo", "foo.example.com", true, false);
      testActive("example.com,~com##foo", "mple.com", false, false);
    });
  });

  describe("Restricted to an unrelated domain", function()
  {
    it("Blocking filters", function()
    {
      testActive("foo$domain=nnnnnnn.nnn", null, false, false);
      testActive("foo$domain=nnnnnnn.nnn", "com", false, false);
      testActive("foo$domain=nnnnnnn.nnn", "example.com", false, false);
      testActive("foo$domain=nnnnnnn.nnn", "example.com.", false, false);
      testActive("foo$domain=nnnnnnn.nnn", "foo.example.com", false, false);
      testActive("foo$domain=nnnnnnn.nnn", "mple.com", false, false);
    });

    it("Hiding rules", function()
    {
      testActive("nnnnnnn.nnn##foo", null, false, false);
      testActive("nnnnnnn.nnn##foo", "com", false, false);
      testActive("nnnnnnn.nnn##foo", "example.com", false, false);
      testActive("nnnnnnn.nnn##foo", "example.com.", false, false);
      testActive("nnnnnnn.nnn##foo", "foo.example.com", false, false);
      testActive("nnnnnnn.nnn##foo", "mple.com", false, false);
    });
  });
});
