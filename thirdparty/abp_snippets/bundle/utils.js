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

import LICENSE from "../LICENSE.js";

export const withLicense = () => `/*!
${LICENSE.trim().replace(/^/gm, " * ")}
 */
`;

export const withoutLicense = content => content.replace(withLicense(), '');

// Note: these classes leak in the globalThis Node.js context
const browser = [
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
]
.map(
  Class => `window.${Class} = Object;`
)
.join("\n")
.concat(
  "class Node { get nodeType() {} }"
);

// Note: these properties leak in the globalThis Node.js context
Object.defineProperties(globalThis, {
  document: {
    configurable: true,
    value: {
      get cookie() {
        return {};
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

export const getSnippets = content => Function(
  "window", "exports",
  `${browser};\n${content};\nreturn exports`
)(globalThis, {});

