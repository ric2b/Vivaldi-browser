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

let ElemHideEmulationFilter = null;
let elemHideEmulation = null;
let elemHideExceptions = null;
let Filter = null;

describe("Element hiding emulation", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Filter,
       ElemHideEmulationFilter} = sandboxedRequire("../lib/filterClasses"),
      {elemHideEmulation} = sandboxedRequire("../lib/elemHideEmulation"),
      {elemHideExceptions} = sandboxedRequire("../lib/elemHideExceptions")
    );
  });

  it("Domain restrictions", function()
  {
    function testSelectorMatches(description, filters, domain, expectedMatches)
    {
      for (let filter of filters)
      {
        filter = Filter.fromText(filter);
        if (filter instanceof ElemHideEmulationFilter)
          elemHideEmulation.add(filter);
        else
          elemHideExceptions.add(filter);
      }

      let matches = elemHideEmulation.getFilters(domain)
          .map(filter => filter.text);
      assert.deepEqual(matches.sort(), expectedMatches.sort(), description);

      elemHideEmulation.clear();
      elemHideExceptions.clear();
    }

    testSelectorMatches(
      "Ignore generic filters",
      [
        "#?#:-abp-properties(foo)", "example.com#?#:-abp-properties(foo)",
        "~example.com#?#:-abp-properties(foo)"
      ],
      "example.com",
      ["example.com#?#:-abp-properties(foo)"]
    );
    testSelectorMatches(
      "Ignore selectors with exceptions",
      [
        "example.com#?#:-abp-properties(foo)",
        "example.com#?#:-abp-properties(bar)",
        "example.com#@#:-abp-properties(foo)"
      ],
      "example.com",
      ["example.com#?#:-abp-properties(bar)"]
    );
    testSelectorMatches(
      "Ignore filters that include parent domain but exclude subdomain",
      [
        "~www.example.com,example.com#?#:-abp-properties(foo)"
      ],
      "www.example.com",
      []
    );
    testSelectorMatches(
      "Ignore filters with parent domain if exception matches subdomain",
      [
        "www.example.com#@#:-abp-properties(foo)",
        "example.com#?#:-abp-properties(foo)"
      ],
      "www.example.com",
      []
    );
    testSelectorMatches(
      "Ignore filters for other subdomain",
      [
        "www.example.com#?#:-abp-properties(foo)",
        "other.example.com#?#:-abp-properties(foo)"
      ],
      "other.example.com",
      ["other.example.com#?#:-abp-properties(foo)"]
    );
  });

  it("Filters container", function()
  {
    function compareFilters(description, domain, expectedMatches)
    {
      let result = elemHideEmulation.getFilters(domain)
          .map(filter => filter.text);
      expectedMatches = expectedMatches.map(filter => filter.text);
      assert.deepEqual(result.sort(), expectedMatches.sort(), description);
    }

    let domainFilter = Filter.fromText("example.com##filter1");
    let subdomainFilter = Filter.fromText("www.example.com##filter2");
    let otherDomainFilter = Filter.fromText("other.example.com##filter3");

    elemHideEmulation.add(domainFilter);
    elemHideEmulation.add(subdomainFilter);
    elemHideEmulation.add(otherDomainFilter);
    compareFilters(
      "Return all matching filters",
      "www.example.com",
      [domainFilter, subdomainFilter]
    );

    elemHideEmulation.remove(domainFilter);
    compareFilters(
      "Return all matching filters after removing one",
      "www.example.com",
      [subdomainFilter]
    );

    elemHideEmulation.clear();
    compareFilters(
      "Return no filters after clearing",
      "www.example.com",
      []
    );
  });
});
