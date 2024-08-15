# @eyeo/snippets

This module goal is to provide a pre-bundled, pre-tested, and pre-optimized version of all *snippets* used in various mobile, or desktop, Ads blockers.


### Snippets in a nutshell

Each snippet is represented as a filter text, defined through `#$#` syntax, followed by the snippet *name* and zero, one, or more arguments separated by a space or, when spaces are part of the argument, included in quotes.

```
! logs 1 2 3 in the console
evilsite.com#$#log 1 2 3
```

Multiple snippets can be executed per single filter by using a semicolon `;` in between calls.

```
! logs 1 2 then 3 4 5 in the console
evilsite.com#$#log 1 2; log 3 4 5
```


#### The two kind of snippets

Snippets can run either as [content script](https://developer.chrome.com/docs/extensions/mv3/content_scripts/#isolated_world), which is a separate environment that replicates the *DOM* but never interferes with the *JS* that is running on the user page (and vice-versa), or as [injected script](https://developer.chrome.com/docs/extensions/mv3/content_scripts/#programmatic) that runs on the page among 3rd party and regular site's *JS* code.

The former kind is called **isolated**, because it runs sandboxed through the Web extension API, while the latter kind is called **main**, or **injected**, because it requires evaluation within the rest of the code that runs in the page, hence it's literally injected into such page.

Some snippet might be just an utility and, as such, be available in both the *main* and the *isolated* worlds. The way this snippet gets executed, and its possible side effects, remain confined within the isolated or main "*world*" it runs into.


## How to use this module

This module currently provides the following exports:

  * `@eyeo/snippets` which includes both `isolated` and `main`, aliased also as `injected`, export names
  * `@eyeo/snippets/isolated` which includes only the `isolated` export
  * `@eyeo/snippets/main`, aliased as `@eyeo/snippets/injected` too, which includes only the `main` export
  * `@eyeo/all` which exports all snippets, except for the Machine Learning one, as single callback for those environments where the *isolated* world is *not supported*, but standard *ESM* is used

Accordingly to the project needs, this module can be used in various ways.


## Machine Learning

The machine learning snippet and files related to it are taken from the @eyeo/mlaf dependency. The machine learning snippet `hide-if-classifies` is included in [webext/ml.mjs](./webext/ml.mjs) (can be imported with `@eyeo/snippets/isolated`) for web extensions and in [isolated-first-all.jst](./dist/isolated-all.jst) for Chromium-based products. Please note that the ML snippet requires a service worker backend to function properly. This is already supported by [eyeo's Web Extension Ad Blocking Toolkit](https://gitlab.com/eyeo/adblockplus/abc/webext-sdk) and [Browser Ad-Filtering Solution](https://gitlab.com/eyeo/adblockplus/chromium-sdk). For web extensions the service worker component of `hide-if-classifies` is exported via the snippets module as well. It can be reached from [./dist/service/mlaf.mjs](./dist/service/mlaf.mjs) or imported with `@eyeo/snippets/mlaf`. For Chromium-based products, the service worker backend component needs to imported directly from the [mlaf repository](https://gitlab.com/eyeo/mlaf) right now.


## How to reach our files directly after releases

Please always check [our releases page](https://gitlab.com/eyeo/snippets/-/releases) and feel free to use URLs that point at our released branches, via `https://gitlab.com/eyeo/snippets/-/raw/RELEASE_VERSION/dist/YOUR_FILE_HERE.js`, where `RELEASE_VERSION` can be `v0.5.0` or others, and `YOUR_FILE_HERE` is:

  * for *Chromium* based products, we have an *ISOLATED* world first artifact, reachable through this module's [isolated-first.jst](./dist/isolated-first.jst) file
  * for *WebKit* based products, we a *MAIN* it all artifact, reachable through this module's [snippets.js](./dist/snippets.js) file

Both `main.js` and `isolated.js` are also there for possible testing purpose, containing the same code other files have.

Their source code is the exact same available in the [webext folder](./webext/) and its source files.

## XPath 3.1 Support

With the release of snippets module version 0.8.0, we introduced a new snippet called `hide-if-matches-xpath3` that allows using XPath 3.1 functions to target ads. As XPath 3.1 is not supported natively by the browsers, the snippet requires injecting a dependency into the isolated context to work properly. This dependency is automatically injected using existing mechanisms for web extensions, if you are using [eyeo's Web Extension Ad Blocking Toolkit](https://gitlab.com/eyeo/adblockplus/abc/webext-sdk). For Chromium based products, the dependency injection logic will be supported by [eyeo Browser Ad-Filtering Solution ](https://gitlab.com/eyeo/adblockplus/chromium-sdk).

As a Chromium-based product, if you are using the `@eyeo/snippets` module as a standalone library you would need to actively opt-in to be able to use the `hide-if-matches-xpath3` snippet. You can keep using the `isolated-first.jst` artifact without any changes to your existing build steps. `isolated-first.jst` does not include `hide-if-matches-xpath3` snippet but it will keep being supported. If you want to include the `hide-if-matches-xpath3` snippet you would need to do two steps. First you would need to use [isolated-first-all.jst](./dist/isolated-first-all.jst) artifact instead. Secondly you would need to implement the dependency injection logic so that the required dependencies are included before the snippet runs. You can do this by injecting the [dependencies.jst](./dist/dependencies.jst) artifact into the page and calling the dependency function: `hideIfMatchesXPath3Dependency()`. The snippet should work as expected after that. 

### Used Third-Party Libraries

* [fontoxpath](https://github.com/FontoXML/fontoxpath) - [License](https://github.com/FontoXML/fontoxpath/blob/master/LICENSE.md)
* [prsc.js](https://github.com/bwrrp/prsc.js) - [License](https://github.com/bwrrp/prsc.js/blob/main/LICENSE)
* [xspattern.js](https://github.com/bwrrp/xspattern.js) - [License](https://github.com/bwrrp/xspattern.js/blob/main/LICENSE)
* [whynot.js](https://github.com/bwrrp/whynot.js) - [License](https://github.com/bwrrp/whynot.js/blob/main/LICENSE)

## Module Structure

The [source folder](./source) contains all the files used to create artifacts in both [dist folder](./dist) and [webext folder](./webext).

The former contains files used by 3rd party, non Web Extension related, projects, while the *webext* folder contains both optimized and source version of the files, so that it is possible to eventually offer a *debugging* mode instead of a *production* one.


### Architectural Overview

This module is used by both Web Extensions and 3rd party partners and, even if the source code is always the same, these are different targets.

This section would like to explain, on a high level, the difference among these folders.

#### Webext

This folder contains all exports defined as *ECMAScript* standard module (aka *ESM*) within the `package.json` file.

Both *main* and *isolated* exports are *pure functions* that can get passed to the Manifest v3 [scripting API](https://developer.chrome.com/docs/extensions/reference/scripting/).

These functions have been augmented like a read-only *Map* via a `.has(snippetName)` and `.get(snipetName)` where:

  * `.has(...)` returns *true* if the snippets is known for that specific world (either *MAIN* or *ISOLATED*)
  * `.get(...)` returns either *null* or a *callback* that is used as *dependency* for that very specific script

*hide-if-graph-matches* is an example of a snippet that returns its whole dependencies wrapped as callback that will be injected in the *ISOLATED* world *before* its related snippet gets excuted in the very same world.

This approach gives us the ability to execute the minimum amount of code whenever such snippet is not needed for that specific page, as dependencies can indeed be quite big (in this case a stripped version of the TensorFlow Library + a binary model).

These callbacks are usually not used, nor needed, by other 3rd party snippets' consumers.

#### Dist

This folder contains files that can be *embedded* directly as part of any *HTML* page and executed right away through surrounding text:

```js
// example of distFileContent source code
// distFileContent = readFileSync("dist/snippets.js")
// distFileContent = readFileSync("dist/isolated-first.jst")
page.evaluate(`(${distFileContent})(...${JSON.stringify([env, ...snippets])})`);
```

In the `snippets.js` case we have all snippets except the *ML* one embedded into a single callback, and all snippets run in the *MAIN* world.

The `isolated-first.jst` file instead executes right away any *ISOLATED* snippet and, if there's anything in the *MAIN* world to execute too, it contains itself some logic to append a script on the user page that runs *MAIN* world snippets right after, still passing along the very same arguments it received through `[env, ...snippets]` serialization.

The `.jst` extension means *JS Template* and it's been chosen because otherwise some toolchain might try to automatically minify such file and, because its content is not referenced anyway as that's just a callback that's not even executed right away, the minifier could decide to entirely drop its content.

We came up with a template extension to avoid minifiers embedded in toolchains (i.e. Android) to actually break our code once injected.

Other files in the *dist* folder offer, like it is for the *webext* one, full source code alternatives or any other possible alternative as just *isolated.js* or *main.js* could be.

**Plase note** these files in the *dist* folder are not exported as module *but* are used through release tags as explained in the previous session.


## About Releases

We try to follow official guidance from [GitLab](https://docs.gitlab.com/ee/user/project/releases/) through the following steps:

  * be sure the internal source code and repository points at the release code we'd like to ship as module
  * create a new branch named after the [semver](https://semver.org/) release version, i.e. `git checkout -b vX.X.X`
  * perform `npm run build.assets ~/gitlab/abp-snippets/source` to import source code files into this repository
  * perform `npm run build` to create all artifacts after a cleanup
  * add all updates that are planned to land as release `vX.X.X`, using **@eyeo/snippets vX.X.X** as commit message
  * ignore `dist`, `webext`, and the `source` folder while reviewing changes, as these will always be differnt in a way or another
  * push the changes via `git push --set-upstream origin vX.X.X` and then create a descriptive release MR following this template:

```md
## Background / User story

A brief description of why we are releasing.

## Dependency changes

  * **Name**: @eyeo/snippets
  * **Version**: vX.X.X
  * **Breaking changes**: none, or a meaningful and detailed description about any breaking change
  * **Other changes**:
    * a description
    * of all the *patches* landing
    * within this release

## New features

A description of any new snippet or feature that is not a patch and it improves the library or filtes ability.
In case of multiple new features, consider splitting these via `### Heading 3` for easy exact links references.

## Deprecations

A list of all deprecations landing with this version.

## Integration notes

A description of any particular change or extra work any module consumer should be aware of.

```

  * once reviewed and approved, create create the tag and bump the version via `npm version [mayor|minor|patch] -m "@eyeo/snippets v%s"`
  * push branch via `git push vX.X.X`
  * push tags via `git push --tags`
  * merge this branch into main squashing commits **without deleting the branch**
  * [create a new release](https://gitlab.com/eyeo/snippets/-/releases/new) following these steps:
    * specify the [semver](https://semver.org/) tag version for the release, i.e. `vX.X.X`
    * create the release from `main` branch
    * use **@eyeo/snippets vX.X.X** as release title
    * optionally pick a *milestone* if needed
    * use the very same release notes/description previously approved and reviewed by your peers
  * once everything looks good, checkout `main`, pull and run `npm publish` being sure you have access rights and a *2FA* protected account
