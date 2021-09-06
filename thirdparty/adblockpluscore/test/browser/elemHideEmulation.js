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
/* global chai */

"use strict";

const {ElemHideEmulation, setTestMode,
       getTestInfo} = require("../../lib/content/elemHideEmulation");
const {timeout} = require("./_utils");

const {assert} = chai;

describe("Element hiding emulation", function()
{
  const REFRESH_INTERVAL = 200;

  let testDocument = null;

  beforeEach(function elemHidingBeforeEach()
  {
    setTestMode();

    let iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    testDocument = iframe.contentDocument;
  });

  afterEach(function elemHidingAfterEach()
  {
    let iframe = testDocument.defaultView.frameElement;
    iframe.parentNode.removeChild(iframe);
    testDocument = null;
  });

  function unexpectedError(error)
  {
    console.error(error);
    assert.ok(false, "Unexpected error: " + error);
  }

  function expectHidden(element, id)
  {
    let withId = "";
    if (typeof id != "undefined")
      withId = ` with ID '${id}'`;

    assert.equal(
      window.getComputedStyle(element).display, "none",
      `The element${withId}'s display property should be set to 'none'`);
  }

  function expectVisible(element, id)
  {
    let withId = "";
    if (typeof id != "undefined")
      withId = ` with ID '${id}'`;

    assert.notEqual(
      window.getComputedStyle(element).display, "none",
      `The element${withId}'s display property should not be set to 'none'`);
  }

  function expectProcessed(element, id = null)
  {
    let withId = "";
    if (id)
      withId = ` with ID '${id}'`;

    assert.ok(
      getTestInfo().lastProcessedElements.has(element),
      `The element${withId} should have been processed`);
  }

  function expectNotProcessed(element, id = null)
  {
    let withId = "";
    if (id)
      withId = ` with ID '${id}'`;

    assert.ok(
      !getTestInfo().lastProcessedElements.has(element),
      `The element${withId} should not have been processed`);
  }

  function findUniqueId()
  {
    let id = "elemHideEmulationTest-" + Math.floor(Math.random() * 10000);
    if (!testDocument.getElementById(id))
      return id;
    return findUniqueId();
  }

  function insertStyleRule(rule)
  {
    let styleElement;
    let styleElements = testDocument.head.getElementsByTagName("style");
    if (styleElements.length)
    {
      styleElement = styleElements[0];
    }
    else
    {
      styleElement = testDocument.createElement("style");
      testDocument.head.appendChild(styleElement);
    }
    styleElement.sheet.insertRule(rule, styleElement.sheet.cssRules.length);
  }

  function createElement(parent, type = "div", id = findUniqueId(),
                         innerText = null)
  {
    let element = testDocument.createElement(type);
    element.id = id;
    if (!parent)
      testDocument.body.appendChild(element);
    else
      parent.appendChild(element);
    if (innerText)
      element.innerText = innerText;
    return element;
  }

  // Insert a <div> with a unique id and a CSS rule
  // for the the selector matching the id.
  function createElementWithStyle(styleBlock, parent)
  {
    let element = createElement(parent);
    insertStyleRule("#" + element.id + " " + styleBlock);
    return element;
  }

  // Create a new ElemHideEmulation instance with @selectors.
  async function applyElemHideEmulation(selectors)
  {
    await Promise.resolve();

    try
    {
      let elemHideEmulation = new ElemHideEmulation(
        elems =>
        {
          // Firefox will send mutation notifications even if the
          // property is set to the same value.
          for (let elem of elems)
          {
            if (elem.style.display != "none")
              elem.style.display = "none";
          }
        }
      );

      elemHideEmulation.document = testDocument;
      elemHideEmulation.minInvocationInterval = REFRESH_INTERVAL / 2;
      elemHideEmulation.apply(selectors.map(
        selector => ({selector, text: selector})
      ));

      return elemHideEmulation;
    }
    catch (error)
    {
      unexpectedError(error);
    }
  }

  it("Verbatim property selector: regular", async function()
  {
    let toHide = createElementWithStyle("{background-color: #000}");
    let selectors = [":-abp-properties(background-color: rgb(0, 0, 0))"];

    if (await applyElemHideEmulation(selectors))
      expectHidden(toHide);
  });

  it("Verbatim property selector: with prefix", async function()
  {
    let parent = createElementWithStyle("{background-color: #000}");
    let toHide = createElementWithStyle("{background-color: #000}", parent);

    let selectors = ["div > :-abp-properties(background-color: rgb(0, 0, 0))"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(parent);
      expectHidden(toHide);
    }
  });

  it("Verbatim property selector: with prefix no match", async function()
  {
    let parent = createElementWithStyle("{background-color: #000}");
    let toHide = createElementWithStyle("{background-color: #fff}", parent);

    let selectors = ["div > :-abp-properties(background-color: rgb(0, 0, 0))"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(parent);
      expectVisible(toHide);
    }
  });

  it("Verbatim property selector: with suffix", async function()
  {
    let parent = createElementWithStyle("{background-color: #000}");
    let toHide = createElementWithStyle("{background-color: #000}", parent);

    let selectors = [":-abp-properties(background-color: rgb(0, 0, 0)) > div"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(parent);
      expectHidden(toHide);
    }
  });

  it("Verbatim property selector: with prefix and suffix", async function()
  {
    let parent = createElementWithStyle("{background-color: #000}");
    let middle = createElementWithStyle("{background-color: #000}", parent);
    let toHide = createElementWithStyle("{background-color: #000}", middle);

    let selectors = ["div > :-abp-properties(background-color: rgb(0, 0, 0)) > div"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(parent);
      expectVisible(middle);
      expectHidden(toHide);
    }
  });

  // Add the style. Then add the element for that style.
  // This should retrigger the filtering and hide it.
  it("Property pseudo selector add style and elemment", async function()
  {
    let styleElement;
    let toHide;

    let selectors = [":-abp-properties(background-color: rgb(0, 0, 0))"];

    if (await applyElemHideEmulation(selectors))
    {
      styleElement = testDocument.createElement("style");
      testDocument.head.appendChild(styleElement);
      styleElement.sheet.insertRule("#toHide {background-color: #000}");
      await timeout(REFRESH_INTERVAL);

      toHide = createElement();
      toHide.id = "toHide";
      expectVisible(toHide);
      await timeout(REFRESH_INTERVAL);

      expectHidden(toHide);
    }
  });

  it("Property selector: with wildcard", async function()
  {
    let toHide = createElementWithStyle("{background-color: #000}");
    let selectors = [":-abp-properties(*color: rgb(0, 0, 0))"];

    if (await applyElemHideEmulation(selectors))
      expectHidden(toHide);
  });

  it("Property selector: with regular expression", async function()
  {
    let toHide = createElementWithStyle("{background-color: #000}");
    let selectors = [":-abp-properties(/.*color: rgb\\(0, 0, 0\\)/)"];

    if (await applyElemHideEmulation(selectors))
      expectHidden(toHide);
  });

  it("Dynamically changed property", async function()
  {
    let toHide = createElementWithStyle("{}");
    let selectors = [":-abp-properties(background-color: rgb(0, 0, 0))"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(toHide);
      insertStyleRule("#" + toHide.id + " {background-color: #000}");

      await timeout(0);

      // Re-evaluation will only happen after a delay
      expectVisible(toHide);
      await timeout(REFRESH_INTERVAL);

      expectHidden(toHide);
    }
  });

  it("Pseudo-class: with property before selector", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{background-color: #000}", parent);

    let selectors = ["div:-abp-properties(content: \"publicite\")"];

    insertStyleRule(`#${child.id}::before {content: "publicite"}`);

    if (await applyElemHideEmulation(selectors))
    {
      expectHidden(child);
      expectVisible(parent);
    }
  });

  function compareExpectations(elems, expectations)
  {
    for (let elem in expectations)
    {
      if (elems[elem])
      {
        if (expectations[elem])
          expectVisible(elems[elem], elem);
        else
          expectHidden(elems[elem], elem);
      }
    }
  }

  it("Pseudo-class: has selector: simple", async function()
  {
    let toHide = createElementWithStyle("{}");

    if (await applyElemHideEmulation(["div:-abp-has(div)"]))
      expectVisible(toHide);
  });

  it("Pseudo-class: has selector: with prefix", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{}", parent);

    if (await applyElemHideEmulation(["div:-abp-has(div)"]))
    {
      expectHidden(parent);
      expectVisible(child);
    }
  });

  it("Pseudo-class: has selector: with suffix", async function()
  {
    let parent = createElementWithStyle("{}");
    let middle = createElementWithStyle("{}", parent);
    let child = createElementWithStyle("{}", middle);

    if (await applyElemHideEmulation(["div:-abp-has(div) > div"]))
    {
      expectVisible(parent);
      expectHidden(middle);
      expectHidden(child);
    }
  });

  it("Pseudo-class: has selector: with suffix sibling", async function()
  {
    let parent = createElementWithStyle("{}");
    let middle = createElementWithStyle("{}", parent);
    let toHide = createElementWithStyle("{}");

    if (await applyElemHideEmulation(["div:-abp-has(div) + div"]))
    {
      expectVisible(parent);
      expectVisible(middle);
      expectHidden(toHide);
    }
  });

  it("Pseudo-class: has Selector: with suffix sibling child", async function()
  {
    //  <div>
    //    <div></div>
    //    <div>
    //      <div>to hide</div>
    //    </div>
    //  </div>
    let parent = createElementWithStyle("{}");
    let middle = createElementWithStyle("{}", parent);
    let sibling = createElementWithStyle("{}");
    let toHide = createElementWithStyle("{}", sibling);

    if (await applyElemHideEmulation(["div:-abp-has(div) + div > div"]))
    {
      expectVisible(parent);
      expectVisible(middle);
      expectVisible(sibling);
      expectHidden(toHide);
    }
  });

  async function runTestPseudoClassHasSelectorWithHasAndWithSuffixSibling(selector, expectations)
  {
    testDocument.body.innerHTML = `<div id="parent">
        <div id="middle">
          <div id="middle1"><div id="inside" class="inside"></div></div>
        </div>
        <div id="sibling">
          <div id="tohide"><span>to hide</span></div>
        </div>
        <div id="sibling2">
          <div id="sibling21"><div id="sibling211" class="inside"></div></div>
        </div>
      </div>`;
    let elems = {
      parent: testDocument.getElementById("parent"),
      middle: testDocument.getElementById("middle"),
      inside: testDocument.getElementById("inside"),
      sibling: testDocument.getElementById("sibling"),
      sibling2: testDocument.getElementById("sibling2"),
      toHide: testDocument.getElementById("tohide")
    };

    insertStyleRule(".inside {}");

    if (await applyElemHideEmulation([selector]))
      compareExpectations(elems, expectations);
  }

  it("Pseudo-class: has selector: with has and with suffix sibling (1)", function()
  {
    let expectations = {
      parent: true,
      middile: true,
      inside: true,
      sibling: true,
      sibling2: true,
      toHide: false
    };
    return runTestPseudoClassHasSelectorWithHasAndWithSuffixSibling(
      "div:-abp-has(:-abp-has(div.inside)) + div > div", expectations);
  });

  it("Pseudo-class: has selector: with has and with suffix sibling (2)", function()
  {
    let expectations = {
      parent: true,
      middile: true,
      inside: true,
      sibling: true,
      sibling2: true,
      toHide: false
    };
    return runTestPseudoClassHasSelectorWithHasAndWithSuffixSibling(
      "div:-abp-has(:-abp-has(> div.inside)) + div > div", expectations);
  });

  it("Pseudo-class: has selector: with has and with suffix sibling (3)", function()
  {
    let expectations = {
      parent: true,
      middile: true,
      inside: true,
      sibling: true,
      sibling2: true,
      toHide: false
    };
    return runTestPseudoClassHasSelectorWithHasAndWithSuffixSibling(
      "div:-abp-has(> div:-abp-has(div.inside)) + div > div", expectations);
  });

  it("Pseudo-class: has selector: with suffix sibling no-op", function()
  {
    let expectations = {
      parent: true,
      middile: true,
      inside: true,
      sibling: true,
      sibling2: true,
      toHide: true
    };
    return runTestPseudoClassHasSelectorWithHasAndWithSuffixSibling(
      "div:-abp-has(> body div.inside) + div > div", expectations);
  });

  it("Pseudo-class: has selector: with suffix sibling contains", function()
  {
    let expectations = {
      parent: true,
      middile: true,
      inside: true,
      sibling: true,
      sibling2: true,
      toHide: true
    };
    return runTestPseudoClassHasSelectorWithHasAndWithSuffixSibling(
      "div:-abp-has(> span:-abp-contains(Advertisment))", expectations);
  });

  async function runTestQualifier(selector)
  {
    testDocument.body.innerHTML = `
      <style>
      span::before {
        content: "any";
      }
      </style>
      <div id="toHide">
        <a>
          <p>
            <span></span>
          </p>
        </a>
      </div>`;

    if (await applyElemHideEmulation([selector]))
      expectHidden(testDocument.getElementById("toHide"));
  }

  // See issue https://issues.adblockplus.org/ticket/7428
  it("Pseudo-class: property selector qualifiers: combinator", function()
  {
    return runTestQualifier(
      "div:-abp-has(> a p > :-abp-properties(content: \"any\"))"
    );
  });

  // See issue https://issues.adblockplus.org/ticket/7359
  it("Pseudo-class: property selector qualifiers: nested combinator", function()
  {
    return runTestQualifier(
      "div:-abp-has(> a p:-abp-has(> :-abp-properties(content: \"any\")))"
    );
  });

  // See issue https://issues.adblockplus.org/ticket/7400
  it("Pseudo-class: property selector qualifiers: identical", function()
  {
    return runTestQualifier(
      "div:-abp-has(span:-abp-properties(content: \"any\"))"
    );
  });

  // See issue https://issues.adblockplus.org/ticket/7400
  it("Pseudo-class: property selector qualifiers: nested identical", function()
  {
    return runTestQualifier(
      "div:-abp-has(p:-abp-has(span:-abp-properties(content: \"any\")))"
    );
  });

  async function runTestPseudoClassContains(selector, expectations)
  {
    testDocument.body.innerHTML = `<div id="parent">
        <div id="middle">
          <div id="middle1"><div id="inside" class="inside"></div></div>
        </div>
        <div id="sibling">
          <div id="tohide">to hide \ud83d\ude42!</div>
        </div>
        <div id="sibling2">
          <div id="sibling21">
            <div id="sibling211" class="inside">Ad*</div>
          </div>
        </div>
      </div>`;
    let elems = {
      parent: testDocument.getElementById("parent"),
      middle: testDocument.getElementById("middle"),
      inside: testDocument.getElementById("inside"),
      sibling: testDocument.getElementById("sibling"),
      sibling2: testDocument.getElementById("sibling2"),
      toHide: testDocument.getElementById("tohide")
    };

    if (await applyElemHideEmulation([selector]))
      compareExpectations(elems, expectations);
  }

  it("Pseudo-class: contains selector: text", function()
  {
    let expectations = {
      parent: true,
      middle: true,
      inside: true,
      sibling: false,
      sibling2: true,
      toHide: true
    };
    return runTestPseudoClassContains(
      "#parent div:-abp-contains(to hide)", expectations);
  });

  it("Pseudo-class: contains selector: regexp", function()
  {
    let expectations = {
      parent: true,
      middle: true,
      inside: true,
      sibling: false,
      sibling2: true,
      toHide: true
    };
    return runTestPseudoClassContains(
      "#parent div:-abp-contains(/to\\shide/)", expectations);
  });

  it("Pseudo-class: contains selector: regexp i flag", function()
  {
    let expectations = {
      parent: true,
      middle: true,
      inside: true,
      sibling: false,
      sibling2: true,
      toHide: true
    };
    return runTestPseudoClassContains(
      "#parent div:-abp-contains(/to\\sHide/i)", expectations);
  });

  it("Pseudo-class: contains selector: regexp u flag", function()
  {
    let expectations = {
      parent: true,
      middle: true,
      inside: true,
      sibling: false,
      sibling2: true,
      toHide: true
    };
    return runTestPseudoClassContains(
      "#parent div:-abp-contains(/to\\shide\\s.!/u)", expectations);
  });

  it("Pseudo-class: contains selector: wildcard no match", function()
  {
    let expectations = {
      parent: true,
      middle: true,
      inside: true,
      sibling: true,
      sibling2: true,
      toHide: true
    };
    // this filter shouldn't match anything as "*" has no meaning.
    return runTestPseudoClassContains(
      "#parent div:-abp-contains(to *hide)", expectations);
  });

  it("Pseudo-class: contains selector: wildcard match", function()
  {
    let expectations = {
      parent: true,
      middle: true,
      inside: true,
      sibling: true,
      sibling2: false,
      toHide: true
    };
    return runTestPseudoClassContains(
      "#parent div:-abp-contains(Ad*)", expectations);
  });

  it("Pseudo-class: has selector with prop selector: already present", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{background-color: #000}", parent);

    let selectors = ["div:-abp-has(:-abp-properties(background-color: rgb(0, 0, 0)))"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(child);
      expectHidden(parent);
    }
  });

  it("Pseudo-class: has selector with prop selector: dynamically added", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{}", parent);

    let selectors = ["div:-abp-has(:-abp-properties(background-color: rgb(0, 0, 0)))"];

    insertStyleRule("body #" + parent.id + " > div { background-color: #000}");

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(child);
      expectHidden(parent);
    }
  });

  it("DOM updates: style", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{}", parent);

    let selectors = ["div:-abp-has(:-abp-properties(background-color: rgb(0, 0, 0)))"];

    if (await applyElemHideEmulation(selectors))
    {
      expectVisible(child);
      expectVisible(parent);

      insertStyleRule("body #" + parent.id + " > div { background-color: #000}");
      await timeout(0);

      expectVisible(child);
      expectVisible(parent);
      await timeout(REFRESH_INTERVAL);

      expectVisible(child);
      expectHidden(parent);
    }
  });

  it("DOM updates: content", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{}", parent);

    if (await applyElemHideEmulation(["div > div:-abp-contains(hide me)"]))
    {
      expectVisible(parent);
      expectVisible(child);

      child.textContent = "hide me";
      await timeout(0);

      expectVisible(parent);
      expectVisible(child);
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectHidden(child);
    }
  });

  it("DOM updates: new element", async function()
  {
    let parent = createElementWithStyle("{}");
    let child = createElementWithStyle("{ background-color: #000}", parent);
    let sibling;
    let child2;

    let selectors = ["div:-abp-has(:-abp-properties(background-color: rgb(0, 0, 0)))"];

    if (await applyElemHideEmulation(selectors))
    {
      expectHidden(parent);
      expectVisible(child);

      sibling = createElementWithStyle("{}");
      await timeout(0);

      expectHidden(parent);
      expectVisible(child);
      expectVisible(sibling);

      await timeout(REFRESH_INTERVAL);

      expectHidden(parent);
      expectVisible(child);
      expectVisible(sibling);

      child2 = createElementWithStyle("{ background-color: #000}",
                                      sibling);
      await timeout(0);

      expectVisible(child2);
      await timeout(REFRESH_INTERVAL);

      expectHidden(parent);
      expectVisible(child);
      expectHidden(sibling);
      expectVisible(child2);
    }
  });

  it("Pseudo-class properties on stylesheet load", async function()
  {
    let parent = createElement();
    let child = createElement(parent);

    let selectors = [
      "div:-abp-properties(background-color: rgb(0, 0, 0))",
      "div:-abp-contains(hide me)",
      "div:-abp-has(> div.hideMe)"
    ];

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectVisible(child);

      // Load a style sheet that targets the parent element. This should run
      // only the "div:-abp-properties(background-color: rgb(0, 0, 0))" pattern.
      insertStyleRule("#" + parent.id + " {background-color: #000}");

      await timeout(REFRESH_INTERVAL);

      expectHidden(parent);
      expectVisible(child);
    }
  });

  it("On DOM mutation: plain attributes", async function()
  {
    let parent = createElement();
    let child = createElement(parent);

    let selectors = [
      "div:-abp-properties(background-color: rgb(0, 0, 0))",
      "div[data-hide-me]",
      "div:-abp-contains(hide me)",
      "div:-abp-has(> div.hideMe)"
    ];

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectVisible(child);

      // Set the "data-hide-me" attribute on the child element.
      //
      // Note: Since the "div[data-hide-me]" pattern has already been processed
      // and the selector added to the document's style sheet, this will in fact
      // not do anything at our end, but the browser will just match the
      // selector and hide the element.
      child.setAttribute("data-hide-me", "");

      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectHidden(child);
    }
  });

  it("On DOM mutation: pseudo-class contains", async function()
  {
    let parent = createElement();
    let child = createElement(parent);

    let selectors = [
      "div:-abp-properties(background-color: rgb(0, 0, 0))",
      "div[data-hide-me]",
      "div:-abp-contains(hide me)",
      "div:-abp-has(> div.hideMe)"
    ];

    child.innerText = "do nothing";

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectVisible(child);

      // Set the child element's text to "hide me". This should run only the
      // "div:-abp-contains(hide me)" pattern.
      //
      // Note: We need to set Node.innerText here in order to trigger the
      // "characterData" DOM mutation on Chromium. If we set Node.textContent
      // instead, it triggers the "childList" DOM mutation instead.
      child.innerText = "hide me";

      await timeout(REFRESH_INTERVAL);

      expectHidden(parent);
      expectVisible(child);
    }
  });

  it("On DOM mutation: pseudo-class has (1)", async function()
  {
    let parent = createElement();
    let child = null;

    let selectors = [
      "div:-abp-properties(background-color: rgb(0, 0, 0))",
      "div[data-hide-me]",
      "div:-abp-contains(hide me)",
      "div:-abp-has(> div)"
    ];

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);

      // Add the child element. This should run all the DOM-dependent patterns
      // ("div:-abp-contains(hide me)" and "div:-abp-has(> div)").
      child = createElement(parent);

      await timeout(REFRESH_INTERVAL);

      expectHidden(parent);
      expectVisible(child);
    }
  });

  it("On DOM mutation: pseudo-class has (2)", async function()
  {
    let parent = createElement();
    let child = createElement(parent);

    let selectors = [
      "div:-abp-properties(background-color: rgb(0, 0, 0))",
      "div[data-hide-me]",
      "div:-abp-contains(hide me)",
      "div:-abp-has(> div.hideMe)"
    ];

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectVisible(child);

      // Set the child element's class to "hideMe". This should run only the
      // "div:-abp-has(> div.hideMe)" pattern.
      child.className = "hideMe";

      await timeout(REFRESH_INTERVAL);

      expectHidden(parent);
      expectVisible(child);
    }
  });

  it("On DOM mutation: pseudo-class has with pseudo-class contains", async function()
  {
    let parent = createElement();
    let child = createElement(parent);

    let selectors = [
      "div:-abp-properties(background-color: rgb(0, 0, 0))",
      "div[data-hide-me]",
      "div:-abp-contains(hide me)",
      "div:-abp-has(> div:-abp-contains(hide me))"
    ];

    child.innerText = "do nothing";

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      expectVisible(parent);
      expectVisible(child);

      // Set the child element's text to "hide me". This should run only the
      // "div:-abp-contains(hide me)" and
      // "div:-abp-has(> div:-abp-contains(hide me))" patterns.
      child.innerText = "hide me";

      await timeout(REFRESH_INTERVAL);

      // Note: Even though it runs both the :-abp-contains() patterns, it only
      // hides the parent element because of revision d7d51d29aa34.
      expectHidden(parent);
      expectVisible(child);
    }
  });

  it("Only relevant elements are processed", async function()
  {
    // <body>
    //   <div id="n1">
    //     <p id="n1_1"></p>
    //     <p id="n1_2"></p>
    //     <p id="n1_4">Hello</p>
    //   </div>
    //   <div id="n2">
    //     <p id="n2_1"></p>
    //     <p id="n2_2"></p>
    //     <p id="n2_4">Hello</p>
    //   </div>
    //   <div id="n3">
    //     <p id="n3_1"></p>
    //     <p id="n3_2"></p>
    //     <p id="n3_4">Hello</p>
    //   </div>
    //   <div id="n4">
    //     <p id="n4_1"></p>
    //     <p id="n4_2"></p>
    //     <p id="n4_4">Hello</p>
    //   </div>
    // </body>
    for (let i of [1, 2, 3, 4])
    {
      let n = createElement(null, "div", `n${i}`);
      for (let [j, text] of [[1], [2], [4, "Hello"]])
        createElement(n, "p", `n${i}_${j}`, text);
    }

    let selectors = [
      "p:-abp-contains(Hello)",
      "div:-abp-contains(Try me!)",
      "div:-abp-has(p:-abp-contains(This is good))"
    ];

    if (await applyElemHideEmulation(selectors))
    {
      await timeout(REFRESH_INTERVAL);

      // This is only a sanity check to make sure everything else is working
      // before we do the actual test.
      for (let i of [1, 2, 3, 4])
      {
        for (let j of [1, 2, 4])
        {
          let id = `n${i}_${j}`;
          if (j == 4)
            expectHidden(testDocument.getElementById(id), id);
          else
            expectVisible(testDocument.getElementById(id), id);
        }
      }

      // All <div> and <p> elements should be processed initially.
      for (let element of [...testDocument.getElementsByTagName("div"),
                           ...testDocument.getElementsByTagName("p")])
        expectProcessed(element, element.id);

      // Modify the text in <p id="n4_1">
      testDocument.getElementById("n4_1").innerText = "Try me!";

      await timeout(REFRESH_INTERVAL);

      // When an element's text is modified, only the element or one of its
      // ancestors matching any selector is processed for :-abp-has() and
      // :-abp-contains()
      for (let element of [...testDocument.getElementsByTagName("div"),
                           ...testDocument.getElementsByTagName("p")])
      {
        if (element.id == "n4" || element.id == "n4_1")
          expectProcessed(element, element.id);
        else
          expectNotProcessed(element, element.id);
      }

      // Create a new <p id="n2_3"> element with no text.
      createElement(testDocument.getElementById("n2"), "p", "n2_3");

      await timeout(REFRESH_INTERVAL);

      // When a new element is added, only the element or one of its ancestors
      // matching any selector is processed for :-abp-has() and :-abp-contains()
      for (let element of [...testDocument.getElementsByTagName("div"),
                           ...testDocument.getElementsByTagName("p")])
      {
        if (element.id == "n2" || element.id == "n2_3")
          expectProcessed(element, element.id);
        else
          expectNotProcessed(element, element.id);
      }
    }
  });
});
