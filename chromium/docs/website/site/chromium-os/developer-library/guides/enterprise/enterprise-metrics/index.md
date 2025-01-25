---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: enterprise-metrics
title: Enterprise metrics
---

[TOC]

Note: The following information is only relevant for developers with access to
Google-internal data (e.g., Google employees).

As we launch new features, we tend to collect adoption metrics and if a feature
has an admin policy, we’ll have to access DPanel data to get information such as
the number of unique domains that have applied the policy or the number of end
users upon whom this policy has been applied etc. Accessing this data requires
some permissions and queries to be run and this article is written to provide
that information.

## Steps

### Permissions

*   Request access to this
    [Ganpati group](https://ganpati2.corp.google.com/group/dasher-policies-dump-readonly-policy.prod).

### Query

Once the access to the above group is granted, you can run queries in Plx
against the dasher_d3 table.

*   Head to https://plx.corp.google.com/
*   Click on ‘New SQL Script’
*   Write a query against the dasher_d3 table. Use data explorer to understand
the table parameters you want to query against.

Reach out to internal development lists for help if necessary.