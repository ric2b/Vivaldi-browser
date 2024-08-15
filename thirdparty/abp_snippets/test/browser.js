/**
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

const patches = new Set();

/**
 * Setup a browser like environment.
 */
export const setup = () => {
  // cleanup previously patched names, if any
  teardown();

  // add fake DOM like classes
  for (const Class of [
    "Attr",
    "CanvasRenderingContext2D",
    "CSSStyleDeclaration",
    "Document",
    "DOMParser",
    "Element",
    "HTMLCanvasElement",
    "HTMLElement",
    "HTMLImageElement",
    "HTMLScriptElement",
    "MouseEvent",
    "MutationRecord",
    "XPathEvaluator",
    "XPathExpression",
    "XPathResult",
    "EventTarget",
    "ShadowRoot"
  ]) {
    if (!(Class in globalThis)) {
      patches.add(Class);
      globalThis[Class] = Object;
    }
  }

  // plus Node and window
  if (!("Node" in globalThis)) {
    patches.add("Node");
    globalThis.Node = class Node { get nodeType() {} };
  }

  if (!("window" in globalThis)) {
    patches.add("window");
    patches.add("onerror");
    patches.add("document");

    globalThis.window = globalThis;

    Object.defineProperties(globalThis, {
      document: {
        configurable: true,
        value: {
          get cookie() {
            return {};
          },
          // it simulates a listener added and it executes it right away
          // @see https://gitlab.com/eyeo/snippets/-/blob/5a878bcc2782b8ce73b76d17fd84f2d1b0eb73e8/bundle/isolated-first.js#L44-57
          addEventListener(_, callback) {
            const {Blob} = globalThis;
            const {createObjectURL, revokeObjectURL} = URL;

            // pass the identity through
            globalThis.Blob = Object;
            URL.createObjectURL = Object;

            // and evaluate the Array's script
            URL.revokeObjectURL = ([script]) => {
              Function(script)();
            };

            // basic HTML element and document shim
            this.createElement = _ => ({});
            this.documentElement = {appendChild: Object};

            // invoke the "readystatechange" listener
            callback();

            // cleanup the env
            globalThis.Blob = Blob;
            URL.createObjectURL = createObjectURL;
            URL.revokeObjectURL = revokeObjectURL;
          }
        }
      },
      onerror: {
        configurable: true,
        get() {
          return null;
        }
      }
    });
  }
};

/**
 * Cleanup the browser like environment.
 */
export const teardown = () => {
  for (const name of patches)
    delete globalThis[name];

  patches.clear();
};
