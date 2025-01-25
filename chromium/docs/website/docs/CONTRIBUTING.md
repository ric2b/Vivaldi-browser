# Contributing to www.chromium.org

In order to contribute to this repo you must have signed the
[Google Contributor License Agreement](https://cla.developers.google.com/clas)
and have an active account on
[Chromium's Gerrit Host](https://chromium-review.googlesource.com).

## Making edits to pages via the web

The site contains a fairly rudimentary in-page editor. To edit a page,
click on the "Edit this Page" button in the left nav bar. That will take
you to [edit.chromium.org](https://edit.chromium.org/edit?repo=chromium/website/main)
and open the page in the editor automatically.

You can edit the Markdown text directly, and, once you're ready to upload
the change, if you you click on the "Create Change" box in the bottom right
corner of the page, that will create a Gerrit CL for review. A builder
will automatically run to build out a copy of the site containing your
changes so that you can preview them.

Any current Chromium/ChromiumOS contributor (basically anyone with with
try-job access or bug-editing privileges) can review CLs, but you also
need OWNERS approval to land them.

This functionality is limited to just editing the text of existing pages,
and there's not yet any way to preview the change before you upload it
for review.

If you need to upload new images or other assets, or add new pages, or
change multiple pages at once, or do anything else more complicated,
keep reading ...

## Making bigger changes using a local Git checkout

*NOTE: If you have an existing Chromium checkout, you should
see this under chromium/src/docs/website already. Skip to step 3.*

*NOTE: If you have an existing ChromiumOS checkout, you should
see this under chromiumos/website already. Skip to step 3.*

1.  Install depot_tools:

    ```bash
    $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    $ export PATH=/path/to/depot_tools:$PATH
    ```

2. Check out the repo and its dependencies:

    ```bash
    $ git clone https://chromium.googlesource.com/website
    $ cd website
    ```

    or

    ```bash
    $ fetch website
    ```

3. Download dependencies:

   ```bash
   $ gclient sync
   ```

4.  Make your changes! Check out [AUTHORING.md](AUTHORING.md) for guidelines.

5.  Build all of the static pages up-front to check for errors.
    The content will be built into `//build` by default.

    ```bash
    $ ./npmw build
    ```

    It should only take a few seconds to build out the website.

    (`npmw` is a simple wrapper around `npm-run-all` that finds the
    appropriate node binary to use.)

6.  Start a local web server to view the site. The server will (re-)generate
    the pages on the fly as needed if the input or conversion code changes.
    The content will be built into `//build` and will be available at the
    `Local` URL.

    ```bash
    $ ./npmw start
    ```

    > Note: If you are a Googler using a Cloudtop you will need to add `-L
    > 8080:localhost:8080` to your
    > [Secure Shell](https://chromewebstore.google.com/detail/secure-shell/iodihamcpbpeioajjeobimgagajmlibd)
    > ssh arguments to view the `Local` URL in your regular browser, or use
    > [Chrome Remote Desktop](https://remotedesktop.corp.google.com/).
    > [Tricium](https://chromium.googlesource.com/infra/infra/+/main/go/src/infra/tricium/README.md)
    > also comments on CLs uploaded to Gerrit with a preview link.

7.  Check in your changes and upload a CL to the Gerrit code review server.

    ```bash
    $ git commit -a -m 'whatever'
    $ git-cl upload
    ```

    If you are adding binary assets (images, etc.) to the site, you will
    need to upload them to the GCS bucket using `//scripts/upload-lobs.py`.

8.  Get one of the [//OWNERS](../OWNERS) to review your changes, and then
    submit the change via the commit queue.

    *NOTE:* If this is your first time contributing something to Chromium
    or ChromiumOS, please make sure you (or your company) has signed
    [Google's Contributor License Agreement](https://cla.developers.google.com/),
    as noted above, and also add yourself to the [//AUTHORS](../AUTHORS) file
    as part of your change.
