/*!
 * This file is part of eyeo's Anti-Circumvention Snippets module (@eyeo/snippets),
 * Copyright (C) 2006-present eyeo GmbH
 * 
 * @eyeo/snippets is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 * 
 * @eyeo/snippets is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with @eyeo/snippets.  If not, see <http://www.gnu.org/licenses/>.
 */

/* global chrome, browser, globalThis */

import env from "./utils/env.js";
import transformer from "./utils/transformer.js";
import {call} from "proxy-pants/function";
import {chain} from "proxy-pants/chain";

const {
  isExtensionContext,
  Array,
  Number,
  String,
  Object
} = env;

const {isArray} = Array;
const {getOwnPropertyDescriptor, setPrototypeOf} = Object;

const {toString} = Object.prototype;
const {slice} = String.prototype;
const getBrand = value => call(slice, call(toString, value), 8, -1);

const {get: nodeType} = getOwnPropertyDescriptor(Node.prototype, "nodeType");

// the main difference between secured classes and chained prototypes
// is that chained values are not something we construct at all, it's
// something we deal with instead, so that proxies are a better option,
// or better, are less obtrusive if their proxy don't leak in the wild.
const chained = isExtensionContext ? {} : {
  Attr: chain(Attr),
  CanvasRenderingContext2D: chain(CanvasRenderingContext2D),
  CSSStyleDeclaration: chain(CSSStyleDeclaration),
  Document: chain(Document),
  Element: chain(Element),
  HTMLCanvasElement: chain(HTMLCanvasElement),
  HTMLElement: chain(HTMLElement),
  HTMLImageElement: chain(HTMLImageElement),
  HTMLScriptElement: chain(HTMLScriptElement),
  MutationRecord: chain(MutationRecord),
  Node: chain(Node),
  ShadowRoot: chain(ShadowRoot),

  // this is some test env shenanigan
  get CSS2Properties() {
    return chained.CSSStyleDeclaration;
  }
};

const upgrade = (value, hint) => {
  if (hint !== "Element" && hint in chained)
    return chained[hint](value);

  if (isArray(value))
    return setPrototypeOf(value, Array.prototype);

  const brand = getBrand(value);
  if (brand in chained)
    return chained[brand](value);

  if (brand in env)
    return setPrototypeOf(value, env[brand].prototype);

  if ("nodeType" in value) {
    switch (call(nodeType, value)) {
      case 1:
        if (!(hint in chained))
          throw new Error("unknown hint " + hint);
        return chained[hint](value);
      case 2:
        return chained.Attr(value);
      case 3:
        return chained.Node(value);
      case 9:
        return chained.Document(value);
    }
  }

  throw new Error("unknown brand " + brand);
};

/* eslint valid-jsdoc: 0 */
/** @type {<T>(t:T)=>t} Any value that can be upgraded or wrapped */
export default isExtensionContext ?
  value => (value === window || value === globalThis ? env : value) :
  transformer((value, hint = "Element") => {
    if (value === window || value === globalThis)
      return env;

    switch (typeof value) {
      case "object":
        return value && upgrade(value, hint);

      case "string":
        return new String(value);

      case "number":
        return new Number(value);

      default:
        throw new Error("unsupported value");
    }
  });
