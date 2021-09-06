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

describe("analytics", function()
{
  let analytics = null;
  let Prefs = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {analytics} = sandboxedRequire("../lib/analytics"),
      {Prefs} = sandboxedRequire("./stub-modules/prefs")
    );
  });

  describe("#isTrusted()", function()
  {
    context("No trusted hosts", function()
    {
      it("should return false for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), false);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return false for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), false);
      });

      it("should return false for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), false);
      });

      it("should return false for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return false for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), false);
      });

      it("should return false for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), false);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: example.com", function()
    {
      beforeEach(function()
      {
        Prefs.analytics = {trustedHosts: ["example.com"]};
      });

      it("should return true for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), true);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return true for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), true);
      });

      it("should return true for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), true);
      });

      it("should return false for https://foo.example.com/", function()
      {
        // Subdomains are not trusted automatically.
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return true for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), true);
      });

      it("should return true for https:example.com:443", function()
      {
        // Since port 443 is the default for HTTPS, the URL
        // https:example.com:443 is canonicalized to https://example.com/ and
        // is therefore trusted.
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), true);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        // Invalid URLs are never trusted.
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: foo.example.com", function()
    {
      beforeEach(function()
      {
        Prefs.analytics = {trustedHosts: ["foo.example.com"]};
      });

      it("should return false for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), false);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return false for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), false);
      });

      it("should return false for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), false);
      });

      it("should return true for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), true);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return false for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), false);
      });

      it("should return false for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), false);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: example.com, foo.example.net", function()
    {
      beforeEach(function()
      {
        Prefs.analytics = {trustedHosts: ["example.com", "foo.example.net"]};
      });

      it("should return true for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), true);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return true for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), true);
      });

      it("should return true for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), true);
      });

      it("should return false for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return true for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), true);
      });

      it("should return true for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), true);
      });

      it("should return true for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), true);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: example.com:8080", function()
    {
      beforeEach(function()
      {
        Prefs.analytics = {trustedHosts: ["example.com:8080"]};
      });

      it("should return false for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), false);
      });

      it("should return true for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), true);
      });

      it("should return false for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), false);
      });

      it("should return false for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), false);
      });

      it("should return false for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return false for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), false);
      });

      it("should return false for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), false);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: example.com:443", function()
    {
      beforeEach(function()
      {
        // Invalid entry, because https://example.com:443/ is always
        // canonicalized to https://example.com/
        Prefs.analytics = {trustedHosts: ["example.com:443"]};
      });

      it("should return false for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), false);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return false for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), false);
      });

      it("should return false for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), false);
      });

      it("should return false for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return false for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), false);
      });

      it("should return false for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), false);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: localhost", function()
    {
      beforeEach(function()
      {
        Prefs.analytics = {trustedHosts: ["localhost"]};
      });

      it("should return false for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), false);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return false for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), false);
      });

      it("should return false for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), false);
      });

      it("should return false for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return false for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), false);
      });

      it("should return false for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), false);
      });

      it("should return true for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), true);
      });

      it("should return false for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), false);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });

    context("Trusted hosts: 127.0.0.1", function()
    {
      beforeEach(function()
      {
        Prefs.analytics = {trustedHosts: ["127.0.0.1"]};
      });

      it("should return false for https://example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/"), false);
      });

      it("should return false for https://example.com:8080/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com:8080/"), false);
      });

      it("should return false for https://example.com/filters.txt", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt"), false);
      });

      it("should return false for https://example.com/filters.txt?source=adblockplus.org", function()
      {
        assert.strictEqual(analytics.isTrusted("https://example.com/filters.txt?source=adblockplus.org"), false);
      });

      it("should return false for https://foo.example.com/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.com/"), false);
      });

      it("should return false for https://foo.example.net/", function()
      {
        assert.strictEqual(analytics.isTrusted("https://foo.example.net/"), false);
      });

      it("should return false for https:example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com"), false);
      });

      it("should return false for https:example.com:443", function()
      {
        assert.strictEqual(analytics.isTrusted("https:example.com:443"), false);
      });

      it("should return false for http://localhost/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://localhost/"), false);
      });

      it("should return true for http://127.0.0.1/", function()
      {
        assert.strictEqual(analytics.isTrusted("http://127.0.0.1/"), true);
      });

      it("should return false for example.com", function()
      {
        assert.strictEqual(analytics.isTrusted("example.com"), false);
      });
    });
  });

  describe("#getFirstVersion()", function()
  {
    context("Fresh installation", function()
    {
      context("Analytics off", function()
      {
        context("No first version", function()
        {
          it("should return null", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), null);
          });
        });

        context("First version: 201905071234", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T12:34Z. This gives us the initial
            // value.
            analytics.recordVersion("201905071234");
          });

          it("should return null after 0 days (version: 201905071234)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), null);
          });
        });
      });

      context("Analytics on", function()
      {
        beforeEach(function()
        {
          Prefs.analytics = {};
        });

        context("No first version", function()
        {
          it("should return '0'", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "0");
          });
        });

        context("First version: 201905071234", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T12:34Z. This gives us the initial
            // value.
            analytics.recordVersion("201905071234");
          });

          it("should return '20190507' after 0 days (version: 201905071234)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "20190507");
          });

          it("should return '20190507' after 30 days (versions: 201905071234, 201906061234)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            assert.strictEqual(analytics.getFirstVersion(), "20190507");
          });

          it("should return '201905' after 30 days and 1 minute (versions: 201905071234, 201906061234, 201906061235)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            assert.strictEqual(analytics.getFirstVersion(), "201905");
          });

          it("should return '201905' after 365 days (versions: 201905071234, 201906061234, 201906061235, 202005061234)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            // 365 days since the first download.
            analytics.recordVersion("202005061234");

            assert.strictEqual(analytics.getFirstVersion(), "201905");
          });

          it("should return '2019' after 365 days and 1 minute (versions: 201905071234, 201906061234, 201906061235, 202005061234, 202005061235)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            // 365 days since the first download.
            analytics.recordVersion("202005061234");

            // 365 days and 1 minute since the first download.
            analytics.recordVersion("202005061235");

            assert.strictEqual(analytics.getFirstVersion(), "2019");
          });

          it("should return '2019' after 2,557 days (~7 years) (versions: 201905071234, 201906061234, 201906061235, 202005061234, 202005061235, 202605071234)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            // 365 days since the first download.
            analytics.recordVersion("202005061234");

            // 365 days and 1 minute since the first download.
            analytics.recordVersion("202005061235");

            // 2,557 days (~7 years) since the first download.
            analytics.recordVersion("202605071234");

            assert.strictEqual(analytics.getFirstVersion(), "2019");
          });
        });

        // Repeat tests with hour-level precision.
        context("First version: 2019050712 (hour-level precision)", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T12:00Z. This gives us the initial
            // value.
            analytics.recordVersion("2019050712");
          });

          it("should return '20190507' after 0 days (version: 2019050712)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "20190507");
          });

          it("should return '20190507' after 30 days (versions: 2019050712, 2019060612)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            assert.strictEqual(analytics.getFirstVersion(), "20190507");
          });

          it("should return '201905' after 30 days and 1 hour (versions: 2019050712, 2019060612, 2019060613)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            assert.strictEqual(analytics.getFirstVersion(), "201905");
          });

          it("should return '201905' after 365 days (versions: 2019050712, 2019060612, 2019060613, 2020050612)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            // 365 days since the first download.
            analytics.recordVersion("2020050612");

            assert.strictEqual(analytics.getFirstVersion(), "201905");
          });

          it("should return '2019' after 365 days and 1 hour (versions: 2019050712, 2019060612, 2019060613, 2020050612, 2020050613)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            // 365 days since the first download.
            analytics.recordVersion("2020050612");

            // 365 days and 1 hour since the first download.
            analytics.recordVersion("2020050613");

            assert.strictEqual(analytics.getFirstVersion(), "2019");
          });

          it("should return '2019' after 2,557 days (~7 years) (versions: 2019050712, 2019060612, 2019060613, 2020050612, 2020050613, 2026050712)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            // 365 days since the first download.
            analytics.recordVersion("2020050612");

            // 365 days and 1 hour since the first download.
            analytics.recordVersion("2020050613");

            // 2,557 days (~7 years) since the first download.
            analytics.recordVersion("2026050712");

            assert.strictEqual(analytics.getFirstVersion(), "2019");
          });
        });

        // Repeat tests with day-level precision.
        context("First version: 20190507 (day-level precision)", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T00:00Z. This gives us the initial
            // value.
            analytics.recordVersion("20190507");
          });

          it("should return '20190507' after 0 days (version: 20190507)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "20190507");
          });

          it("should return '20190507' after 30 days (versions: 20190507, 20190606)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            assert.strictEqual(analytics.getFirstVersion(), "20190507");
          });

          it("should return '201905' after 31 days (versions: 20190507, 20190606, 20190607)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            assert.strictEqual(analytics.getFirstVersion(), "201905");
          });

          it("should return '201905' after 365 days (versions: 20190507, 20190606, 20190607, 20200506)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            // 365 days since the first download.
            analytics.recordVersion("20200506");

            assert.strictEqual(analytics.getFirstVersion(), "201905");
          });

          it("should return '2019' after 366 days (versions: 20190507, 20190606, 20190607, 20200506, 20200507)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            // 365 days since the first download.
            analytics.recordVersion("20200506");

            // 366 days since the first download.
            analytics.recordVersion("20200507");

            assert.strictEqual(analytics.getFirstVersion(), "2019");
          });

          it("should return '2019' after 2,557 days (~7 years) (versions: 20190507, 20190606, 20190607, 20200506, 20200507, 20260507)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            // 365 days since the first download.
            analytics.recordVersion("20200506");

            // 366 days since the first download.
            analytics.recordVersion("20200507");

            // 2,557 days (~7 years) since the first download.
            analytics.recordVersion("20260507");

            assert.strictEqual(analytics.getFirstVersion(), "2019");
          });
        });
      });
    });

    context("Existing installation", function()
    {
      beforeEach(function()
      {
        Prefs.notificationdata = {data: {}};
      });

      context("Analytics off", function()
      {
        context("No first version", function()
        {
          it("should return null", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), null);
          });
        });

        context("First version: 201905071234", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T12:34Z. This gives us the initial
            // value.
            analytics.recordVersion("201905071234");
          });

          it("should return null after 0 days (version: 201905071234)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), null);
          });
        });
      });

      context("Analytics on", function()
      {
        beforeEach(function()
        {
          Prefs.analytics = {};
        });

        context("No first version", function()
        {
          it("should return '0-E'", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "0-E");
          });
        });

        context("First version: 201905071234", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T12:34Z. This gives us the initial
            // value.
            analytics.recordVersion("201905071234");
          });

          it("should return '20190507-E' after 0 days (version: 201905071234)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "20190507-E");
          });

          it("should return '20190507-E' after 30 days (versions: 201905071234, 201906061234)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            assert.strictEqual(analytics.getFirstVersion(), "20190507-E");
          });

          it("should return '201905-E' after 30 days and 1 minute (versions: 201905071234, 201906061234, 201906061235)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            assert.strictEqual(analytics.getFirstVersion(), "201905-E");
          });

          it("should return '201905-E' after 365 days (versions: 201905071234, 201906061234, 201906061235, 202005061234)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            // 365 days since the first download.
            analytics.recordVersion("202005061234");

            assert.strictEqual(analytics.getFirstVersion(), "201905-E");
          });

          it("should return '2019-E' after 365 days and 1 minute (versions: 201905071234, 201906061234, 201906061235, 202005061234, 202005061235)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            // 365 days since the first download.
            analytics.recordVersion("202005061234");

            // 365 days and 1 minute since the first download.
            analytics.recordVersion("202005061235");

            assert.strictEqual(analytics.getFirstVersion(), "2019-E");
          });

          it("should return '2019-E' after 2,557 days (~7 years) (versions: 201905071234, 201906061234, 201906061235, 202005061234, 202005061235, 202605071234)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("201906061234");

            // 30 days and 1 minute since the first download.
            analytics.recordVersion("201906061235");

            // 365 days since the first download.
            analytics.recordVersion("202005061234");

            // 365 days and 1 minute since the first download.
            analytics.recordVersion("202005061235");

            // 2,557 days (~7 years) since the first download.
            analytics.recordVersion("202605071234");

            assert.strictEqual(analytics.getFirstVersion(), "2019-E");
          });
        });

        // Repeat tests with hour-level precision.
        context("First version: 2019050712 (hour-level precision)", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T12:00Z. This gives us the initial
            // value.
            analytics.recordVersion("2019050712");
          });

          it("should return '20190507-E' after 0 days (version: 2019050712)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "20190507-E");
          });

          it("should return '20190507-E' after 30 days (versions: 2019050712, 2019060612)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            assert.strictEqual(analytics.getFirstVersion(), "20190507-E");
          });

          it("should return '201905-E' after 30 days and 1 hour (versions: 2019050712, 2019060612, 2019060613)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            assert.strictEqual(analytics.getFirstVersion(), "201905-E");
          });

          it("should return '201905-E' after 365 days (versions: 2019050712, 2019060612, 2019060613, 2020050612)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            // 365 days since the first download.
            analytics.recordVersion("2020050612");

            assert.strictEqual(analytics.getFirstVersion(), "201905-E");
          });

          it("should return '2019-E' after 365 days and 1 hour (versions: 2019050712, 2019060612, 2019060613, 2020050612, 2020050613)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            // 365 days since the first download.
            analytics.recordVersion("2020050612");

            // 365 days and 1 hour since the first download.
            analytics.recordVersion("2020050613");

            assert.strictEqual(analytics.getFirstVersion(), "2019-E");
          });

          it("should return '2019-E' after 2,557 days (~7 years) (versions: 2019050712, 2019060612, 2019060613, 2020050612, 2020050613, 2026050712)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("2019060612");

            // 30 days and 1 hour since the first download.
            analytics.recordVersion("2019060613");

            // 365 days since the first download.
            analytics.recordVersion("2020050612");

            // 365 days and 1 hour since the first download.
            analytics.recordVersion("2020050613");

            // 2,557 days (~7 years) since the first download.
            analytics.recordVersion("2026050712");

            assert.strictEqual(analytics.getFirstVersion(), "2019-E");
          });
        });

        // Repeat tests with day-level precision.
        context("First version: 20190507 (day-level precision)", function()
        {
          beforeEach(function()
          {
            // First download on 2019-05-07T00:00Z. This gives us the initial
            // value.
            analytics.recordVersion("20190507");
          });

          it("should return '20190507-E' after 0 days (version: 20190507)", function()
          {
            assert.strictEqual(analytics.getFirstVersion(), "20190507-E");
          });

          it("should return '20190507-E' after 30 days (versions: 20190507, 20190606)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            assert.strictEqual(analytics.getFirstVersion(), "20190507-E");
          });

          it("should return '201905-E' after 31 days (versions: 20190507, 20190606, 20190607)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            assert.strictEqual(analytics.getFirstVersion(), "201905-E");
          });

          it("should return '201905-E' after 365 days (versions: 20190507, 20190606, 20190607, 20200506)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            // 365 days since the first download.
            analytics.recordVersion("20200506");

            assert.strictEqual(analytics.getFirstVersion(), "201905-E");
          });

          it("should return '2019-E' after 366 days (versions: 20190507, 20190606, 20190607, 20200506, 20200507)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            // 365 days since the first download.
            analytics.recordVersion("20200506");

            // 366 days since the first download.
            analytics.recordVersion("20200507");

            assert.strictEqual(analytics.getFirstVersion(), "2019-E");
          });

          it("should return '2019-E' after 2,557 days (~7 years) (versions: 20190507, 20190606, 20190607, 20200506, 20200507, 20260507)", function()
          {
            // 30 days since the first download.
            analytics.recordVersion("20190606");

            // 31 days since the first download.
            analytics.recordVersion("20190607");

            // 365 days since the first download.
            analytics.recordVersion("20200506");

            // 366 days since the first download.
            analytics.recordVersion("20200507");

            // 2,557 days (~7 years) since the first download.
            analytics.recordVersion("20260507");

            assert.strictEqual(analytics.getFirstVersion(), "2019-E");
          });
        });
      });
    });
  });

  describe("#recordVersion()", function()
  {
    context("Fresh installation", function()
    {
      context("Analytics off", function()
      {
        it("should not throw on version 'Invalid version string'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("Invalid version string"));
        });

        it("should not throw on version '201905071234'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("201905071234"));
        });
      });

      context("Analytics on", function()
      {
        beforeEach(function()
        {
          Prefs.analytics = {};
        });

        it("should not throw on version 'Invalid version string'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("Invalid version string"));
        });

        it("should not throw on version '201905071234'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("201905071234"));
        });

        it("should not throw on version '2019050712' (hour-level precision)", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("2019050712"));
        });

        it("should not throw on version '20190507' (day-level precision)", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("20190507"));
        });
      });
    });

    context("Existing installation", function()
    {
      beforeEach(function()
      {
        Prefs.notificationdata = {data: {}};
      });

      context("Analytics off", function()
      {
        it("should not throw on version 'Invalid version string'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("Invalid version string"));
        });

        it("should not throw on version '201905071234'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("201905071234"));
        });
      });

      context("Analytics on", function()
      {
        beforeEach(function()
        {
          Prefs.analytics = {};
        });

        it("should not throw on version 'Invalid version string'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("Invalid version string"));
        });

        it("should not throw on version '201905071234'", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("201905071234"));
        });

        it("should not throw on version '2019050712' (hour-level precision)", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("2019050712"));
        });

        it("should not throw on version '20190507' (day-level precision)", function()
        {
          assert.doesNotThrow(() => analytics.recordVersion("20190507"));
        });
      });
    });
  });
});
