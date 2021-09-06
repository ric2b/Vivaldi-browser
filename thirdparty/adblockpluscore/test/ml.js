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
const tf = require("@tensorflow/tfjs");
const tfn = require("@tensorflow/tfjs-node");
const {createSandbox} = require("./_common");

const MODEL_URL = "data/mlHideIfGraphMatches/model.json";
const OTHER_MODEL_URL = "data/mlHideIfGraphMatches/otherModel.json";

describe("ML", function()
{
  let ML = null;

  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {ML} = sandboxedRequire("../lib/ml")
    );
  });

  describe("#constructor()", function()
  {
    // A TensorFlow library must be specified.
    it("should throw when lib is missing", function()
    {
      assert.throws(() => new ML(), Error);
    });

    it("should throw when lib is null", function()
    {
      assert.throws(() => new ML(null), Error);
    });

    // Should work with tf library specified.
    it("should not throw when lib is non-null", function()
    {
      assert.doesNotThrow(() => new ML(tf));
    });
  });

  describe("#get modelURL()", function()
  {
    let ml = null;
    let modelURL = null;
    let otherModelURL = null;

    beforeEach(function()
    {
      ml = new ML(tf);
      modelURL = tfn.io.fileSystem(MODEL_URL);
      otherModelURL = tfn.io.fileSystem(OTHER_MODEL_URL);
    });

    it("should return null if not set", function()
    {
      assert.strictEqual(ml.modelURL, null);
    });

    it("should return value if set", function()
    {
      ml.modelURL = modelURL;
      assert.strictEqual(ml.modelURL, modelURL);
    });

    it("should return null if reset to null", function()
    {
      ml.modelURL = modelURL;
      ml.modelURL = null;
      assert.strictEqual(ml.modelURL, null);
    });

    it("should return value if set to different value", function()
    {
      ml.modelURL = modelURL;
      ml.modelURL = otherModelURL;
      assert.strictEqual(ml.modelURL, otherModelURL);
    });
  });

  describe("#set modelURL()", function()
  {
    let ml = null;
    let modelURL = null;
    let otherModelURL = null;

    beforeEach(function()
    {
      ml = new ML(tf);
      modelURL = tfn.io.fileSystem(MODEL_URL);
      otherModelURL = tfn.io.fileSystem(OTHER_MODEL_URL);
    });

    it("should not throw when set", function()
    {
      assert.doesNotThrow(() => ml.modelURL = modelURL);
    });

    it("should not throw when reset to null", function()
    {
      ml.modelURL = modelURL;
      assert.doesNotThrow(() => ml.modelURL = null);
    });

    it("should not throw when set to different value", function()
    {
      ml.modelURL = modelURL;
      assert.doesNotThrow(() => ml.modelURL = otherModelURL);
    });
  });
});
