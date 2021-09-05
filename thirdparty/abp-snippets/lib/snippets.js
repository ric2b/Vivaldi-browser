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

/** @module */

"use strict";

/**
 * @fileOverview Snippets implementation.
 */

const {EventEmitter} = require("./events");

let singleCharacterEscapes = new Map([
  ["n", "\n"], ["r", "\r"], ["t", "\t"]
]);

/**
 * `{@link module:snippets.snippets snippets}` implementation.
 */
class Snippets extends EventEmitter
{
  /**
   * @hideconstructor
   */
  constructor()
  {
    super();

    /**
     * All known snippet filters.
     * @type {Set.<module:filterClasses.SnippetFilter>}
     * @private
     */
    this._filters = new Set();
  }

  /**
   * Removes all known snippet filters.
   */
  clear()
  {
    let {_filters} = this;

    if (_filters.size == 0)
      return;

    _filters.clear();

    this.emit("snippets.filtersCleared");
  }

  /**
   * Adds a new snippet filter.
   * @param {module:filterClasses.SnippetFilter} filter
   */
  add(filter)
  {
    let {_filters} = this;
    let {size} = _filters;

    _filters.add(filter);

    if (size != _filters.size)
      this.emit("snippets.filterAdded", filter);
  }

  /**
   * Removes an existing snippet filter.
   * @param {module:filterClasses.SnippetFilter} filter
   */
  remove(filter)
  {
    let {_filters} = this;
    let {size} = _filters;

    _filters.delete(filter);

    if (size != _filters.size)
      this.emit("snippets.filterRemoved", filter);
  }

  /**
   * Checks whether a snippet filter exists.
   * @param {module:filterClasses.SnippetFilter} filter
   * @returns {boolean}
   */
  has(filter)
  {
    return this._filters.has(filter);
  }

  /**
   * Returns a list of all snippet filters active on the given domain.
   * @param {string} domain The domain.
   * @returns {Array.<module:filterClasses.SnippetFilter>} A list of snippet
   *   filters.
   */
  getFilters(domain)
  {
    let result = [];

    for (let filter of this._filters)
    {
      if (filter.isActiveOnDomain(domain))
        result.push(filter);
    }

    return result;
  }

  /**
   * Returns a list of all snippet filters active on the given domain.
   * @param {string} domain The domain.
   * @returns {Array.<module:filterClasses.SnippetFilter>} A list of snippet
   *   filters.
   *
   * @deprecated Use <code>{@link module:snippets~Snippets#getFilters}</code>
   *   instead.
   * @see module:snippets~Snippets#getFilters
   */
  getFiltersForDomain(domain)
  {
    return this.getFilters(domain);
  }
}

/**
 * Container for snippet filters.
 * @type {module:snippets~Snippets}
 */
exports.snippets = new Snippets();

let parseScript =
/**
 * Parses a script and returns a list of all its commands and their arguments.
 * @param {string} script The script.
 * @returns {Array.<string[]>} A list of commands and their arguments.
 * @package
 */
exports.parseScript = function parseScript(script)
{
  let tree = [];

  let escape = false;
  let withinQuotes = false;

  let unicodeEscape = null;

  let quotesClosed = false;

  let call = [];
  let argument = "";

  for (let character of script.trim() + ";")
  {
    let afterQuotesClosed = quotesClosed;
    quotesClosed = false;

    if (unicodeEscape != null)
    {
      unicodeEscape += character;

      if (unicodeEscape.length == 4)
      {
        let codePoint = parseInt(unicodeEscape, 16);
        if (!isNaN(codePoint))
          argument += String.fromCodePoint(codePoint);

        unicodeEscape = null;
      }
    }
    else if (escape)
    {
      escape = false;

      if (character == "u")
        unicodeEscape = "";
      else
        argument += singleCharacterEscapes.get(character) || character;
    }
    else if (character == "\\")
    {
      escape = true;
    }
    else if (character == "'")
    {
      withinQuotes = !withinQuotes;

      if (!withinQuotes)
        quotesClosed = true;
    }
    else if (withinQuotes || character != ";" && !/\s/.test(character))
    {
      argument += character;
    }
    else
    {
      if (argument || afterQuotesClosed)
      {
        call.push(argument);
        argument = "";
      }

      if (character == ";" && call.length > 0)
      {
        tree.push(call);
        call = [];
      }
    }
  }

  return tree;
};

/**
 * Compiles a script against a given list of libraries into executable code.
 * @param {string|Array.<string>} scripts One or more scripts to convert into
 *  executable code.
 * @param {Array.<string>} libraries A list of libraries.
 * @param {object} [environment] An object containing environment variables.
 * @returns {string} Executable code.
 */
exports.compileScript = function compileScript(scripts,
                                               libraries,
                                               environment = {})
{
  return `
    "use strict";
    {
      let libraries = ${JSON.stringify(libraries)};

      let scripts = ${JSON.stringify([].concat(scripts).map(parseScript))};

      let imports = Object.create(null);
      for (let library of libraries)
      {
        let loadLibrary = new Function("exports", "environment", library);
        loadLibrary(imports, ${JSON.stringify(environment)});
      }

      let {hasOwnProperty} = Object.prototype;

      if (hasOwnProperty.call(imports, "prepareInjection"))
        imports.prepareInjection();

      for (let script of scripts)
      {
        for (let [name, ...args] of script)
        {
          if (hasOwnProperty.call(imports, name))
          {
            let value = imports[name];
            if (typeof value == "function")
              value(...args);
          }
        }
      }

      if (hasOwnProperty.call(imports, "commitInjection"))
        imports.commitInjection();
    }
  `;
};
