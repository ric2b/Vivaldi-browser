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
let SPECIAL_TYPES = null;
let ALLOWING_TYPES = null;
let enumerateTypes = null;

function assertNumerical(n)
{
  assert.equal(typeof n, "number", `${n} is not of type number`);
}

function assertPositivePowerOfTwo(n)
{
  // Use Object.is() here to fail any comparison with -0
  // (e.g. Math.log2(0.5) % 1 is -0 while 0.5 is 2 ** -1).
  assert.ok(Object.is(Math.log2(n) % 1, 0), `${n} is not a positive power of 2`);
}

function assertLessThanEqualTwoPowThirty(n)
{
  assert.ok(n <= 2 ** 30, `${n} is not less than or equal to 2 ** 30`);
}

function assertPositive(n)
{
  assert.ok(n >= 0, `${n} is not positive`);
}

function assertLessThanTwoPowThirtyOne(n)
{
  assert.ok(n < 2 ** 31, `${n} is not less than 2 ** 31`);
}

function assertNoBitsShared(n, o)
{
  assert.equal(n & o, 0, `${n} shares bits with ${o}`);
}

function assertAllBitsShared(n, o)
{
  assert.equal(n & o, n, `${n} does not share some bits with ${o}`);
}

beforeEach(function()
{
  let sandboxedRequire = createSandbox();
  (
    {contentTypes, RESOURCE_TYPES, SPECIAL_TYPES, ALLOWING_TYPES,
     enumerateTypes} = sandboxedRequire("../lib/contentTypes")
  );
});

// Types of web resources.
describe("contentTypes.OTHER", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.OTHER);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.OTHER);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.OTHER);
  });
});

describe("contentTypes.XBL", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.XBL);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.XBL);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.XBL);
  });
});

describe("contentTypes.DTD", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.DTD);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.DTD);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.DTD);
  });
});

describe("contentTypes.SCRIPT", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.SCRIPT);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.SCRIPT);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.SCRIPT);
  });
});

describe("contentTypes.IMAGE", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.IMAGE);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.IMAGE);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.IMAGE);
  });
});

describe("contentTypes.BACKGROUND", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.BACKGROUND);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.BACKGROUND);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.BACKGROUND);
  });
});

describe("contentTypes.STYLESHEET", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.STYLESHEET);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.STYLESHEET);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.STYLESHEET);
  });
});

describe("contentTypes.OBJECT", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.OBJECT);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.OBJECT);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.OBJECT);
  });
});

describe("contentTypes.SUBDOCUMENT", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.SUBDOCUMENT);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.SUBDOCUMENT);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.SUBDOCUMENT);
  });
});

describe("contentTypes.WEBSOCKET", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.WEBSOCKET);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.WEBSOCKET);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.WEBSOCKET);
  });
});

describe("contentTypes.WEBRTC", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.WEBRTC);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.WEBRTC);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.WEBRTC);
  });
});

describe("contentTypes.PING", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.PING);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.PING);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.PING);
  });
});

describe("contentTypes.XMLHTTPREQUEST", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.XMLHTTPREQUEST);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.XMLHTTPREQUEST);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.XMLHTTPREQUEST);
  });
});

describe("contentTypes.MEDIA", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.MEDIA);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.MEDIA);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.MEDIA);
  });
});

describe("contentTypes.FONT", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.FONT);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.FONT);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.FONT);
  });
});

// Special filter options.
describe("contentTypes.POPUP", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.POPUP);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.POPUP);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.POPUP);
  });
});

describe("contentTypes.CSP", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.CSP);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.CSP);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.CSP);
  });
});

// Allowlisting flags.
describe("contentTypes.DOCUMENT", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.DOCUMENT);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.DOCUMENT);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.DOCUMENT);
  });
});

describe("contentTypes.GENERICBLOCK", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.GENERICBLOCK);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.GENERICBLOCK);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.GENERICBLOCK);
  });
});

describe("contentTypes.ELEMHIDE", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.ELEMHIDE);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.ELEMHIDE);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.ELEMHIDE);
  });
});

describe("contentTypes.GENERICHIDE", function()
{
  it("should be numerical", function()
  {
    assertNumerical(contentTypes.GENERICHIDE);
  });

  it("should be a positive non-fractional power of 2", function()
  {
    assertPositivePowerOfTwo(contentTypes.GENERICHIDE);
  });

  it("should be less than or equal to 2 ** 30", function()
  {
    assertLessThanEqualTwoPowThirty(contentTypes.GENERICHIDE);
  });
});

describe("RESOURCE_TYPES", function()
{
  it("should be numerical", function()
  {
    assertNumerical(RESOURCE_TYPES);
  });

  it("should be positive", function()
  {
    assertPositive(RESOURCE_TYPES);
  });

  it("should be less than 2 ** 31", function()
  {
    assertLessThanTwoPowThirtyOne(RESOURCE_TYPES);
  });

  it("should share no bits with SPECIAL_TYPES", function()
  {
    assertNoBitsShared(RESOURCE_TYPES, SPECIAL_TYPES);
  });
});

