---
breadcrumbs:
- - /chromium-os/developer-library/guides/#testing
  - ChromiumOS > Guides
page_name: anatomy-of-test-test
title: Anatomy of test.test
---

This document will go through the pieces of test.test that are available to use,
describing what each does and when it could be used.

# Attributes

job backreference to the job this test instance is part of

outputdir eg. results/&lt;job&gt;/&lt;testname.tag&gt;

resultsdir eg. results/&lt;job&gt;/&lt;testname.tag&gt;/results

profdir eg. results/&lt;job&gt;/&lt;testname.tag&gt;/profiling

debugdir eg. results/&lt;job&gt;/&lt;testname.tag&gt;/debug

bindir eg. tests/&lt;test&gt;

src eg. tests/&lt;test&gt;/src

tmpdir eg. tmp/&lt;tempname&gt;_&lt;testname.tag&gt;

# initialize()

Everytime a test runs this function runs. For example, if you have
iterations=100 set this will run 100 times. Use this when you need to do
something like ensure a package is there.

# setup()

setup() runs the first time the test is set up. This is often used for compiling
source code one should not need to recompile the source for the test for every
iteration or install the same

# run_once()

This is called by job.run_test N times, where N is controlled by the iterations
parameter to run_test (Defaulting to one). it also gets called an additional
time with profilers turn on, if any profilers are enabled.

# postprocess_iteration()

This processes any results generated by the test iteration and writes them out
into a keyval. It's generally not called for the profiling iteration, as that
may have difference performance implications.

warmup()
