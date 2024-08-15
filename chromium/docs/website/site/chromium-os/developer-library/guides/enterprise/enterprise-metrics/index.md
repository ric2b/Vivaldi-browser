---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: enterprise-metrics
title: Enterprise metrics
---

[TOC]

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

Once the access to the above group is granted, you can run below queries in Plx
to get some adoption data:

*   Head to https://plx.corp.google.com/
*   Click on ‘New SQL Script’
*   Write a query using the sample given here as a reference and execute it. (A
    [Plx script](https://plx.corp.google.com/scripts2/script_cb._47877f_23a0_40c2_9769_b48284ad621b)
    with these queries has been created and shared with the team.)

![Choosing a test image](/chromium-os/developer-library/guides/enterprise/enterprise-metrics/enterprise_metrics.png)

The first select statement provides us with domains that have enabled the
‘AllowCellularSimLock’ policy along with the number of affected users in that
domain. The number of entries in the result is the number of domains who are
using this policy.

The second select statement provides us with the count of unique end users
across all domains on whom the policy has been applied.