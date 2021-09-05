This guide is aimed at filter authors and advanced users that want to make the most out of Adblock Plus. It's a companion to the [guide on writing filters](https://help.eyeo.com/en/adblockplus/how-to-write-filters) that covers snippet filters. While it's recommended that you be familiar with writing filters, you should be OK otherwise. You don't need a deep knowledge of JavaScript, but you need to be familiar with it, as well as with web technologies like HTML, CSS, and the DOM. If you really want, Mozilla has a great resource to [learn JavaScript, HTML and CSS](https://developer.mozilla.org/en-US/docs/Learn/JavaScript).

This guide doesn't cover how to write an actual snippet, i.e. the JavaScript code that is loaded by Adblock Plus into the browser when you execute a snippet filter. This requires modifying the extension, and it will be covered in an upcoming developer guide.

## Table of contents

- [What are snippet filters?](#about-snippet-filters)
- [Syntax](#syntax)
  - [Arguments](#arguments)
- [Examples](#examples)
- [Debugging](#debugging)
- [Snippets reference](#snippets-ref)

## <a name="about-snippet-filters"></a>What are snippet filters?

Snippet filters are a new type of filter introduced in Adblock Plus 3.3 that allows running specialized code snippets (written in JavaScript) on pages within a list of specified domains. These snippets are selected from an existing library available in Adblock Plus.

The snippet approach allows interfering with ad display code in a more efficient manner, and also allows locating elements to hide in a way that can't be expressed with CSS or the current advanced selectors.

For security reasons, snippet filters can only be added to a user's custom filters list or in the [*ABP Anti-Circumvention Filter List*](https://github.com/abp-filters/abp-filters-anti-cv), the latter being vetted and reviewed.

## <a name="syntax"></a>Syntax

The basic syntax of a snippet filter is as follows:

`domain#$#script`

- `domain` - A domain or a list of comma-separated domain names. See [example 2](#example-domains) and [how to write element hiding filters](https://help.eyeo.com/en/adblockplus/how-to-write-filters#elemhide_domains).

- `#$#` - This sequence marks the end of the domain list and the start of the actual snippet filter. `#$#` mean this is a snippet filter. There are other separator sequences for different kinds of filters, as explained in the [How to write filters](https://help.eyeo.com/en/adblockplus/how-to-write-filters#elemhide_basic) guide.

- `script` - A script that invokes one or more snippets.

The `script` has the following syntax:

`command arguments`

- `command` - The name of the snippet command to execute. Consult the [snippet commands reference](#snippets-refs) for a list of possible snippet commands.

- `arguments` - The snippet command arguments, separated by spaces. The [snippet commands reference](#snippets-refs) defines the arguments.

You can run several snippets in the same filter by separating them with a `;`. See [example 6](#example-multiple-snippets).

### <a name="arguments"></a>Arguments

Arguments are strings. They can be surrounded by single quotes (`'`) if they contain spaces. To have a single quote `'` as part of the argument, escape it like this: `\'` ; you can also escape spaces with `\ ` instead of quoting the argument. See [examples 3a](#example-quote), [3b](#example-quote-2) and [3c](#example-quote-3).

You can input Unicode characters by using the syntax `\uNNNN`, where NNNN is the Unicode codepoint. See [example 4](#example-unicode).

You can also input carriage return, line feed, and tabs using `\r`, `\n`, and `\t`, respectively. To have a `\` in an argument, including a regular expression, it must be escaped with `\`, i.e. doubled up. See [example 5](#example-lf).

## <a name="examples"></a>Examples

### Basic snippet filter syntax

These examples demonstrate the syntax of a snippet filter and how to pass arguments. While we mostly use the `log` snippet, these examples apply to other snippet functions as well.

#### <a name="example1"></a>Example 1

`example.com#$#log Hello`

This prints "Hello" in the web console on the domain `example.com` and its subdomains.

#### <a name="example-domains"></a>Example 2

`anotherexample.com,example.com#$#log Hello`

Same as example 1, but "Hello" also appears on the domain `anotherexample.com`. Like with other filter types, multiple domains should be separated by commas `,`.

This filter makes the filter in example 1 obsolete. Having both filters makes the message appear twice on `example.com`, as both would be run.

#### <a name="example-quote"></a>Example 3a

`example.net,example.com#$#log 'Hello from an example to another example'`

This is a different message from example 2. Because it contains spaces, the string must be surrounded with single quotes `'`.

#### <a name="example-quote-2"></a>Example 3b

`example.net,example.com#$#log 'Hello it\'s me again'`

This shows the escape sequence for having a single quote, using `\'`.

#### <a name="example-quote-3"></a>Example 3c

`example.net,example.com#$#log Hello\ no\ quotes`

You can also escape spaces instead of quoting the argument, using `\ `.


#### <a name="example-unicode"></a>Example 4

`emoji.example.com#$#log \u1F601`

On the domain `emoji.example.com`, the emoji 😀 is printed in the console. The filter will not be run on `example.com` or `www.example.com`. This shows how to use escape for Unicode symbols.

#### <a name="example-lf"></a>Example 5

`emoji.example.com#$#log '\u1F601\nand a new line'`

This is similar to example 4, except that the filter prints the emoji 😀 and then on a new line, prints `and a new line`. This shows how to use escape for Unicode symbols, as well as adding a new line.

#### <a name="example-multiple-snippets"></a>Example 6

Running two snippets:

`debug.example.com#$#debug; log 'This should be prefixed by DEBUG'`

First, we see `debug`. This is the snippet that enables debugging. It's just like any other snippet. Then we see `log`, whose output is modified when debugging is enabled. See the section on [debugging](#debugging) for more details.

`example.com#$#log 'Always log'; log 'And better twice than once'`

This is another example of logging two lines. Here the snippet command will be called twice.

## <a name="debugging"></a>Debugging

We saw a `debug` snippet in [Example 6](#example-multiple-snippets). Its use is simple: calling `debug` before a snippet turns on the debug flag. If the other snippets support it, they'll enable their debug mode. What a snippet does in debug mode is up to the snippet.

## <a name="snippets-ref"></a>Snippet commands reference