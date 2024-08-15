---
breadcrumbs: []
page_name: tab-to-search
title: Tab to Search
---

Among the many features Chromium's Omnibox offers is the ability to search a
site without navigating to the sites homepage. Once Chromium has determined it
can search a site, any time the user types the URL of the site into the Omnibox
the user is offered the ability to search the site. Once the user presses tab,
then types in a string and presses the enter, the site is searched and the
results shown to the user. It's important to note the search is done by the site
itself, not Google or Chromium.

Chromium provides two heuristics that automatically add a site to the list of
searchable sites. The following describes these heuristics:

1. On your site's homepage provide a link to an
[OpenSearch description document][osdd].

    The link to the OSDD is placed in the head of the html file. For example:

    ```html
    <head>
      <link type="application/opensearchdescription+xml" rel="search" href="url_of_osdd_file"/>
    </head>
    ```

    The important part of this document is the URL used to search your site. The
    following is an example that contains the bare minimum needed, see the
    [OpenSearch description document specification][osdd] for the list of values
    you can specify.

    ```xml
    <?xml version="1.0"?>
    <OpenSearchDescription xmlns="http://a9.com/-/spec/opensearch/1.1/">
      <ShortName>Search My Site</ShortName>
      <Description>Search My Site</Description>
      <Url type="text/html" method="get" template="http://my_site/{searchTerms}"/>
    </OpenSearchDescription>
    ```

    When the user presses enter in the Omnibox the string {searchTerms} in the
    url is replaced with the string the user typed.

    You can also include a suggestion service by adding another URL element with
    rel="suggestions" such as:

    ```xml
    <Url type="application/json" rel="suggestions" template="http://my_site/suggest?q={searchTerms}" />
    ```

    If you include this, the omnibox will use your suggestion service to provide
    query suggestions based on the user's partial query.

2. If the site does not provide a link to an OpenSearch description document but
the user submits a form, then Chrome automatically adds the site to the list of
searchable sites. There are a number of restrictions with this approach though.
In particular the form must generate a GET, must result in a HTTP url, and the
form must not have OnSubmit script. Additionally there must be only one input
element of type text, no passwords, files or text areas and all other input
elements must be in their default state.

For 1 and 2 Chromium only adds sites that the user navigated to without a path.
For example `http://mysite.com`, but not `http://mysite.com/foo`.

[osdd]: https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#opensearch-description-document