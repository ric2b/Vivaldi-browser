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

describe("compareVersions()", function()
{
  let compareVersions = null;

  function checkEqual(v1, v2)
  {
    assert.strictEqual(compareVersions(v1, v2), 0,
                       `${v1} and ${v2} should be equal`);
  }

  function checkLessThan(v1, v2)
  {
    assert.strictEqual(compareVersions(v1, v2), -1,
                       `${v1} should be less than ${v2}`);
  }

  function checkGreaterThan(v1, v2)
  {
    assert.strictEqual(compareVersions(v1, v2), 1,
                       `${v1} should be greater than ${v2}`);
  }

  function compare(op, v1, v2)
  {
    if (op == "=")
      checkEqual(v1, v2);
    else if (op == "<")
      checkLessThan(v1, v2);
    else if (op == ">")
      checkGreaterThan(v1, v2);
  }

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {compareVersions} = sandboxedRequire("../lib/versions.js")
    );
  });

  it("should return 0 for 1 and 1", function()
  {
    compare("=", "1", "1");
  });

  it("should return 0 for 0.1 and 0.1", function()
  {
    compare("=", "0.1", "0.1");
  });

  it("should return 0 for 0.12 and 0.12", function()
  {
    compare("=", "0.12", "0.12");
  });

  it("should return 0 for 1.0 and 1.0", function()
  {
    compare("=", "1.0", "1.0");
  });

  it("should return 0 for 1.0a and 1.0a", function()
  {
    compare("=", "1.0a", "1.0a");
  });

  it("should return 0 for 1.0b1 and 1.0b1", function()
  {
    compare("=", "1.0b1", "1.0b1");
  });

  it("should return 0 for 1.0b1.2749 and 1.0b1.2749", function()
  {
    compare("=", "1.0b1.2749", "1.0b1.2749");
  });

  it("should return 0 for 1.0beta and 1.0beta", function()
  {
    compare("=", "1.0beta", "1.0beta");
  });

  it("should return 0 for 1.0beta1 and 1.0beta1", function()
  {
    compare("=", "1.0beta1", "1.0beta1");
  });

  it("should return 0 for 2.1alpha3 and 2.1alpha3", function()
  {
    compare("=", "2.1alpha3", "2.1alpha3");
  });

  // Note: Each of the tests below is repeated with the arguments swapped.

  // 0[.x] < 1[.x]

  it("should return -1 for 0.1 and 1", function()
  {
    compare("<", "0.1", "1");
  });

  it("should return 1 for 1 and 0.1", function()
  {
    compare(">", "1", "0.1");
  });

  it("should return -1 for 0.3 and 1", function()
  {
    compare("<", "0.3", "1");
  });

  it("should return 1 for 1 and 0.3", function()
  {
    compare(">", "1", "0.3");
  });

  it("should return -1 for 0.1 and 1.0", function()
  {
    compare("<", "0.1", "1.0");
  });

  it("should return 1 for 1.0 and 0.1", function()
  {
    compare(">", "1.0", "0.1");
  });

  it("should return -1 for 0.3 and 1.2", function()
  {
    compare("<", "0.3", "1.2");
  });

  it("should return 1 for 1.2 and 0.3", function()
  {
    compare(">", "1.2", "0.3");
  });

  // 1[.x] < 2[.x]

  it("should return -1 for 1 and 2", function()
  {
    compare("<", "1", "2");
  });

  it("should return 1 for 2 and 1", function()
  {
    compare(">", "2", "1");
  });

  it("should return -1 for 1.1 and 2", function()
  {
    compare("<", "1.1", "2");
  });

  it("should return 1 for 2 and 1.1", function()
  {
    compare(">", "2", "1.1");
  });

  it("should return -1 for 1.2 and 2", function()
  {
    compare("<", "1.2", "2");
  });

  it("should return 1 for 2 and 1.2", function()
  {
    compare(">", "2", "1.2");
  });

  it("should return -1 for 1.4 and 2", function()
  {
    compare("<", "1.4", "2");
  });

  it("should return 1 for 2 and 1.4", function()
  {
    compare(">", "2", "1.4");
  });

  it("should return -1 for 1 and 2.0", function()
  {
    compare("<", "1", "2.0");
  });

  it("should return 1 for 2.0 and 1", function()
  {
    compare(">", "2.0", "1");
  });

  it("should return -1 for 1 and 2.1", function()
  {
    compare("<", "1", "2.1");
  });

  it("should return 1 for 2.1 and 1", function()
  {
    compare(">", "2.1", "1");
  });

  it("should return -1 for 1 and 2.3", function()
  {
    compare("<", "1", "2.3");
  });

  it("should return 1 for 2.3 and 1", function()
  {
    compare(">", "2.3", "1");
  });

  it("should return -1 for 1.1 and 2.0", function()
  {
    compare("<", "1.1", "2.0");
  });

  it("should return 1 for 2.0 and 1.1", function()
  {
    compare(">", "2.0", "1.1");
  });

  it("should return -1 for 1.2 and 2.1", function()
  {
    compare("<", "1.2", "2.1");
  });

  it("should return 1 for 2.1 and 1.2", function()
  {
    compare(">", "2.1", "1.2");
  });

  it("should return -1 for 1.4 and 2.3", function()
  {
    compare("<", "1.4", "2.3");
  });

  it("should return 1 for 2.3 and 1.4", function()
  {
    compare(">", "2.3", "1.4");
  });

  // 1.0 beta < 1.0

  it("should return -1 for 1.0b and 1.0", function()
  {
    compare("<", "1.0b", "1.0");
  });

  it("should return 1 for 1.0 and 1.0b", function()
  {
    compare(">", "1.0", "1.0b");
  });

  it("should return -1 for 1.0beta and 1.0", function()
  {
    compare("<", "1.0beta", "1.0");
  });

  it("should return 1 for 1.0 and 1.0beta", function()
  {
    compare(">", "1.0", "1.0beta");
  });

  // 1.0 beta 1 < 1.0

  it("should return -1 for 1.0b1 and 1.0", function()
  {
    compare("<", "1.0b1", "1.0");
  });

  it("should return 1 for 1.0 and 1.0b1", function()
  {
    compare(">", "1.0", "1.0b1");
  });

  it("should return -1 for 1.0beta1 and 1.0", function()
  {
    compare("<", "1.0beta1", "1.0");
  });

  it("should return 1 for 1.0 and 1.0beta1", function()
  {
    compare(">", "1.0", "1.0beta1");
  });

  // 1.0 alpha < 1.0 beta

  it("should return -1 for 1.0a and 1.0b", function()
  {
    compare("<", "1.0a", "1.0b");
  });

  it("should return 1 for 1.0b and 1.0a", function()
  {
    compare(">", "1.0b", "1.0a");
  });

  it("should return -1 for 1.0alpha and 1.0beta", function()
  {
    compare("<", "1.0alpha", "1.0beta");
  });

  it("should return 1 for 1.0beta and 1.0alpha", function()
  {
    compare(">", "1.0beta", "1.0alpha");
  });

  // 1.0 alpha < 1.0 alpha 2

  it("should return -1 for 1.0a and 1.0a2", function()
  {
    compare("<", "1.0a", "1.0a2");
  });

  it("should return 1 for 1.0a2 and 1.0a", function()
  {
    compare(">", "1.0a2", "1.0a");
  });

  it("should return -1 for 1.0alpha and 1.0alpha2", function()
  {
    compare("<", "1.0alpha", "1.0alpha2");
  });

  it("should return 1 for 1.0alpha2 and 1.0alpha", function()
  {
    compare(">", "1.0alpha2", "1.0alpha");
  });

  // 1.0 alpha 1 < 1.0 alpha 2

  it("should return -1 for 1.0a1 and 1.0a2", function()
  {
    compare("<", "1.0a1", "1.0a2");
  });

  it("should return 1 for 1.0a2 and 1.0a1", function()
  {
    compare(">", "1.0a2", "1.0a1");
  });

  it("should return -1 for 1.0alpha1 and 1.0alpha2", function()
  {
    compare("<", "1.0alpha1", "1.0alpha2");
  });

  it("should return 1 for 1.0alpha2 and 1.0alpha1", function()
  {
    compare(">", "1.0alpha2", "1.0alpha1");
  });

  // 1.0 alpha 2 < 1.0 beta 1

  it("should return -1 for 1.0a2 and 1.0b1", function()
  {
    compare("<", "1.0a2", "1.0b1");
  });

  it("should return 1 for 1.0b1 and 1.0a2", function()
  {
    compare(">", "1.0b1", "1.0a2");
  });

  it("should return -1 for 1.0alpha2 and 1.0beta1", function()
  {
    compare("<", "1.0alpha2", "1.0beta1");
  });

  it("should return 1 for 1.0beta1 and 1.0alpha2", function()
  {
    compare(">", "1.0beta1", "1.0alpha2");
  });

  // 1.0 < 1.1 beta

  it("should return -1 for 1.0 and 1.1b", function()
  {
    compare("<", "1.0", "1.1b");
  });

  it("should return 1 for 1.1b and 1.0", function()
  {
    compare(">", "1.1b", "1.0");
  });

  it("should return -1 for 1.0 and 1.1beta", function()
  {
    compare("<", "1.0", "1.1beta");
  });

  it("should return 1 for 1.1beta and 1.0", function()
  {
    compare(">", "1.1beta", "1.0");
  });

  // 1.0 beta < 1.1

  it("should return -1 for 1.0b and 1.1", function()
  {
    compare("<", "1.0b", "1.1");
  });

  it("should return 1 for 1.1 and 1.0b", function()
  {
    compare(">", "1.1", "1.0b");
  });

  it("should return -1 for 1.0beta and 1.1", function()
  {
    compare("<", "1.0beta", "1.1");
  });

  it("should return 1 for 1.1 and 1.0beta", function()
  {
    compare(">", "1.1", "1.0beta");
  });

  // 1.0 beta < 1.1 alpha

  it("should return -1 for 1.0b and 1.1a", function()
  {
    compare("<", "1.0b", "1.1a");
  });

  it("should return 1 for 1.1a and 1.0b", function()
  {
    compare(">", "1.1a", "1.0b");
  });

  it("should return -1 for 1.0beta and 1.1alpha", function()
  {
    compare("<", "1.0beta", "1.1alpha");
  });

  it("should return 1 for 1.1alpha and 1.0beta", function()
  {
    compare(">", "1.1alpha", "1.0beta");
  });

  // 1.0 beta 1 < 1.1 alpha 1

  it("should return -1 for 1.0b1 and 1.1a1", function()
  {
    compare("<", "1.0b1", "1.1a1");
  });

  it("should return 1 for 1.1a1 and 1.0b1", function()
  {
    compare(">", "1.1a1", "1.0b1");
  });

  it("should return -1 for 1.0beta1 and 1.1alpha1", function()
  {
    compare("<", "1.0beta1", "1.1alpha1");
  });

  it("should return 1 for 1.1alpha1 and 1.0beta1", function()
  {
    compare(">", "1.1alpha1", "1.0beta1");
  });

  // 1.0 beta 3 < 1.1 alpha 1

  it("should return -1 for 1.0b3 and 1.1a1", function()
  {
    compare("<", "1.0b3", "1.1a1");
  });

  it("should return 1 for 1.1a1 and 1.0b3", function()
  {
    compare(">", "1.1a1", "1.0b3");
  });

  it("should return -1 for 1.0beta3 and 1.1alpha1", function()
  {
    compare("<", "1.0beta3", "1.1alpha1");
  });

  it("should return 1 for 1.1alpha1 and 1.0beta3", function()
  {
    compare(">", "1.1alpha1", "1.0beta3");
  });

  // 1.1 < 2.0 beta

  it("should return -1 for 1.1 and 2.0b", function()
  {
    compare("<", "1.1", "2.0b");
  });

  it("should return 1 for 2.0b and 1.1", function()
  {
    compare(">", "2.0b", "1.1");
  });

  it("should return -1 for 1.1 and 2.0beta", function()
  {
    compare("<", "1.1", "2.0beta");
  });

  it("should return 1 for 2.0beta and 1.1", function()
  {
    compare(">", "2.0beta", "1.1");
  });

  // 1.1 beta < 2.0

  it("should return -1 for 1.1b and 2.0", function()
  {
    compare("<", "1.1b", "2.0");
  });

  it("should return 1 for 2.0 and 1.1b", function()
  {
    compare(">", "2.0", "1.1b");
  });

  it("should return -1 for 1.1beta and 2.0", function()
  {
    compare("<", "1.1beta", "2.0");
  });

  it("should return 1 for 2.0 and 1.1beta", function()
  {
    compare(">", "2.0", "1.1beta");
  });

  // 1.1 beta < 2.0 alpha

  it("should return -1 for 1.1b and 2.0a", function()
  {
    compare("<", "1.1b", "2.0a");
  });

  it("should return 1 for 2.0a and 1.1b", function()
  {
    compare(">", "2.0a", "1.1b");
  });

  it("should return -1 for 1.1beta and 2.0alpha", function()
  {
    compare("<", "1.1beta", "2.0alpha");
  });

  it("should return 1 for 2.0alpha and 1.1beta", function()
  {
    compare(">", "2.0alpha", "1.1beta");
  });

  // 1.1 beta 1 < 2.0 alpha 1

  it("should return -1 for 1.1b1 and 2.0a1", function()
  {
    compare("<", "1.1b1", "2.0a1");
  });

  it("should return 1 for 2.0a1 and 1.1b1", function()
  {
    compare(">", "2.0a1", "1.1b1");
  });

  it("should return -1 for 1.1beta1 and 2.0alpha1", function()
  {
    compare("<", "1.1beta1", "2.0alpha1");
  });

  it("should return 1 for 2.0alpha1 and 1.1beta1", function()
  {
    compare(">", "2.0alpha1", "1.1beta1");
  });

  // 1.1 beta 3 < 2.0 alpha 1

  it("should return -1 for 1.1b3 and 2.0a1", function()
  {
    compare("<", "1.1b3", "2.0a1");
  });

  it("should return 1 for 2.0a1 and 1.1b3", function()
  {
    compare(">", "2.0a1", "1.1b3");
  });

  it("should return -1 for 1.1beta3 and 2.0alpha1", function()
  {
    compare("<", "1.1beta3", "2.0alpha1");
  });

  it("should return 1 for 2.0alpha1 and 1.1beta3", function()
  {
    compare(">", "2.0alpha1", "1.1beta3");
  });

  // Revisions

  it("should return -1 for 1.0b1.2749 and 1.0b1.2791", function()
  {
    compare("<", "1.0b1.2749", "1.0b1.2791");
  });

  it("should return 1 for 1.0b1.2791 and 1.0b1.2749", function()
  {
    compare(">", "1.0b1.2791", "1.0b1.2749");
  });

  it("should return -1 for 1.0b1.2749 and 1.0b2.1297", function()
  {
    compare("<", "1.0b1.2749", "1.0b2.1297");
  });

  it("should return 1 for 1.0b2.1297 and 1.0b1.2749", function()
  {
    compare(">", "1.0b2.1297", "1.0b1.2749");
  });

  it("should return -1 for 1.0a1.9241 and 1.0b1.2749", function()
  {
    compare("<", "1.0a1.9241", "1.0b1.2749");
  });

  it("should return 1 for 1.0b1.2749 and 1.0a1.9241", function()
  {
    compare(">", "1.0b1.2749", "1.0a1.9241");
  });
});