describe("SPECIAL_TYPES", function()
{
  it("should be numerical", function()
  {
    assertNumerical(SPECIAL_TYPES);
  });

  it("should be positive", function()
  {
    assertPositive(SPECIAL_TYPES);
  });

  it("should be less than 2 ** 31", function()
  {
    assertLessThanTwoPowThirtyOne(SPECIAL_TYPES);
  });

  it("should share no bits with RESOURCE_TYPES", function()
  {
    assertNoBitsShared(SPECIAL_TYPES, RESOURCE_TYPES);
  });
});

describe("ALLOWING_TYPES", function()
{
  it("should be numerical", function()
  {
    assertNumerical(ALLOWING_TYPES);
  });

  it("should be positive", function()
  {
    assertPositive(ALLOWING_TYPES);
  });

  it("should be less than 2 ** 31", function()
  {
    assertLessThanTwoPowThirtyOne(ALLOWING_TYPES);
  });

  it("should share all bits with SPECIAL_TYPES", function()
  {
    assertAllBitsShared(ALLOWING_TYPES, SPECIAL_TYPES);
  });
});

describe("*enumerateTypes()", function()
{
  function testEnumerateTypes(types, expectedTypes, {selection} = {})
  {
    let contentType = 0;
    for (let type of types.split(","))
      contentType |= contentTypes[type];

    let expectedContentType = 0;
    for (let type of expectedTypes.split(","))
      expectedContentType |= contentTypes[type];

    let actualContentType = 0;
    for (let value of enumerateTypes(contentType, selection))
    {
      assertNumerical(value);
      assertPositivePowerOfTwo(value);
      assertLessThanEqualTwoPowThirty(value);

      assert.equal(actualContentType & value, 0,
                   `${value} already yielded once`);

      actualContentType |= value;
    }

    assert.equal(actualContentType, expectedContentType,
                 `${actualContentType} is not equal to ${expectedContentType}`);
  }

  // Resource types only.
  it("should yield SCRIPT,IMAGE,STYLESHEET for SCRIPT,IMAGE,STYLESHEET", function()
  {
    testEnumerateTypes("SCRIPT,IMAGE,STYLESHEET", "SCRIPT,IMAGE,STYLESHEET");
  });

  it("should yield SCRIPT,IMAGE,STYLESHEET for SCRIPT,IMAGE,STYLESHEET and selection RESOURCE_TYPES", function()
  {
    testEnumerateTypes("SCRIPT,IMAGE,STYLESHEET", "SCRIPT,IMAGE,STYLESHEET",
                       {selection: RESOURCE_TYPES});
  });

  it("should yield nothing for SCRIPT,IMAGE,STYLESHEET and selection SPECIAL_TYPES", function()
  {
    testEnumerateTypes("SCRIPT,IMAGE,STYLESHEET", "",
                       {selection: SPECIAL_TYPES});
  });

  // Special types only.
  it("should yield CSP,DOCUMENT,GENERICHIDE for CSP,DOCUMENT,GENERICHIDE", function()
  {
    testEnumerateTypes("CSP,DOCUMENT,GENERICHIDE",
                       "CSP,DOCUMENT,GENERICHIDE");
  });

  it("should yield nothing for CSP,DOCUMENT,GENERICHIDE and selection RESOURCE_TYPES", function()
  {
    testEnumerateTypes("CSP,DOCUMENT,GENERICHIDE", "",
                       {selection: RESOURCE_TYPES});
  });

  it("should yield CSP,DOCUMENT,GENERICHIDE for CSP,DOCUMENT,GENERICHIDE and selection SPECIAL_TYPES", function()
  {
    testEnumerateTypes("CSP,DOCUMENT,GENERICHIDE",
                       "CSP,DOCUMENT,GENERICHIDE",
                       {selection: SPECIAL_TYPES});
  });

  // Mixed.
  it("should yield WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE for WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE", function()
  {
    testEnumerateTypes("WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE",
                       "WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE");
  });

  it("should yield WEBSOCKET,XMLHTTPREQUEST,FONT for WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE and selection RESOURCE_TYPES", function()
  {
    testEnumerateTypes("WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE",
                       "WEBSOCKET,XMLHTTPREQUEST,FONT",
                       {selection: RESOURCE_TYPES});
  });

  it("should yield POPUP,GENERICBLOCK,ELEMHIDE for WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE and selection SPECIAL_TYPES", function()
  {
    testEnumerateTypes("WEBSOCKET,XMLHTTPREQUEST,FONT,POPUP,GENERICBLOCK,ELEMHIDE",
                       "POPUP,GENERICBLOCK,ELEMHIDE",
                       {selection: SPECIAL_TYPES});
  });

  // None.
  it("should yield nothing for nothing", function()
  {
    testEnumerateTypes("", "");
  });

  it("should yield nothing for nothing and selection RESOURCE_TYPES", function()
  {
    testEnumerateTypes("", "", {selection: RESOURCE_TYPES});
  });

  it("should yield nothing for nothing and selection SPECIAL_TYPES", function()
  {
    testEnumerateTypes("", "", {selection: SPECIAL_TYPES});
  });
});
