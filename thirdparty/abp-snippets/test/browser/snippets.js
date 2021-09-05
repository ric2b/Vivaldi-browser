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
/* eslint no-new-func: "off" */

"use strict";

const libraryText = require("raw-loader!../../lib/content/snippets.js");
const {compileScript} = require("../../lib/snippets.js");
const {timeout} = require("./_utils");

const {assert} = chai;

describe("Snippets", function()
{
  before(function()
  {
    // We need this stub for the injector.
    window.browser = {
      runtime: {
        getURL: () => ""
      }
    };
  });

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

  async function runSnippetScript(script)
  {
    new Function(compileScript(script, [libraryText]))();

    // For snippets that run in the context of the document via a <script>
    // element (i.e. snippets that use makeInjector()), we need to wait for
    // execution to be complete.
    await timeout(100);
  }

  function testProperty(property, result = true, errorName = "ReferenceError")
  {
    let path = property.split(".");

    let exceptionCaught = false;
    let value = 1;

    try
    {
      let obj = window;
      while (path.length > 1)
        obj = obj[path.shift()];
      value = obj[path.shift()];
    }
    catch (e)
    {
      assert.equal(e.name, errorName);
      exceptionCaught = true;
    }

    assert.equal(
      exceptionCaught,
      result,
      `The property "${property}" ${result ? "should" : "shouldn't"} trigger an exception.`
    );
    assert.equal(
      value,
      result ? 1 : undefined,
      `The value for "${property}" ${result ? "shouldn't" : "should"} have been read.`
    );
  }

  it("abort-property-read", async function()
  {
    window.abpTest = "fortytwo";
    await runSnippetScript("abort-on-property-read abpTest");
    testProperty("abpTest");

    window.abpTest2 = {prop1: "fortytwo"};
    await runSnippetScript("abort-on-property-read abpTest2.prop1");
    testProperty("abpTest2.prop1");

    // Test that we try to catch a property that doesn't exist yet.
    await runSnippetScript("abort-on-property-read abpTest3.prop1");
    window.abpTest3 = {prop1: "fortytwo"};
    testProperty("abpTest3.prop1");

    // Test that other properties don't trigger.
    testProperty("abpTest3.prop2", false);

    // Test overwriting the object with another object.
    window.abpTest4 = {prop3: {}};
    await runSnippetScript("abort-on-property-read abpTest4.prop3.foo");
    testProperty("abpTest4.prop3.foo");
    window.abpTest4.prop3 = {};
    testProperty("abpTest4.prop3.foo");

    // Test if we start with a non-object.
    window.abpTest5 = 42;
    await runSnippetScript("abort-on-property-read abpTest5.prop4.bar");

    testProperty("abpTest5.prop4.bar", true, "TypeError");

    window.abpTest5 = {prop4: 42};
    testProperty("abpTest5.prop4.bar", false);
    window.abpTest5 = {prop4: {}};
    testProperty("abpTest5.prop4.bar");

    // Check that it works on properties that are functions.
    // https://issues.adblockplus.org/ticket/7419

    // Existing function (from the API).
    await runSnippetScript("abort-on-property-read Object.keys");
    testProperty("Object.keys");

    // Function properties.
    window.abpTest6 = function() {};
    window.abpTest6.prop1 = function() {};
    await runSnippetScript("abort-on-property-read abpTest6.prop1");
    testProperty("abpTest6.prop1");

    // Function properties, with sub-property set afterwards.
    window.abpTest7 = function() {};
    await runSnippetScript("abort-on-property-read abpTest7.prop1");
    window.abpTest7.prop1 = function() {};
    testProperty("abpTest7.prop1");

    // Function properties, with base property as function set afterwards.
    await runSnippetScript("abort-on-property-read abpTest8.prop1");
    window.abpTest8 = function() {};
    window.abpTest8.prop1 = function() {};
    testProperty("abpTest8.prop1");

    // Arrow function properties.
    window.abpTest9 = () => {};
    await runSnippetScript("abort-on-property-read abpTest9");
    testProperty("abpTest9");

    // Class function properties.
    window.abpTest10 = class {};
    await runSnippetScript("abort-on-property-read abpTest10");
    testProperty("abpTest10");

    // Class function properties with prototype function properties.
    window.abpTest11 = class {};
    window.abpTest11.prototype.prop1 = function() {};
    await runSnippetScript("abort-on-property-read abpTest11.prototype.prop1");
    testProperty("abpTest11.prototype.prop1");

    // Class function properties with prototype function properties, with
    // prototype property set afterwards.
    window.abpTest12 = class {};
    await runSnippetScript("abort-on-property-read abpTest12.prototype.prop1");
    window.abpTest12.prototype.prop1 = function() {};
    testProperty("abpTest12.prototype.prop1");
  });

  it("abort-on-propery-write", async function()
  {
    try
    {
      await runSnippetScript("abort-on-property-write document.createElement");

      let element = document.createElement("script");
      assert.ok(!!element);
    }
    catch (error)
    {
      assert.fail(error);
    }
  });

  it("abort-on-iframe-property-read", async function()
  {
    await runSnippetScript("abort-on-iframe-property-read document.createElement");
    let iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    assert.throws(() =>
    {
      window[0].document.createElement("script");
    });
  });

  it("abort-on-iframe-property-write", async function()
  {
    await runSnippetScript("abort-on-iframe-property-write adblock");
    let iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    assert.throws(() =>
    {
      window[0].adblock = true;
    });
  });

  it("abort-curent-inline-script", async function()
  {
    function injectInlineScript(doc, script)
    {
      let scriptElement = doc.createElement("script");
      scriptElement.type = "application/javascript";
      scriptElement.async = false;
      scriptElement.textContent = script;
      doc.body.appendChild(scriptElement);
    }

    await runSnippetScript(
      "abort-current-inline-script document.write atob"
    );
    await runSnippetScript(
      "abort-current-inline-script document.write btoa"
    );

    document.body.innerHTML = "<p id=\"result1\"></p><p id=\"message1\"></p><p id=\"result2\"></p><p id=\"message2\"></p>";

    let script = `
      try
      {
        let element = document.getElementById("result1");
        document.write("<p>atob: " + atob("dGhpcyBpcyBhIGJ1Zw==") + "</p>");
        element.textContent = atob("dGhpcyBpcyBhIGJ1Zw==");
      }
      catch (e)
      {
        let msg = document.getElementById("message1");
        msg.textContent = e.name;
      }`;

    injectInlineScript(document, script);

    let element = document.getElementById("result1");
    assert.ok(element, "Element 'result1' was not found");

    let msg = document.getElementById("message1");
    assert.ok(msg, "Element 'message1' was not found");

    if (element && msg)
    {
      assert.equal(element.textContent, "", "Result element should be empty");
      assert.equal(msg.textContent, "ReferenceError",
                   "There should have been an error");
    }

    script = `
      try
      {
        let element = document.getElementById("result2");
        document.write("<p>btoa: " + btoa("this is a bug") + "</p>");
        element.textContent = btoa("this is a bug");
      }
      catch (e)
      {
        let msg = document.getElementById("message2");
        msg.textContent = e.name;
      }`;

    injectInlineScript(document, script);

    element = document.getElementById("result2");
    assert.ok(element, "Element 'result2' was not found");

    msg = document.getElementById("message2");
    assert.ok(msg, "Element 'message2' was not found");

    if (element && msg)
    {
      assert.equal(element.textContent, "", "Result element should be empty");
      assert.equal(msg.textContent, "ReferenceError",
                   "There should have been an error");
    }
  });

  it("json-prune", async function()
  {
    // ensure the JSON object is the window one, not one
    // the testing environment is providing
    let {JSON} = window;
    await runSnippetScript("json-prune toBeDeleted");
    let testProp = {
      toBeDeleted: "delete me",
      a: "a"
    };
    let result = JSON.parse(JSON.stringify(testProp));
    delete testProp.toBeDeleted;
    assert.equal(JSON.stringify(testProp), JSON.stringify(result));

    await runSnippetScript("json-prune toBeDeleted2 a");
    let testProp2 = {
      toBeDeleted2: "delete me",
      a: "a"
    };
    let testProp3 = {
      toBeDeleted2: "don't delete me",
      b: "b"
    };
    let result2 = JSON.parse(JSON.stringify(testProp2));
    let result3 = JSON.parse(JSON.stringify(testProp3));
    delete testProp2.toBeDeleted2;
    assert.equal(JSON.stringify(testProp2), JSON.stringify(result2));
    assert.equal(JSON.stringify(testProp3), JSON.stringify(result3));
  });

  it("hide-if-contains-visible-text", async function()
  {
    document.body.innerHTML = `
      <style type="text/css">
        body {
          margin: 0;
          padding: 0;
        }
        .transparent {
          opacity: 0;
          position: absolute;
          display: block;
        }
        .zerosize {
          font-size: 0;
        }
        div {
          display: block;
        }
        .a {
          display: inline-block;
          white-space: pre-wrap;
        }
        .disp_none {
          display: none;
        }
        .vis_hid {
          visibility: hidden;
        }
        .vis_collapse {
          visibility: collapse;
        }
        .same_colour {
          color: rgb(255,255,255);
          background-color: rgb(255,255,255);
        }
        .transparent {
          color: transparent;
        }
        #label {
          overflow-wrap: break-word;
        }
        #pseudo .static::before {
          content: "sp";
        }
        #pseudo .static::after {
          content: "ky";
        }
        #pseudo .attr::before {
          content: attr(data-before);
        }
        #pseudo .attr::after {
          content: attr(data-after) "ky";
        }
      </style>
      <div id="parent">
        <div id="middle">
          <div id="middle1"><div id="inside" class="inside"></div></div>
        </div>
        <div id="sibling">
          <div id="tohide">to hide \ud83d\ude42!</div>
        </div>
        <div id="sibling2">
          <div id="sibling21"><div id="sibling211" class="inside">Ad*</div></div>
        </div>
        <div id="label">
          <div id="content"><div class="a transparent">Sp</div><div class="a">Sp</div><div class="a zerosize">S</div><div class="a transparent">on</div><div class="a">on</div><div class="a zerosize">S</div></div>
        </div>
        <div id="label2">
          <div class="a vis_hid">Visibility: hidden</div><div class="a">S</div><div class="a vis_collapse">Visibility: collapse</div><div class="a">p</div><div class="a disp_none">Display: none</div><div class="a">o</div><div class="a same_colour">Same colour</div><div class="a transparent">Transparent</div><div class="a">n</div>
        </div>
        <article id="article">
          <div style="display: none"><a href="foo"><div>Spon</div></a>Visit us</div>
        </article>
        <article id="article2">
          <div><a href="foo"><div>Spon</div></a>By this</div>
        </article>
        <article id="article3">
          <div><a href="foo"><div>by Writer</div></a> about the Sponsorship.</div>
        </article>
      </div>
      <div id="pseudo">
        <div class="static">oo</div>
        <div>ok</div>
        <div class="attr" data-before="sp">oo</div>
      </div>`;

    await runSnippetScript(
      "hide-if-contains-visible-text Spon '#parent > div'"
    );

    let element = document.getElementById("label");
    expectHidden(element, "label");
    element = document.getElementById("label2");
    expectHidden(element, "label2");

    element = document.getElementById("article");
    expectVisible(element, "article");
    element = document.getElementById("article2");
    expectVisible(element, "article2");

    await runSnippetScript(
      "hide-if-contains-visible-text Spon '#parent > article' '#parent > article a'"
    );

    element = document.getElementById("article");
    expectVisible(element, "article");
    element = document.getElementById("article2");
    expectHidden(element, "article2");
    element = document.getElementById("article3");
    expectVisible(element, "article3");

    await runSnippetScript(
      "hide-if-contains-visible-text spooky '#pseudo > div'"
    );

    element = document.getElementById("pseudo");
    expectHidden(element.querySelector(".static"), "#pseudo .static");
    expectHidden(element.querySelector(".attr"), "#pseudo .attr");
    expectVisible(element.children[1], "#pseudo div");
  });

  it("hide-if-contains-image-hash", async function()
  {
    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9ba0WqDnZQcchQnSyIijhqFYpQIdQKrTqYXPoFTRqSFBdHwbXg4Mdi1cHFWVcHV0EQ/ABxcXVSdJES/5cUWsR4cNyPd/ced+8Af73MVLNjHFA1y0gl4kImuyqEXhFEJ0IYRK/ETH1OFJPwHF/38PH1LsazvM/9OXqUnMkAn0A8y3TDIt4gnt60dM77xBFWlBTic+Ixgy5I/Mh12eU3zgWH/TwzYqRT88QRYqHQxnIbs6KhEk8RRxVVo3x/xmWF8xZntVxlzXvyF4Zz2soy12kOI4FFLEGEABlVlFCGhRitGikmUrQf9/APOX6RXDK5SmDkWEAFKiTHD/4Hv7s185MTblI4DgRfbPtjBAjtAo2abX8f23bjBAg8A1day1+pAzOfpNdaWvQI6NsGLq5bmrwHXO4AA0+6ZEiOFKDpz+eB9zP6pizQfwt0r7m9Nfdx+gCkqavkDXBwCIwWKHvd491d7b39e6bZ3w/1+HJ1S9l56wAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFBgcZNA50WAgAAAAMSURBVAjXY/j//z8ABf4C/tzMWecAAAAASUVORK5CYII=\" /><img id=\"img-2\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bS0tpdbCDiEOG6mRBVMRRq1CECqFWaNXB5NIvaGJIUlwcBdeCgx+LVQcXZ10dXAVB8APExdVJ0UVK/F9SaBHjwXE/3t173L0D/M0aU82eMUDVLCObTgn5wooQekUQYcQQQa/ETH1WFDPwHF/38PH1LsmzvM/9OWJK0WSATyCeYbphEa8TT21aOud94jirSArxOfGoQRckfuS67PIb57LDfp4ZN3LZOeI4sVDuYrmLWcVQiSeJE4qqUb4/77LCeYuzWquz9j35C6NFbXmJ6zSHkMYCFiFCgIw6qqjBQpJWjRQTWdpPefgHHb9ILplcVTByzGMDKiTHD/4Hv7s1SxPjblI0BQRfbPtjGAjtAq2GbX8f23brBAg8A1dax7/RBKY/SW90tMQR0LcNXFx3NHkPuNwBBp50yZAcKUDTXyoB72f0TQWg/xaIrLq9tfdx+gDkqKvMDXBwCIyUKXvN493h7t7+PdPu7wfkk3Juqb5bhwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFCA0KNmzdilMAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAADElEQVQI12NgYGAAAAAEAAEnNCcKAAAAAElFTkSuQmCC\" />";

    await runSnippetScript("hide-if-contains-image-hash 8000000000000000");

    // Since the images are blocked via an async event handler (onload) we need
    // to give the snippet an opportunity to execute
    await timeout(100);

    expectHidden(document.getElementById("img-1"), "img-1");
    expectVisible(document.getElementById("img-2"), "img-2");

    document.body.innerHTML = "<div id=\"div-1\"><img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9ba0WqDnZQcchQnSyIijhqFYpQIdQKrTqYXPoFTRqSFBdHwbXg4Mdi1cHFWVcHV0EQ/ABxcXVSdJES/5cUWsR4cNyPd/ced+8Af73MVLNjHFA1y0gl4kImuyqEXhFEJ0IYRK/ETH1OFJPwHF/38PH1LsazvM/9OXqUnMkAn0A8y3TDIt4gnt60dM77xBFWlBTic+Ixgy5I/Mh12eU3zgWH/TwzYqRT88QRYqHQxnIbs6KhEk8RRxVVo3x/xmWF8xZntVxlzXvyF4Zz2soy12kOI4FFLEGEABlVlFCGhRitGikmUrQf9/APOX6RXDK5SmDkWEAFKiTHD/4Hv7s185MTblI4DgRfbPtjBAjtAo2abX8f23bjBAg8A1day1+pAzOfpNdaWvQI6NsGLq5bmrwHXO4AA0+6ZEiOFKDpz+eB9zP6pizQfwt0r7m9Nfdx+gCkqavkDXBwCIwWKHvd491d7b39e6bZ3w/1+HJ1S9l56wAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFBgcZNA50WAgAAAAMSURBVAjXY/j//z8ABf4C/tzMWecAAAAASUVORK5CYII=\" /></div>";

    await runSnippetScript("hide-if-contains-image-hash 8000000000000000 #div-1");

    await timeout(100);

    expectHidden(document.getElementById("div-1"), "div-1");
    expectVisible(document.getElementById("img-1"), "img-1");

    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9ba0WqDnZQcchQnSyIijhqFYpQIdQKrTqYXPoFTRqSFBdHwbXg4Mdi1cHFWVcHV0EQ/ABxcXVSdJES/5cUWsR4cNyPd/ced+8Af73MVLNjHFA1y0gl4kImuyqEXhFEJ0IYRK/ETH1OFJPwHF/38PH1LsazvM/9OXqUnMkAn0A8y3TDIt4gnt60dM77xBFWlBTic+Ixgy5I/Mh12eU3zgWH/TwzYqRT88QRYqHQxnIbs6KhEk8RRxVVo3x/xmWF8xZntVxlzXvyF4Zz2soy12kOI4FFLEGEABlVlFCGhRitGikmUrQf9/APOX6RXDK5SmDkWEAFKiTHD/4Hv7s185MTblI4DgRfbPtjBAjtAo2abX8f23bjBAg8A1day1+pAzOfpNdaWvQI6NsGLq5bmrwHXO4AA0+6ZEiOFKDpz+eB9zP6pizQfwt0r7m9Nfdx+gCkqavkDXBwCIwWKHvd491d7b39e6bZ3w/1+HJ1S9l56wAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFBgcZNA50WAgAAAAMSURBVAjXY/j//z8ABf4C/tzMWecAAAAASUVORK5CYII=\" /><img id=\"img-2\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bS0tpdbCDiEOG6mRBVMRRq1CECqFWaNXB5NIvaGJIUlwcBdeCgx+LVQcXZ10dXAVB8APExdVJ0UVK/F9SaBHjwXE/3t173L0D/M0aU82eMUDVLCObTgn5wooQekUQYcQQQa/ETH1WFDPwHF/38PH1LsmzvM/9OWJK0WSATyCeYbphEa8TT21aOud94jirSArxOfGoQRckfuS67PIb57LDfp4ZN3LZOeI4sVDuYrmLWcVQiSeJE4qqUb4/77LCeYuzWquz9j35C6NFbXmJ6zSHkMYCFiFCgIw6qqjBQpJWjRQTWdpPefgHHb9ILplcVTByzGMDKiTHD/4Hv7s1SxPjblI0BQRfbPtjGAjtAq2GbX8f23brBAg8A1dax7/RBKY/SW90tMQR0LcNXFx3NHkPuNwBBp50yZAcKUDTXyoB72f0TQWg/xaIrLq9tfdx+gDkqKvMDXBwCIyUKXvN493h7t7+PdPu7wfkk3Juqb5bhwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFCA0KNmzdilMAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAADElEQVQI12NgYGAAAAAEAAEnNCcKAAAAAElFTkSuQmCC\" />";

    await runSnippetScript("hide-if-contains-image-hash 0800000000000000 '' 1");

    await timeout(100);

    expectVisible(document.getElementById("img-1"), "img-1");
    expectHidden(document.getElementById("img-2"), "img-2");

    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9ba0WqDnZQcchQnSyIijhqFYpQIdQKrTqYXPoFTRqSFBdHwbXg4Mdi1cHFWVcHV0EQ/ABxcXVSdJES/5cUWsR4cNyPd/ced+8Af73MVLNjHFA1y0gl4kImuyqEXhFEJ0IYRK/ETH1OFJPwHF/38PH1LsazvM/9OXqUnMkAn0A8y3TDIt4gnt60dM77xBFWlBTic+Ixgy5I/Mh12eU3zgWH/TwzYqRT88QRYqHQxnIbs6KhEk8RRxVVo3x/xmWF8xZntVxlzXvyF4Zz2soy12kOI4FFLEGEABlVlFCGhRitGikmUrQf9/APOX6RXDK5SmDkWEAFKiTHD/4Hv7s185MTblI4DgRfbPtjBAjtAo2abX8f23bjBAg8A1day1+pAzOfpNdaWvQI6NsGLq5bmrwHXO4AA0+6ZEiOFKDpz+eB9zP6pizQfwt0r7m9Nfdx+gCkqavkDXBwCIwWKHvd491d7b39e6bZ3w/1+HJ1S9l56wAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFBgcZNA50WAgAAAAMSURBVAjXY/j//z8ABf4C/tzMWecAAAAASUVORK5CYII=\" />";

    await runSnippetScript(
      "hide-if-contains-image-hash 8000000000000000000000000000000000000000000000000000000000000000 '' 0 16");

    await timeout(100);

    expectHidden(document.getElementById("img-1"), "img-1");

    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bS0tpdbCDiEOG6mRBVMRRq1CECqFWaNXB5NIvaGJIUlwcBdeCgx+LVQcXZ10dXAVB8APExdVJ0UVK/F9SaBHjwXE/3t173L0D/M0aU82eMUDVLCObTgn5wooQekUQYcQQQa/ETH1WFDPwHF/38PH1LsmzvM/9OWJK0WSATyCeYbphEa8TT21aOud94jirSArxOfGoQRckfuS67PIb57LDfp4ZN3LZOeI4sVDuYrmLWcVQiSeJE4qqUb4/77LCeYuzWquz9j35C6NFbXmJ6zSHkMYCFiFCgIw6qqjBQpJWjRQTWdpPefgHHb9ILplcVTByzGMDKiTHD/4Hv7s1SxPjblI0BQRfbPtjGAjtAq2GbX8f23brBAg8A1dax7/RBKY/SW90tMQR0LcNXFx3NHkPuNwBBp50yZAcKUDTXyoB72f0TQWg/xaIrLq9tfdx+gDkqKvMDXBwCIyUKXvN493h7t7+PdPu7wfkk3Juqb5bhwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFCQkxNu/aqtIAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAAD0lEQVQI12P4//8/AwMDAA74Av7BVpVFAAAAAElFTkSuQmCC\" />";

    await runSnippetScript("hide-if-contains-image-hash 8000000000000000 '' 0 8 0x0x1x1");

    await timeout(100);

    expectHidden(document.getElementById("img-1"), "img-1");

    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bS0tpdbCDiEOG6mRBVMRRq1CECqFWaNXB5NIvaGJIUlwcBdeCgx+LVQcXZ10dXAVB8APExdVJ0UVK/F9SaBHjwXE/3t173L0D/M0aU82eMUDVLCObTgn5wooQekUQYcQQQa/ETH1WFDPwHF/38PH1LsmzvM/9OWJK0WSATyCeYbphEa8TT21aOud94jirSArxOfGoQRckfuS67PIb57LDfp4ZN3LZOeI4sVDuYrmLWcVQiSeJE4qqUb4/77LCeYuzWquz9j35C6NFbXmJ6zSHkMYCFiFCgIw6qqjBQpJWjRQTWdpPefgHHb9ILplcVTByzGMDKiTHD/4Hv7s1SxPjblI0BQRfbPtjGAjtAq2GbX8f23brBAg8A1dax7/RBKY/SW90tMQR0LcNXFx3NHkPuNwBBp50yZAcKUDTXyoB72f0TQWg/xaIrLq9tfdx+gDkqKvMDXBwCIyUKXvN493h7t7+PdPu7wfkk3Juqb5bhwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFCQkxNu/aqtIAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAAD0lEQVQI12P4//8/AwMDAA74Av7BVpVFAAAAAElFTkSuQmCC\" />";

    await runSnippetScript(
      "hide-if-contains-image-hash 0000000000000000 '' 0 8 1x0x1x1");

    await timeout(100);

    expectHidden(document.getElementById("img-1"), "img-1");

    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bS0tpdbCDiEOG6mRBVMRRq1CECqFWaNXB5NIvaGJIUlwcBdeCgx+LVQcXZ10dXAVB8APExdVJ0UVK/F9SaBHjwXE/3t173L0D/M0aU82eMUDVLCObTgn5wooQekUQYcQQQa/ETH1WFDPwHF/38PH1LsmzvM/9OWJK0WSATyCeYbphEa8TT21aOud94jirSArxOfGoQRckfuS67PIb57LDfp4ZN3LZOeI4sVDuYrmLWcVQiSeJE4qqUb4/77LCeYuzWquz9j35C6NFbXmJ6zSHkMYCFiFCgIw6qqjBQpJWjRQTWdpPefgHHb9ILplcVTByzGMDKiTHD/4Hv7s1SxPjblI0BQRfbPtjGAjtAq2GbX8f23brBAg8A1dax7/RBKY/SW90tMQR0LcNXFx3NHkPuNwBBp50yZAcKUDTXyoB72f0TQWg/xaIrLq9tfdx+gDkqKvMDXBwCIyUKXvN493h7t7+PdPu7wfkk3Juqb5bhwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFCQkxNu/aqtIAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAAD0lEQVQI12P4//8/AwMDAA74Av7BVpVFAAAAAElFTkSuQmCC\" />";

    await runSnippetScript(
      "hide-if-contains-image-hash 0000000000000000 '' 0 8 1x0x1x1");

    await timeout(100);

    expectHidden(document.getElementById("img-1"), "img-1");

    document.body.innerHTML = "<img id=\"img-1\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bS0tpdbCDiEOG6mRBVMRRq1CECqFWaNXB5NIvaGJIUlwcBdeCgx+LVQcXZ10dXAVB8APExdVJ0UVK/F9SaBHjwXE/3t173L0D/M0aU82eMUDVLCObTgn5wooQekUQYcQQQa/ETH1WFDPwHF/38PH1LsmzvM/9OWJK0WSATyCeYbphEa8TT21aOud94jirSArxOfGoQRckfuS67PIb57LDfp4ZN3LZOeI4sVDuYrmLWcVQiSeJE4qqUb4/77LCeYuzWquz9j35C6NFbXmJ6zSHkMYCFiFCgIw6qqjBQpJWjRQTWdpPefgHHb9ILplcVTByzGMDKiTHD/4Hv7s1SxPjblI0BQRfbPtjGAjtAq2GbX8f23brBAg8A1dax7/RBKY/SW90tMQR0LcNXFx3NHkPuNwBBp50yZAcKUDTXyoB72f0TQWg/xaIrLq9tfdx+gDkqKvMDXBwCIyUKXvN493h7t7+PdPu7wfkk3Juqb5bhwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MFCQkxNu/aqtIAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAAD0lEQVQI12P4//8/AwMDAA74Av7BVpVFAAAAAElFTkSuQmCC\" />";

    await runSnippetScript(
      "hide-if-contains-image-hash 8000000000000000 '' 0 8 1x1x-1x-1");

    await timeout(100);

    expectHidden(document.getElementById("img-1"), "img-1");
  });

  it("does not leak snippets to the global scope", async function()
  {
    assert.ok(typeof window.log === "undefined", "The window has no log function");
    await runSnippetScript("trace OK");
    assert.ok(typeof window.log === "undefined", "The window was not polluted");
  });

  it("debug flag", async function()
  {
    function injectInlineScript(doc, script)
    {
      let scriptElement = doc.createElement("script");
      scriptElement.type = "application/javascript";
      scriptElement.async = false;
      scriptElement.textContent = script;
      doc.body.appendChild(scriptElement);
    }

    let logArgs = [];

    // Type 1 test: no debug
    let {log} = console;
    console.log = (...args) =>
    {
      console.log = log;
      logArgs = args.join(",");
    };
    runSnippetScript("log 1 2");
    assert.strictEqual(logArgs, "1,2", "type 1 debug flag should be false");

    // Type 2 test: no debug
    injectInlineScript(document, `(() =>
    {
      let {log} = console;
      console.log = (...args) =>
      {
        console.log = log;
        document.log = args.join(",");
      }
    })();`);
    await runSnippetScript("trace 1 2");
    assert.strictEqual(document.log, "1,2",
                       "type 2 debug flag should be false");


    // Type 1 test: debug flag enabled
    console.log = (...args) =>
    {
      console.log = log;
      logArgs = args.join(",");
    };
    await runSnippetScript("debug; log 1 2");
    assert.strictEqual(logArgs, "%c DEBUG,font-weight: bold,1,2",
                       "type 1 debug flag should be true");

    // Type 2 test: debug flag enabled
    injectInlineScript(document, `(() =>
    {
      let {log} = console;
      console.log = (...args) =>
      {
        console.log = log;
        document.log = args.join(",");
      }
    })();`);
    await runSnippetScript("debug; trace 1 2");
    assert.strictEqual(document.log, "%c DEBUG,font-weight: bold,1,2",
                       "type 2 debug flag should be true");

    delete document.log;
  });

  it("hide-if-matches-xpath", async function()
  {
    document.body.innerHTML = '<div id="xpath-target"></div>';
    let target = document.getElementById("xpath-target");
    expectVisible(target);
    await runSnippetScript("hide-if-matches-xpath //*[@id=\"xpath-target\"]");
    expectHidden(target);
  });

  it("hide-if-matches-xpath lazily", async function()
  {
    await runSnippetScript("hide-if-matches-xpath //*[@id=\"xpath-lazily\"]");
    document.body.innerHTML = '<div id="xpath-lazily"></div>';
    let target = document.getElementById("xpath-lazily");
    expectVisible(target);
    await timeout(100);
    expectHidden(target);
  });

  it("hide-if-matches-xpath text", async function()
  {
    document.body.innerHTML = '<div id="xpath-text">out<p>in</p></div>';
    let target = document.getElementById("xpath-text").firstChild;
    assert.ok(target.textContent === "out");
    await runSnippetScript("hide-if-matches-xpath //*[@id=\"xpath-text\"]/child::text()[contains(.,\"out\")]");
    assert.ok(target.textContent === "");
  });

  it("hide-if-labelled-by", async function()
  {
    document.body.innerHTML = `
      <div id="hilb-label">Sponsored</div>
      <div id="hilb-target">
        <div aria-labelledby="hilb-label">Content</div>
      </div>
    `;
    let target = document.getElementById("hilb-target");
    expectVisible(target);
    await runSnippetScript("hide-if-labelled-by 'Sponsored' '#hilb-target [aria-labelledby]' '#hilb-target'");
    expectHidden(target);
  });

  it("hide-if-labelled-by lazily", async function()
  {
    await runSnippetScript("hide-if-labelled-by 'Sponsored' '#hilb-target-lazy [aria-labelledby]' '#hilb-target-lazy'");
    document.body.innerHTML = `
      <div id="hilb-label-lazy">Sponsored</div>
      <div id="hilb-target">
        <div aria-labelledby="hilb-label-lazy">Content</div>
      </div>
    `;
    let target = document.getElementById("hilb-target");
    expectVisible(target);
    await timeout(100);
    expectHidden(target);
  });

  it("hide-if-labelled-by inline", async function()
  {
    document.body.innerHTML = `
      <div id="hilb-target-inline">
        <div aria-labelledby="hilb-label-nope" aria-label="Sponsored">Content</div>
      </div>
    `;
    let target = document.getElementById("hilb-target-inline");
    expectVisible(target);
    await runSnippetScript("hide-if-labelled-by 'Sponsored' '#hilb-target-inline [aria-labelledby]' '#hilb-target-inline'");
    expectHidden(target);
  });

  describe("freeze-element", function()
  {
    let {freeze} = window.Object;
    let {WeakMap} = window;
    let snippetWM = new WeakMap();
    let {has} = WeakMap.prototype;
    let domTree = `
    <div id="page">
      <div id="header">
        <div id="navbar">
          <span class="nav-item">home</span>
          <span class="nav-item">contact</span>
        </div>
        <span id="logo">logo</span>
        <span id="title">title</span>
      </div>
      <div id="content">
        <div id="article">
          <h2>title</h2>
          <p>article body</p>
        </div>
        <div id="comments">
          <div class="comment">
            <img class="profile" src="img1.jpg"/>
            <p>comment_1</p>
          </div>
          <div class="comment">
            <img class="profile" src="img2.jpg"/>
            <p>comment_2</p>
          </div>
        </div>
      </div>
    </div>`;
    let targetNode = null;
    let targetNodeChild = null;
    let targetNodeGrandChild = null;
    let targetNodeParent = null;
    let targetNodeSibling = null;
    let domContent = null;
    let propertiesList = [
      "appendChild",
      "append",
      "prepend",
      "insertBefore",
      "replaceChild",
      "replaceWith",
      "after",
      "before",
      "insertAdjacentElement",
      "insertAdjacentHTML",
      "insertAdjacentText",
      "innerHTML",
      "outerHTML",
      "textContent",
      "innerText",
      "nodeValue"
    ];

    async function resetAndRun(script)
    {
      document.body.innerHTML = domTree;
      domContent = document.body.innerHTML;
      attachToWeakMap();
      window.Object.freeze = window.Object;
      await runSnippetScript(script);
      await timeout(100);
      window.Object.freeze = freeze;
      targetNode = document.querySelector("#header");
      targetNodeChild = document.querySelector("#navbar");
      targetNodeGrandChild = document.querySelectorAll(".nav-item")[0];
      targetNodeParent = document.querySelector("#page");
      targetNodeSibling = document.querySelector("#content");
    }

    function attachToWeakMap()
    {
      WeakMap.prototype.has = function(x)
      {
        if (x !== document)
          return has.call(this, x);
        this.set = function(key, value)
        {
          snippetWM.set(key, value);
        };
        this.get = function(key)
        {
          return snippetWM.get(key);
        };
        this.has = function(key)
        {
          return snippetWM.has(key);
        };
        WeakMap.prototype.has = has;
        if (!snippetWM.has(x))
        {
          snippetWM.set(x, true);
          return false;
        }
        return true;
      };
    }

    let badNode = document.createElement("div");
    badNode.className = "bad-node";
    let getBadNode = () => badNode.cloneNode();

    let goodNode = document.createElement("div");
    goodNode.className = "good-node";
    let queryGoodNodes = () => document.querySelectorAll(".good-node");
    let getGoodNode = () => goodNode.cloneNode();

    let exceptionNode = document.createElement("div");
    exceptionNode.className = "exception-node";
    let queryExceptionNodes = () => document.querySelectorAll(".exception-node");
    let getExceptionNode = () => exceptionNode.cloneNode();

    function protect(cb, abort, shouldThrow)
    {
      if (!abort)
        cb();
      else
        shouldThrow ? assert.throws(cb) : assert.doesNotThrow(cb);
    }

    async function testProps(title, subtree, abort, exceptions, script)
    {
      describe(title, function()
      {
        beforeEach(async function()
        {
          await resetAndRun(script);
        });
        for (let property of propertiesList)
        {
          it(property, function()
          {
            testProp(property, subtree, abort, exceptions);
          });
        }
      });
    }

    let targetMessage = "targeted node should be frozen";
    let exceptionMessage = "exception nodes should be allowed";
    let nonTargetMessage = "non-targeted node should not be frozen";

    function testProp(property, subtree, abort, exceptions)
    {
      switch (property)
      {
        case "appendChild":
        case "append":
        case "prepend":
          return testAppendChild();
        case "insertBefore":
          return testInsertBefore();
        case "replaceChild":
          return testReplaceChild();
        case "replaceWith":
          return testReplaceWith();
        case "after":
        case "before":
          return testBeforeAndAfter();
        case "insertAdjacentElement":
        case "insertAdjacentHTML":
        case "insertAdjacentText":
          return testInsertAdjacent();
        case "outerHTML":
        case "innerHTML":
          return testInnerHTML();
        case "textContent":
        case "innerText":
          return testTextContent();
        case "nodeValue":
          return testNodeValue();
      }

      function testAppendChild()
      {
        let exceptionsCount;
        let notTargetedCount;
        // targeted node should be frozen
        protect(() => Element.prototype[property].call(targetNode, getBadNode()), abort, true);
        protect(() => targetNode[property](getBadNode()), abort, true);
        if (subtree)
        {
          protect(() => targetNodeChild[property](getBadNode()), abort, true);
          protect(() => targetNodeGrandChild[property](getBadNode()), abort, true);
        }
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          exceptionsCount = 2;
          protect(() => Element.prototype[property].call(targetNode, getExceptionNode()), abort, false);
          protect(() => targetNode[property](getExceptionNode()), abort, false);
          if (subtree)
          {
            exceptionsCount = 4;
            protect(() => targetNodeChild[property](getExceptionNode()), abort, false);
            protect(() => targetNodeGrandChild[property](getExceptionNode()), abort, false);
          }
          assert.strictEqual(queryExceptionNodes().length, exceptionsCount, exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        notTargetedCount = 3;
        protect(() => Element.prototype[property].call(targetNodeParent, getGoodNode()), abort, false);
        protect(() => targetNodeParent[property](getGoodNode()), abort, false);
        protect(() => targetNodeSibling[property](getGoodNode()), abort, false);
        if (!subtree)
        {
          notTargetedCount = 4;
          protect(() => targetNodeChild[property](getGoodNode()), abort, false);
        }
        assert.strictEqual(queryGoodNodes().length, notTargetedCount, nonTargetMessage);
      }

      function testInsertBefore()
      {
        let exceptionsCount;
        let notTargetedCount;
        // targeted node should be frozen
        protect(() => Node.prototype.insertBefore.call(targetNode, getBadNode(), targetNodeChild), abort, true);
        protect(() => targetNode.insertBefore(getBadNode(), targetNodeChild), abort, true);
        if (subtree)
        {
          protect(() => targetNodeChild.insertBefore(getBadNode(), null), abort, true);
          protect(() => targetNodeGrandChild.insertBefore(getBadNode(), null), abort, true);
        }
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          exceptionsCount = 2;
          protect(() => Node.prototype.insertBefore.call(targetNode, getExceptionNode(), targetNodeChild), abort, false);
          protect(() => targetNode.insertBefore(getExceptionNode(), targetNodeChild), abort, false);
          if (subtree)
          {
            exceptionsCount = 4;
            protect(() => targetNodeChild.insertBefore(getExceptionNode(), null), abort, false);
            protect(() => targetNodeGrandChild.insertBefore(getExceptionNode(), null), abort, false);
          }
          assert.strictEqual(queryExceptionNodes().length, exceptionsCount, exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        notTargetedCount = 3;
        protect(() => Node.prototype.insertBefore.call(targetNodeParent, getGoodNode(), null), abort, false);
        protect(() => targetNodeParent.insertBefore(getGoodNode(), null), abort, false);
        protect(() => targetNodeSibling.insertBefore(getGoodNode(), null), abort, false);
        if (!subtree)
        {
          notTargetedCount = 4;
          protect(() => targetNodeChild.insertBefore(getGoodNode(), null), abort, false);
        }
        assert.strictEqual(queryGoodNodes().length, notTargetedCount, nonTargetMessage);
      }

      function testReplaceChild()
      {
        let exceptionsCount;
        let notTargetedCount;
        // targeted node should be frozen
        protect(() => Node.prototype.replaceChild.call(targetNode, getBadNode(), targetNodeChild), abort, true);
        protect(() => targetNode.replaceChild(getBadNode(), targetNodeChild), abort, true);
        if (subtree)
          protect(() => targetNodeChild.replaceChild(getBadNode(), targetNodeGrandChild), abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          exceptionsCount = 2;
          protect(() => Node.prototype.replaceChild.call(targetNode, getExceptionNode(), targetNode.children[1]), abort, false);
          protect(() => targetNode.replaceChild(getExceptionNode(), targetNode.children[2]), abort, false);
          if (subtree)
          {
            exceptionsCount = 3;
            protect(() => targetNodeChild.replaceChild(getExceptionNode(), targetNodeGrandChild), abort, false);
          }
          assert.strictEqual(queryExceptionNodes().length, exceptionsCount, exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        notTargetedCount = 2;
        protect(() => Node.prototype.replaceChild.call(targetNodeSibling, getGoodNode(), targetNodeSibling.children[1]), abort, false);
        protect(() => targetNodeSibling.replaceChild(getGoodNode(), targetNodeSibling.children[0]), abort, false);
        if (!subtree)
        {
          notTargetedCount = 3;
          protect(() => targetNodeChild.replaceChild(getGoodNode(), targetNodeGrandChild), abort, false);
        }
        assert.strictEqual(queryGoodNodes().length, notTargetedCount, nonTargetMessage);
      }

      function testReplaceWith()
      {
        // targeted node should be frozen
        protect(() => Element.prototype[property].call(targetNode, getBadNode()), abort, true);
        protect(() => targetNode[property](getBadNode()), abort, true);
        protect(() => targetNodeChild[property](getBadNode()), abort, true);
        if (subtree)
          protect(() => targetNodeGrandChild[property](getBadNode()), abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          if (subtree)
          {
            protect(() => targetNodeGrandChild[property](getExceptionNode()), abort, false);
            assert.strictEqual(queryExceptionNodes().length, 1, exceptionMessage);
          }
          protect(() => Element.prototype[property].call(targetNodeChild, getExceptionNode()), abort, false);
          assert.strictEqual(queryExceptionNodes().length, 1, exceptionMessage);
          protect(() => targetNode[property](getExceptionNode()), abort, false);
          assert.strictEqual(queryExceptionNodes().length, 1, exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        protect(() => Element.prototype[property].call(targetNodeSibling.children[0], getGoodNode()), abort, false);
        assert.strictEqual(queryGoodNodes().length, 1, nonTargetMessage);
        protect(() => targetNodeSibling[property](getGoodNode()), abort, false);
        assert.strictEqual(queryGoodNodes().length, 1, nonTargetMessage);
      }

      function testBeforeAndAfter()
      {
        let exceptionsCount;
        let notTargetedCount;
        // targeted node should be frozen
        protect(() => Element.prototype[property].call(targetNode, getBadNode()), abort, true);
        protect(() => targetNode[property](getBadNode()), abort, true);
        protect(() => targetNodeChild[property](getBadNode()), abort, true);
        if (subtree)
          protect(() => targetNodeGrandChild[property](getBadNode()), abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          exceptionsCount = 2;
          protect(() => Element.prototype[property].call(targetNode, getExceptionNode()), abort, false);
          protect(() => targetNodeChild[property](getExceptionNode()), abort, false);
          if (subtree)
          {
            exceptionsCount = 3;
            protect(() => targetNodeGrandChild[property](getExceptionNode()), abort, false);
          }
          assert.strictEqual(queryExceptionNodes().length, exceptionsCount, exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        notTargetedCount = 2;
        protect(() => Element.prototype[property].call(targetNodeParent, getGoodNode()), abort, false);
        protect(() => targetNodeSibling[property](getGoodNode()), abort, false);
        if (!subtree)
        {
          notTargetedCount = 3;
          protect(() => targetNodeGrandChild[property](getGoodNode()), abort, false);
        }
        assert.strictEqual(queryGoodNodes().length, notTargetedCount, nonTargetMessage);
      }

      function testInsertAdjacent()
      {
        let exceptionsCount;
        let notTargetedCount;
        // targeted node should be frozen
        protect(() => Element.prototype[property].call(targetNode, "afterbegin", getBad()), abort, true);
        protect(() => targetNode[property]("afterbegin", getBad()), abort, true);
        protect(() => targetNode[property]("beforeend", getBad()), abort, true);
        protect(() => targetNodeChild[property]("beforebegin", getBad()), abort, true);
        protect(() => targetNodeChild[property]("afterend", getBad()), abort, true);
        if (subtree)
          protect(() => targetNodeChild[property]("afterbegin", getBad()), abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          exceptionsCount = 5;
          protect(() => Element.prototype[property].call(targetNode, "afterbegin", getException()), abort, false);
          protect(() => targetNode[property]("afterbegin", getException()), abort, false);
          protect(() => targetNode[property]("beforeend", getException()), abort, false);
          protect(() => targetNodeChild[property]("beforebegin", getException()), abort, false);
          protect(() => targetNodeChild[property]("afterend", getException()), abort, false);
          if (subtree)
          {
            exceptionsCount = 6;
            protect(() => targetNodeChild[property]("afterbegin", getException()), abort, false);
          }
          if (property === "insertAdjacentText")
          {
            assert.notStrictEqual(targetNodeParent.textContent.match(/\(exception\)/g), null, exceptionMessage);
            assert.strictEqual(targetNodeParent.textContent.match(/\(exception\)/g).length, exceptionsCount, exceptionMessage);
          }
          else
          { assert.strictEqual(queryExceptionNodes().length, exceptionsCount, exceptionMessage); }
        }

        // non-targeted nodes should not be frozen
        notTargetedCount = 5;
        protect(() => Element.prototype[property].call(targetNode, "beforebegin", getGood()), abort, false);
        protect(() => targetNode[property]("beforebegin", getGood()), abort, false);
        protect(() => targetNode[property]("afterend", getGood()), abort, false);
        protect(() => targetNodeSibling[property]("afterbegin", getGood()), abort, false);
        protect(() => targetNodeParent[property]("afterbegin", getGood()), abort, false);
        if (!subtree)
        {
          notTargetedCount = 6;
          protect(() => targetNodeChild[property]("afterbegin", getGood()), abort, false);
        }
        if (property === "insertAdjacentText")
        {
          assert.notStrictEqual(targetNodeParent.textContent.match(/\(good\)/g), null, nonTargetMessage);
          assert.strictEqual(targetNodeParent.textContent.match(/\(good\)/g).length, notTargetedCount, nonTargetMessage);
        }
        else
        { assert.strictEqual(queryGoodNodes().length, notTargetedCount, nonTargetMessage); }

        function getBad()
        {
          switch (property)
          {
            case "insertAdjacentElement":
              return getBadNode();
            case "insertAdjacentHTML":
              return getBadNode().outerHTML;
            case "insertAdjacentText":
              return "(bad)";
          }
        }

        function getException()
        {
          switch (property)
          {
            case "insertAdjacentElement":
              return getExceptionNode();
            case "insertAdjacentHTML":
              return getExceptionNode().outerHTML;
            case "insertAdjacentText":
              return "(exception)";
          }
        }

        function getGood()
        {
          switch (property)
          {
            case "insertAdjacentElement":
              return getGoodNode();
            case "insertAdjacentHTML":
              return getGoodNode().outerHTML;
            case "insertAdjacentText":
              return "(good)";
          }
        }
      }

      function testInnerHTML()
      {
        let setter = Object.getOwnPropertyDescriptor(Element.prototype, property).set;

        // targeted node should be frozen
        protect(() => setter.call(targetNode, getBadNode().outerHTML), abort, true);
        protect(() => targetNode[property] = getBadNode().outerHTML, abort, true);
        if (subtree)
          protect(() => targetNodeChild[property] = getBadNode().outerHTML, abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          if (subtree)
          {
            protect(() => targetNodeChild[property] = getExceptionNode().outerHTML, abort, false);
            assert.strictEqual(queryExceptionNodes().length, 1, exceptionMessage);
          }
          protect(() => targetNode[property] = getExceptionNode().outerHTML, abort, false);
          assert.strictEqual(queryExceptionNodes().length, 1, exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        protect(() => targetNodeSibling[property] = getGoodNode().outerHTML, abort, false);
        assert.strictEqual(queryGoodNodes().length, 1, nonTargetMessage);
      }

      function testTextContent()
      {
        let setter;
        switch (property)
        {
          case "textContent":
            setter = Object.getOwnPropertyDescriptor(Node.prototype, property).set;
            break;
          case "innerText":
            setter = Object.getOwnPropertyDescriptor(HTMLElement.prototype, property).set;
            break;
          default:
            break;
        }

        // targeted node should be frozen
        protect(() => setter.call(targetNode, "bad"), abort, true);
        protect(() => targetNode[property] = "bad", abort, true);
        if (subtree)
          protect(() => targetNodeChild[property] = "bad", abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          if (subtree)
          {
            protect(() => targetNodeChild[property] = "(exception)", abort, false);
            assert.strictEqual(targetNodeChild[property], "(exception)", exceptionMessage);
          }
          protect(() => targetNode[property] = "(exception)", abort, false);
          assert.strictEqual(targetNode[property], "(exception)", exceptionMessage);
        }

        // non-targeted nodes should not be frozen
        protect(() => targetNodeSibling[property] = "good", abort, false);
        assert.strictEqual(targetNodeSibling[property], "good", nonTargetMessage);
        protect(() => targetNodeParent[property] = "good", abort, false);
        assert.strictEqual(targetNodeSibling[property], "good", nonTargetMessage);
      }

      function testNodeValue()
      {
        let setter = Object.getOwnPropertyDescriptor(Node.prototype, property).set;

        // targeted node should be frozen
        if (targetNode.nodeType === Node.TEXT_NODE)
        {
          protect(() => setter.call(targetNode, "bad"), abort, true);
          protect(() => targetNode.nodeValue = "bad", abort, true);
        }
        let navItem = document.querySelectorAll(".nav-item")[0];
        if (subtree)
          protect(() => navItem.childNodes[0].nodeValue = "bad", abort, true);
        assert.strictEqual(document.body.innerHTML, domContent, targetMessage);

        // exception nodes should be allowed
        if (exceptions)
        {
          protect(() => targetNode.nodeValue = "(exception)", abort, false);
          if (targetNode.nodeType === Node.TEXT_NODE)
            assert.strictEqual(targetNode.nodeValue, "(exception)", exceptionMessage);
          if (subtree)
          {
            protect(() => navItem.childNodes[0].nodeValue = "(exception)", abort, false);
            assert.strictEqual(navItem.childNodes[0].nodeValue, "(exception)", exceptionMessage);
          }
        }

        // non-targeted nodes should not be frozen
        let articleTitle = document.querySelector("#article h2");
        protect(() => articleTitle.childNodes[0].nodeValue = "good", abort, false);
        assert.strictEqual(articleTitle.childNodes[0].nodeValue, "good", nonTargetMessage);
      }
    }

    testProps("basic", false, false, false, "freeze-element #header");
    testProps("subtree", true, false, false, "freeze-element #header subtree");
    testProps("abort", false, true, false, "freeze-element #header abort");
    testProps("exceptions", false, false, true, "freeze-element #header '' .exception-node /(exception)/");
    testProps("subtree+abort", true, true, false, "freeze-element #header subtree+abort");
    testProps("subtree+exceptions", true, false, true, "freeze-element #header subtree .exception-node /(exception)/");
    testProps("abort+exceptions", false, true, true, "freeze-element #header abort .exception-node /(exception)/");
    testProps("subtree+abort+exceptions", true, true, true, "freeze-element #header subtree+abort .exception-node /(exception)/");
  });

  function testPropertyOverride(property, value)
  {
    let result = false;
    let path = property.split(".");
    let actualValue;
    let obj = window;

    while (path.length > 1)
      obj = obj[path.shift()];
    actualValue = obj[path.shift()];

    if (value === "undefined" && typeof actualValue === value)
      result = true;
    else if (value === "false" && actualValue === false)
      result = true;
    else if (value === "true" && actualValue === true)
      result = true;
    else if (value === "null" && actualValue === null)
      result = true;
    else if (value === "noopFunc" && actualValue == `${() => {}}`)
      result = true;
    else if (value === "trueFunc" && actualValue == `${() => true}`)
      result = true;
    else if (value === "falseFunc" && actualValue == `${() => false}`)
      result = true;
    else if (/^\d+$/.test(value) && actualValue == value)
      result = true;
    else if (value === "" && actualValue === "")
      result = true;

    assert.strictEqual(
      result,
      true,
      `The property "${property}" doesn't have the right value: "${value}"`
    );
  }

  it("override-property-read", async function()
  {
    window.overrideTest = "fortytwo";
    await runSnippetScript("override-property-read overrideTest undefined");
    testPropertyOverride("overrideTest", "undefined");

    window.overrideTest2 = {prop1: "fortytwo"};
    await runSnippetScript("override-property-read overrideTest2.prop1 false");
    testPropertyOverride("overrideTest2.prop1", "false");

    // Test that we try to catch a property that doesn't exist yet.
    await runSnippetScript("override-property-read overrideTest3.prop1 true");
    window.overrideTest3 = {prop1: "fortytwo"};
    testPropertyOverride("overrideTest3.prop1", "true");

    // Test overwriting the object with another object.
    window.overrideTest4 = {prop3: {}};
    await runSnippetScript("override-property-read overrideTest4.prop3.foo null");
    testPropertyOverride("overrideTest4.prop3.foo", "null");
    window.overrideTest4.prop3 = {};
    testPropertyOverride("overrideTest4.prop3.foo", "null");

    // Existing function (from the API).
    await runSnippetScript("override-property-read Object.fromEntries noopFunc");
    testPropertyOverride("Object.fromEntries", "noopFunc");

    // Function properties.
    window.overrideTest6 = function() {};
    window.overrideTest6.prop1 = function() {};
    await runSnippetScript("override-property-read overrideTest6.prop1 trueFunc");
    testPropertyOverride("overrideTest6.prop1", "trueFunc");

    // Function properties, with sub-property set afterwards.
    window.overrideTest7 = function() {};
    await runSnippetScript("override-property-read overrideTest7.prop1 falseFunc");
    window.overrideTest7.prop1 = function() {};
    testPropertyOverride("overrideTest7.prop1", "falseFunc");

    // Function properties, with base property as function set afterwards.
    await runSnippetScript("override-property-read overrideTest8.prop1 ''");
    window.overrideTest8 = function() {};
    window.overrideTest8.prop1 = function() {};
    testPropertyOverride("overrideTest8.prop1", "");

    // Arrow function properties.
    window.overrideTest9 = () => {};
    await runSnippetScript("override-property-read overrideTest9 0");
    testPropertyOverride("overrideTest9", "0");

    // Class function properties.
    window.overrideTest10 = class {};
    await runSnippetScript("override-property-read overrideTest10 1");
    testPropertyOverride("overrideTest10", "1");

    // Class function properties with prototype function properties.
    window.overrideTest11 = class {};
    window.overrideTest11.prototype.prop1 = function() {};
    await runSnippetScript("override-property-read overrideTest11.prototype.prop1 2");
    testPropertyOverride("overrideTest11.prototype.prop1", "2");

    // Class function properties with prototype function properties, with
    // prototype property set afterwards.
    window.overrideTest12 = class {};
    await runSnippetScript("override-property-read overrideTest12.prototype.prop1 3");
    window.overrideTest12.prototype.prop1 = function() {};
    testPropertyOverride("overrideTest12.prototype.prop1", "3");
  });
});
