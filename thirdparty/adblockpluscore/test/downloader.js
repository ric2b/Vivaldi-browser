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

describe("Downloader.isValidURL()", function()
{
  let Downloader = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Downloader} = sandboxedRequire("../lib/downloader")
    );
  });

  it("should return true for https://example.com/", function()
  {
    assert.strictEqual(Downloader.isValidURL("https://example.com/"), true);
  });

  it("should return true for https://example.com/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("https://example.com/list.txt"), true);
  });

  it("should return true for https://example.com:8080/", function()
  {
    assert.strictEqual(Downloader.isValidURL("https://example.com:8080/"), true);
  });

  it("should return true for https://example.com:8080/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("https://example.com:8080/list.txt"), true);
  });

  it("should return false for http://example.com/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://example.com/"), false);
  });

  it("should return false for http://example.com/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://example.com/list.txt"), false);
  });

  it("should return false for http://example.com:8080/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://example.com:8080/"), false);
  });

  it("should return false for http://example.com:8080/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://example.com:8080/list.txt"), false);
  });

  it("should return true for https:example.com/", function()
  {
    assert.strictEqual(Downloader.isValidURL("https:example.com/"), true);
  });

  it("should return true for https:example.com/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("https:example.com/list.txt"), true);
  });

  it("should return false for http:example.com/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http:example.com/"), false);
  });

  it("should return false for http:example.com/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http:example.com/list.txt"), false);
  });

  it("should return true for http://localhost/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://localhost/"), true);
  });

  it("should return true for http://localhost/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://localhost/list.txt"), true);
  });

  it("should return true for http://127.0.0.1/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://127.0.0.1/"), true);
  });

  it("should return true for http://127.0.0.1/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://127.0.0.1/list.txt"), true);
  });

  it("should return true for http://[::1]/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://[::1]/"), true);
  });

  it("should return true for http://[::1]/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://[::1]/list.txt"), true);
  });

  it("should return true for http://0x7f000001/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://0x7f000001/"), true);
  });

  it("should return true for http://0x7f000001/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://0x7f000001/list.txt"), true);
  });

  it("should return true for http://[0:0:0:0:0:0:0:1]/", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://[0:0:0:0:0:0:0:1]/"), true);
  });

  it("should return true for http://[0:0:0:0:0:0:0:1]/list.txt", function()
  {
    assert.strictEqual(Downloader.isValidURL("http://[0:0:0:0:0:0:0:1]/list.txt"), true);
  });

  it("should return true for data:,Hello%2C%20World!", function()
  {
    assert.strictEqual(Downloader.isValidURL("data:,Hello%2C%20World!"), true);
  });

  it("should return true for data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==", function()
  {
    assert.strictEqual(Downloader.isValidURL("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="), true);
  });
});
