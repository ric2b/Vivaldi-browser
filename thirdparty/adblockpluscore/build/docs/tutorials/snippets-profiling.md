The latest version of Adblock Plus (ABP) allows some snippets to be profiled through the `profile;` flag prefix. Example:

```
example.com#$#profile; log OK
```

This filter enables profiling for the `log` snippet, sending information to the background page.

### Getting to the background page

In Chrome, go to `chrome://extensions/`. In the ABP card, click the _Inspect views_ link.

![inspect background page in Chrome](img/abp-inspect-background-chrome.png)

In Firefox, go to `about:addons`. Click the **Settings** icon and select **Debug Add-ons**.

Locate the ABP card and click **Inspect**.

![inspect background page in Firefox](img/abp-inspect-background-firefox.jpg)

### Retrieving messages

Different from the regular _devtool_ attached to the page, or the extension itself, the _background_ inspector preserves its state over a page refresh or reload.

It is helpful to attach listeners so information from other tabs can be collected and analyzed.

To listen to a message, use the following API:

```js
browser.runtime.onMessage.addListener(console.log);
// to remove the listener in the future
// browser.runtime.onMessage.removeListener(console.log);
```

## Collecting profile details

Each message sent to the background page is an object with a few keys.
The keys most relevant for profiling data are:

  * The `type` equivalent to the string `"profiler.sample"`.
  * The `category` equivalent to the string `"snippets"`.
  * The `samples` entry, which contains an `Array` of sample objects with a `name` (a label describing the code being sampled). It also includes a `duration` detail (the time it took for the code being sampled to execute).

```js
// used to be notified about data coming
let profiler = new EventTarget();

profiler.addEventListener("data", ({detail}) =>
{
  // collect the detail as needed
  let {time, name, duration} = detail;
  console.log(`The profiled data ${name} took ${duration.toFixed(2)}ms`);
});


// listener dedicated to profiling only
browser.runtime.onMessage.addListener(
  ({type, category, samples}) =>
  {
    // ensure you collect details only when/if sent
    if (type !== "profiler.sample" || category !== "snippets")
      return;

    const time = Date.now();
    for (let {name, duration} of samples)
    {
      profiler.dispatchEvent(
        new CustomEvent('data', {detail: {time, name, duration}})
      );
    }
  }
);
```

