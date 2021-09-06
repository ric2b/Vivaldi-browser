We have recently landed the `hide-if-matches-xpath` snippet, which allows the usage of [XPath](https://www.w3.org/TR/1999/REC-xpath-19991116/) queries to directly target elements.

This document summarizes how to use _XPath_, and explains how the snippet helps to hide unwanted ads from a generic page.


### Background

_XPath_ is a web standard available since the year 2000. It is not an extremely common technology compared to _CSS_. However, it is different from _CSS_ selectors because it allows queries to crawl any document in different axes.

For example, with _CSS_ selectors it is not possible to target the parent node of an element that matches some specific rule, while _XPath_ provides [axes-related](https://developer.mozilla.org/en-US/docs/Web/XPath/Axes) syntax to move around any specific target node.

This possibility alone makes _XPath_ unique and more powerful than just _CSS_ selectors, which is one of the reasons we decided to ship the `hide-if-matches-xpath` snippet.


### Basic Examples

Visiting [example.com](https://example.com/) after adding the following filter hides the main `<div>` element by checking the content of any node in the document:

```
example.com#$#hide-if-matches-xpath '//*[contains(text(),"More information...")]/ancestor::div'
```

However, it is possible to also target any other node by crawling the hierarchy of such an ancestor via `/`, resulting in hiding only the first `<p>` element of the container. Example:

```
//*[contains(text(),"More information...")]/ancestor::div/p[1]
```

But, if it is the parent's previous sibling that we are after, we can obtain the same result via the following selector:

```
//*[contains(text(),"More information...")]/ancestor::*/preceding::p
```

In these examples, we have already used a few _XPath_ concepts, such as:

  * a wild character or a tag name to reach any, or a specific kind of, node
  * the document root via `//`, or the immediate next path via `/`, to continue crawling via other queries
  * the `contains(source, match)` function
  * axes, such as `ancestor` or `preceding`, to move around the initial node


### Functions

There is a list of available [XPath functions](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions) in _MDN_, but the most interesting for ad-blocking purposes are:

  * [concat()](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions/concat) - to concatenate strings from various attributes or multiple nodes
  * [contains()](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions/contains) - to verify if a specific node attribute or content contains a specific string
  * [last()](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions/last) - to retrieve the last element that matches a specific query, as in `//p[last()]`
  * [not()](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions/not) - to negate any expression, as in `//*[not(*[last()])]`
  * [position()](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions/position) - to compare a specific element position, as in `//p[position() = 2]`; mostly useful together with `not()`
  * [starts-with()](https://developer.mozilla.org/en-US/docs/Web/XPath/Functions/starts-with) - to search a specific value at the beginning of some text or attribute, as in `//a[starts-with(@href,"https://www.iana.org/")]`

There are surely other functions that might be handy in specific cases, but the best part is that all functions can be combined and used as expressions.


### Axes

In _MDN_, there is also a [list of usable axes](https://developer.mozilla.org/en-US/docs/Web/XPath/Axes) that are all useful.

The handiest tip regarding axes is that `self` can be represented as `.` and parent can be represented as `..`.

The following query, for example, hides the first previous sibling from an `https:` link, regardless of its tag name.

```
//a[starts-with(@href,"https:")]/../preceding-sibling::*[position()=1]
```


### Operators

This time from the [W3schools pages](https://www.w3schools.com/xml/xpath_operators.asp), the list of operators resembles most programming languages, and the only one that might cause confusion is the `div` to divide numbers.

For ad-blocking use cases though, besides `<`, `>`, `=`, and similar operations needed in conjunction with `position()`, for example, it is important to remember that the pipe `|` operator can be used to group multiple queries at once, which is basically the equivalent of the comma `,` CSS separator.


### Location paths

Back to the [standard XPath documentation](https://www.w3.org/TR/1999/REC-xpath-19991116/#location-paths), there is a long list of path examples, but the most important path for ad-blocking purposes is `text()`, which retrieves the node text to search, and `node()`, which grabs all children of a specific target.
