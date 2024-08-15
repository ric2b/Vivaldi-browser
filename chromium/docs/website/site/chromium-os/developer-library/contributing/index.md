---
breadcrumbs:
- - /chromium-os/developer-library
  - ChromiumOS
page_name: contributing
title: Contributing
---

> See [Contributing to www.chromium.org](https://chromium.googlesource.com/website/+/refs/heads/main/docs/CONTRIBUTING.md)
> for a general contribution guide.

Maintaining a well-curated developer library is a worthy goal which requires
the entire team to contribute. Thankfully modifying the CrOS Developer Library
is straightforward, and this guide will take you through the steps.

## Adding a new page to CrOS Developer Library

Once you have the website source code (see [Contributing to www.chromium.org](https://chromium.googlesource.com/website/+/refs/heads/main/docs/CONTRIBUTING.md)), you are able to add a new page (document)
to the library.

### Determining the right library location

The developer library is organized into four main sections: Getting Started,
Guides, Reference, and Training. The Getting Started guide is a multipage book,
while the other sections are flat indexes grouped by topic. To make sure your
new content is easy to find, pick the most appropriate location and topic for
your new page.

<table>
  <thead>
    <tr>
      <th scope="col">Section</th>
      <th scope="col">Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th scope="row">Getting Started</th>
      <td>A multipage guide to bootstrap the development process</td>
    </tr>
    <tr>
      <th scope="row">Guides</th>
      <td>Step-by-step instructions for accomplishing tasks</td>
    </tr>
    <tr>
      <th scope="row">Reference</th>
      <td>Educational descriptions of tools, processes, and infrastructure</td>
    </tr>
    <tr>
      <th scope="row">Training</th>
      <td>Person-oriented training material like videos and codelabs</td>
    </tr>
  </tbody>
</table>

### Adding the page directory and index.md

To give an example of a new page to add to the Guides section of the library
whose URL is `chromium.org/chromium-os/developer-library/guides/code-coverage`,
create the new source folder named code-coverage and Markdown file index.md:

```
website$ mkdir chromium-os/developer-library/guides/code-coverage
website$ touch chromium-os/developer-library/guides/code-coverage/index.md
```

### Writing the page metadata

At the top of each page in the library, breadcrumb links are rendered to allow
easy navigation. This content is rendered from the metadata section at the top
of each Markdown file. Here is the metadata for our example new page:

```
---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: code-coverage
title: Code Coverage
---
```

The `breadcrumbs` section has two entries: the first is the URL of the
breadcrumb link, and the second is the text rendered in the link. The
`page_name` variable should match the name of the directory containing this
Markdown file. The `title` variable is the title of the rendered webpage.

### Creating content

You're now ready to create the content of the new library page. Please consider
technical writing best practices by writing to the intended audience, succinctly
providing necessary context, and being to-the-point.

### Testing, Uploading, and Submitting Changes

See [Contributing to www.chromium.org](https://chromium.googlesource.com/website/+/refs/heads/main/docs/CONTRIBUTING.md).