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
const fs = require("fs");
const path = require("path");
const {promisify} = require("util");
const {createSandbox, unexpectedError} = require("./_common");

let Filter = null;
let filterStorage = null;
let IO = null;
let Prefs = null;
let SpecialSubscription = null;

describe("Filter storage read/write", function()
{
  beforeEach(function()
  {
    let sandboxedRequire = createSandbox();
    (
      {Filter} = sandboxedRequire("../lib/filterClasses"),
      {filterStorage} = sandboxedRequire("../lib/filterStorage"),
      {IO} = sandboxedRequire("./stub-modules/io"),
      {Prefs} = sandboxedRequire("./stub-modules/prefs"),
      {SpecialSubscription} = sandboxedRequire("../lib/subscriptionClasses")
    );

    filterStorage.addFilter(Filter.fromText("foobar"));
  });

  async function readData()
  {
    let dataPath = path.resolve(__dirname, "data", "patterns.ini");
    let readFileAsync = promisify(fs.readFile);
    let data = await readFileAsync(dataPath, "utf-8");
    return data.split(/[\r\n]+/);
  }

  let testData = readData();

  function canonize(data)
  {
    let curSection = null;
    let sections = [];
    for (let line of data)
    {
      if (/^\[.*\]$/.test(line))
      {
        if (curSection)
          sections.push(curSection);

        curSection = {header: line, data: []};
      }
      else if (curSection && /\S/.test(line))
      {
        curSection.data.push(line);
      }
    }
    if (curSection)
      sections.push(curSection);

    for (let section of sections)
    {
      section.key = section.header + " " + section.data[0];
      section.data.sort();
    }
    sections.sort((a, b) =>
    {
      if (a.key < b.key)
        return -1;
      else if (a.key > b.key)
        return 1;
      return 0;
    });
    return sections;
  }

  async function testReadWrite(withEmptySpecial)
  {
    assert.ok(!filterStorage.initialized, "Uninitialized before the first load");

    try
    {
      let data = await testData;

      IO._setFileContents(filterStorage.sourceFile, data);
      await filterStorage.loadFromDisk();

      assert.ok(filterStorage.initialized, "Initialize after the first load");
      assert.equal(filterStorage.fileProperties.version, filterStorage.formatVersion, "File format version");

      if (withEmptySpecial)
      {
        let specialSubscription =
          SpecialSubscription.createForFilter(Filter.fromText("!foo"));
        filterStorage.addSubscription(specialSubscription);

        filterStorage.removeFilter(Filter.fromText("!foo"), specialSubscription);

        assert.equal(specialSubscription.filterCount, 0,
                     "No filters in special subscription");
        assert.ok(new Set(filterStorage.subscriptions()).has(specialSubscription),
                  "Empty special subscription still in storage");
      }

      await filterStorage.saveToDisk();

      let expected = await testData;

      assert.deepEqual(canonize(IO._getFileContents(filterStorage.sourceFile)),
                       canonize(expected), "Read/write result");
    }
    catch (error)
    {
      unexpectedError.call(assert, error);
    }
  }

  describe("Read and save", function()
  {
    it("To file", function()
    {
      this.timeout(5000);

      return testReadWrite();
    });

    it("To file with empty special subscription", function()
    {
      this.timeout(5000);

      return testReadWrite(true);
    });
  });

  it("Import/export", async function()
  {
    this.timeout(5000);

    try
    {
      let lines = await testData;

      if (lines.length && lines[lines.length - 1] == "")
        lines.pop();

      let importer = filterStorage.importData();
      for (let line of lines)
        importer(line);
      importer(null);

      assert.equal(filterStorage.fileProperties.version, filterStorage.formatVersion, "File format version");

      // Testing for compatibility changes for
      // https://gitlab.com/eyeo/adblockplus/adblockpluscore/-/issues/140
      let found = false;
      filterStorage._knownSubscriptions.forEach(sub =>
      {
        if (sub.url == "~user~12345")
        {
          found = true;
          assert.deepEqual(sub.defaults, ["allowing"]);
        }
      });
      assert.ok(found, "Allowlist subscription not found");

      let exported = Array.from(filterStorage.exportData());
      assert.deepEqual(canonize(exported), canonize(lines), "Import/export result");
    }
    catch (error)
    {
      unexpectedError.call(assert, error);
    }
  });

  describe("Backups", function()
  {
    it("Saving without", async function()
    {
      Prefs.patternsbackups = 0;
      Prefs.patternsbackupinterval = 24;

      try
      {
        await filterStorage.saveToDisk();
        await filterStorage.saveToDisk();

        assert.ok(!IO._getFileContents(filterStorage.getBackupName(1)),
                  "Backup shouldn't be created");
      }
      catch (error)
      {
        unexpectedError.call(assert, error);
      }
    });

    it("Saving with", async function()
    {
      Prefs.patternsbackups = 2;
      Prefs.patternsbackupinterval = 24;

      let backupFile = filterStorage.getBackupName(1);
      let backupFile2 = filterStorage.getBackupName(2);
      let backupFile3 = filterStorage.getBackupName(3);

      let oldModifiedTime;

      try
      {
        await filterStorage.saveToDisk();

        // Save again immediately
        await filterStorage.saveToDisk();

        assert.ok(IO._getFileContents(backupFile), "First backup created");

        oldModifiedTime = IO._getModifiedTime(backupFile) - 10000;
        IO._setModifiedTime(backupFile, oldModifiedTime);
        await filterStorage.saveToDisk();

        assert.equal(IO._getModifiedTime(backupFile), oldModifiedTime, "Backup not overwritten if it is only 10 seconds old");

        oldModifiedTime -= 40 * 60 * 60 * 1000;
        IO._setModifiedTime(backupFile, oldModifiedTime);
        await filterStorage.saveToDisk();

        assert.notEqual(IO._getModifiedTime(backupFile), oldModifiedTime, "Backup overwritten if it is 40 hours old");

        assert.ok(IO._getFileContents(backupFile2), "Second backup created when first backup is overwritten");

        IO._setModifiedTime(backupFile, IO._getModifiedTime(backupFile) - 20000);
        oldModifiedTime = IO._getModifiedTime(backupFile2);
        await filterStorage.saveToDisk();

        assert.equal(IO._getModifiedTime(backupFile2), oldModifiedTime, "Second backup not overwritten if first one is only 20 seconds old");

        IO._setModifiedTime(backupFile, IO._getModifiedTime(backupFile) - 25 * 60 * 60 * 1000);
        oldModifiedTime = IO._getModifiedTime(backupFile2);
        await filterStorage.saveToDisk();

        assert.notEqual(IO._getModifiedTime(backupFile2), oldModifiedTime, "Second backup overwritten if first one is 25 hours old");

        assert.ok(!IO._getFileContents(backupFile3), "Third backup not created with patternsbackups = 2");
      }
      catch (error)
      {
        unexpectedError.call(assert, error);
      }
    });

    it("Restoring", async function()
    {
      Prefs.patternsbackups = 2;
      Prefs.patternsbackupinterval = 24;

      try
      {
        await filterStorage.saveToDisk();

        assert.equal([...filterStorage.subscriptions()][0].filterCount, 1, "Initial filter count");
        filterStorage.addFilter(Filter.fromText("barfoo"));
        assert.equal([...filterStorage.subscriptions()][0].filterCount, 2, "Filter count after adding a filter");
        await filterStorage.saveToDisk();

        await filterStorage.loadFromDisk();

        assert.equal([...filterStorage.subscriptions()][0].filterCount, 2, "Filter count after adding filter and reloading");
        await filterStorage.restoreBackup(1);

        assert.equal([...filterStorage.subscriptions()][0].filterCount, 1, "Filter count after restoring backup");
        await filterStorage.loadFromDisk();

        assert.equal([...filterStorage.subscriptions()][0].filterCount, 1, "Filter count after reloading");
      }
      catch (error)
      {
        unexpectedError.call(assert, error);
      }
    });
  });
});
