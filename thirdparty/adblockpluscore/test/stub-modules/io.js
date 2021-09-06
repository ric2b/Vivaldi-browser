"use strict";

let data = new Map();

exports.IO = {
  // Non-public API, for tests only
  _getFileContents(fileName)
  {
    if (data.has(fileName))
      return data.get(fileName).contents;
    return null;
  },
  _setFileContents(fileName, contents)
  {
    data.set(fileName, {
      lastModified: Date.now(),
      contents
    });
  },
  _getModifiedTime(fileName)
  {
    if (data.has(fileName))
      return data.get(fileName).lastModified;
    return 0;
  },
  _setModifiedTime(fileName, lastModified)
  {
    if (data.has(fileName))
      data.get(fileName).lastModified = lastModified;
  },

  // Public API
  async writeToFile(fileName, generator)
  {
    data.set(fileName, {
      lastModified: Date.now(),
      contents: Array.from(generator)
    });
  },
  async readFromFile(fileName, listener)
  {
    if (!data.has(fileName))
      throw new Error("File doesn't exist");

    let lines = data.get(fileName).contents;
    for (let line of lines)
      listener(line);
  },
  async copyFile(fromName, toName)
  {
    if (!data.has(fromName))
      throw new Error("File doesn't exist");

    if (fromName == toName)
      throw new Error("Cannot copy file to itself");

    data.set(toName, data.get(fromName));
  },
  async renameFile(fromName, toName)
  {
    await this.copyFile(fromName, toName);
    await this.removeFile(fromName);
  },
  async removeFile(fileName)
  {
    if (!data.has(fileName))
      throw new Error("File doesn't exist");

    data.delete(fileName);
  },
  async statFile(fileName)
  {
    if (data.has(fileName))
    {
      return {
        exists: true,
        lastModified: data.get(fileName).lastModified
      };
    }

    return {
      exists: false,
      lastModified: 0
    };
  }
};
