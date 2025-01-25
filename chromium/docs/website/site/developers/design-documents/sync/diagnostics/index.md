---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/design-documents
  - Design Documents
- - /developers/design-documents/sync
  - Sync
page_name: sync-diagnostics
title: Sync diagnostics
---

### Overview

*   Located in **about:sync** (or **chrome://sync-internals**)
*   Tabs:
    *   About: Concise status overview, also contains mini versions of "Data"
        and "Traffic Log"
    *   Data: Dump debugging data to text for copy & paste
    *   Sync Node Browser: Browse all synced data
    *   Search: Search for strings in all synced data
    *   User Events: Create test USER_EVENTS entities
    *   Traffic Log: Log of all of the sent and received messages to and from
        Chrome sync servers
    *   Invalidations: Inspect incoming sync invalidations per data type, i.e.
        messages from the server informing the client that new data is
        available.

### About tab

*   Contains all the most-used features and stats; most error states should
    show up here, and will be colored red
*   Updates on the fly
    *   Try moving a bookmark, or stopping and starting sync
*   Contains "Dump status" (same as the "Data" tab), and a protocol log (same as
    the "Traffic Log" tab)

### Data tab

*   "Dump sync events to text" button: Dumps all info from the "About" tab,
    including the protocol log, to copy-pastable text
    *   But can also select datatypes info to dump
*   "Dump sync nodes to file" button: Dumps all the above, plus the actual
    synced data (basically the contents of the "Sync Node Browser" tab), to a
    file

### Sync Node Browser tab

*   Shows sync nodes aka entities
    *   Sync nodes are structured in a tree, shown on the left, with details of
        the selected node on the right
    *   Each data type has a top-level node which is a child of the root node
    *   Only the bookmarks sub-tree has a true tree structure
        *   Everything else is just a list under the top-level node
*   Does *not* auto-update; you need click "Refresh"

### Search tab

*   Searches expressions in all the nodes
*   Supports Regex over the JSONified version of each node
*   Has quick searches to provide for easy access to interesting nodes
    *   Unapplied updates
    *   Unsynced items
    *   Conflicted items (unapplied and unsynced)
    *   Deleted items

### User Events tab

*   Allows creating test USER_EVENTS

### Traffic Log tab

*   Shows all messages (Commit and GetUpdates) sent between Chrome and the sync
    servers
*   By default, does *not* capture the actual entity specifics; this can be
    enabled via the "Capture Specifics" checkbox in the "About" tab, or the
    `--sync-include-specifics` command-line param
*   Logs all messages that happen while sync-internals is open, plus up to 6 (by
    default) messages that happened before; this can be configured via the
    `--sync-protocol-log-buffer-size` command-line param

### Invalidations tab

*   Show a log and running totals (since browser startup) of incoming
    invalidations per data type
