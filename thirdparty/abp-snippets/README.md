Adblock Plus core
=================

This repository contains the generic Adblock Plus code that's shared between
platforms. This repository is __not designed to be used directly__, but instead
to serve as a dependency for `adblockpluschrome` and `libadblockplus`.

Running the unit tests
----------------------

### Requirements

In order to run the unit test suite you need
[Node.js 10.12.0 or higher](https://nodejs.org/). Once Node.js is installed
please run `npm install` in the repository directory in order to install the
required dependencies.

### Running all tests

`npm test` will run all tests in the `test` directory of the repository.

### Running specific tests

You can specify specific test files or directories on the
command line, e.g.:
`npm test test/synchronizer.js test/browser/elemHideEmulation.js`.

### Running the browser tests in a real browser

The tests under `test/browser` require a browser environment. `npm test` will
run these in a headless browser, with each module being loaded in a new frame.

The default is to run in both Chromium (using the remote interface)
and Firefox. You can select which runners to use by setting the
BROWSER_TEST_RUNNERS environment, the default is
"chromium_remote,firefox". Possible values (separated by a ',') are:

- "chromium_remote": Chromium 60 (using the remote interface)
- "chromium": Chrome 63 (using WebDriver)
- "firefox": Firefox 57 (using WebDriver)

You can not set a specific version of the browser at runtime.

Browser tests run headless by default (except on Windows). If you want
to disable headless mode on the WebDriver controlled tests, set the
BROWSER_TEST_HEADLESS environment to 0.

Linting
-------

You can lint the code using [ESLint](http://eslint.org) by running
`npm run lint`.

Help center documentation generation
------------------------------------

You can automatically generate the snippets documentation available at
[help.eyeo.com](https://help.eyeo.com/en/adblockplus/) by running
`npm run helpcenter`. This will generate a file `snippet-filters-tutorial.md`
that contains the markdown text as suited for our CMS. The content is the same
as the tutorial included as part of the JSDoc.

Node.js module
-----------------------------

There is now __experimental__ support for this repository to be used directly
as a Node.js module.

```
npm install git+https://gitlab.com/eyeo/adblockplus/adblockpluscore
```

```javascript
let {contentTypes, filterEngine} = require("adblockpluscore");

async function main()
{
  await filterEngine.initialize(
    [
      "/annoying-ad.$image",
      "||example.com/social-widget.html^"
    ]
  );

  let resource = {
    url: "https://ad-server.example.net/annoying-ad.png",
    documentURL: "https://news.example.com/world.html"
  };

  let filter = filterEngine.match(resource.url, contentTypes.IMAGE,
                                  new URL(resource.documentURL).hostname);
  console.log(filter); // prints "/annoying-ad.$image"
}

if (require.main == module)
  main();
```
