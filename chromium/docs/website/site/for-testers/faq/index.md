---
breadcrumbs:
- - /for-testers
  - For Testers
page_name: faq
title: Issue Tracker FAQ
---

### What happened?

Chromium has moved to a different issue tracker to provide a well-supported user experience
for the long term. We migrated all Chromium issues, including issue history and stars, from
Monorail to a different tool: Chromium Issue Tracker, powered by the
[Google Issue Tracker](https://developers.google.com/issue-tracker).
This tooling change will provide a feature-rich and well-supported issue tracker for
Chromium's ecosystem. Chromium has joined other open source projects (Git, Gerrit) on this
tooling. Existing transparency levels to bugs will be maintained.

### What about historic bug links?

Existing Monorail issue links will redirect to the migrated issues in the new issue tracker.

---

## General Workflow FAQ

*FAQs specifically focused on what common Monorail workflows now look like in Issue Tracker
and how things will generally work in Issue Tracker.*

### How do I file an issue?

You can report an issue in the Issue Tracker using the Issue Wizard at
[issues.chromium.org](https://issues.chromium.org/issues/new) or see Issue
Tracker documentation on [how to file an issue](https://developers.google.com/issue-tracker/guides/create-issue-ui).


### How do I file a Security bug?

When filing a new issue:
* Set the component to  “Chromium” (top level component)
* Select the “Security Bug” template in the Template dropdown.
* Provide a title, description according to the content template, and any relevant attachments.
* Press “Create”.

When marking an existing issue as a Security bug:
* **CC** yourself on the issue if you would like to preserve access to the issue
* Set the Type to “Vulnerability”. Once this is done, the following will automatically occur:
  * The issue’s Access setting will be updated to "Limited Visibility" 
  * security@chromium.org will be added as a Collaborator

### How do I search for an issue?

See Issue Tracker’s documentation on [how to search for an issue](https://developers.google.com/issue-tracker/concepts/searches).

Issue Tracker has its own distinct query syntax that is very similar to Monorail, supporting
the majority of issue search features that Monorail did. For more detail on how to convert
Monorail searches into Issue Tracker, see [Issue Tracker Query Syntax for Monorail users](/for-testers/query-syntax).

### How do I search for a component?

See Issue Tracker’s documentation on [how to search for a component](https://developers.google.com/issue-tracker/guides/find-a-component).

Also see [Issue Tracker Query Syntax for Monorail users](/for-testers/query-syntax)
for Issue Tracker query syntax for Monorail users.

### How do I find my Monorail labels?

**Step 1)** All labels in Monorail have been migrated to either a hotlist or custom field in Issue Tracker.
Note: the majority of Monorail labels have been migrated to the “Chromium Labels” custom field.

**Step 2)**
* *[Option A]* <br>
   If your label has been mapped to a hotlist, see Issue Tracker’s documentation on
   [how to work with hotlists](https://developers.google.com/issue-tracker/guides/work-with-hotlist#:~:text=Click%20the%20magnifying%20glass%20icon%20next%20to%20Hotlists%20in%20the%20left%2Dhand%20navigation).
   Also see the predefined migration Bookmark Groups:
    * The [“Chromium Migrated Hotlists” Bookmark group](https://issues.chromium.org/bookmark-groups/835579)
       contains ALL hotlists that have been created as part of Chromium’s migration
        to Issue Tracker.
    * The [“Chromium Critical Hotlists” Bookmark group](https://issues.chromium.org/bookmark-groups/860925)
        contains critical hotlists needed to manage Chrome.
    * Example search query: `hotlistid:<hotlistid>`

* *[Option B]* <br>
   If your label has been mapped to a custom field, see Issue Tracker’s documentation on [how to search by a custom field value](https://developers.google.com/issue-tracker/concepts/custom-fields#searching_for_issues_by_custom_fields).

  * Example search query: `customfield[id]:<value>`

  * Example Chromium Label query: `customfield1223031:Type-Bug-Regression`

### How do I search for hotlists?

See Issue Tracker’s documentation on [how to search for a hotlist](https://developers.google.com/issue-tracker/guides/work-with-hotlist#:~:text=Click%20the%20magnifying%20glass%20icon%20next%20to%20Hotlists%20in%20the%20left%2Dhand%20navigation).

*Tip:* Autocomplete will help find the hotlist ID if you start typing the name of the
hotlist in the search bar after `hotlistid:`

### What are the recommended ACLs that I should use for my hotlists in Issue Tracker?
Unlike labels in Monorail, hotlists in Issue Tracker have ACLs. To ensure your hotlists are
not overly restricted, we recommend that you add `edit-bug-access@chromium.org` to the
“View and Append” ACL. If you want to make your hotlist publicly visible, add `Public` to the
“View only” ACL.

### How do I access the ACLs for my hotlists in Issue Tracker?
There are two ways to access the Hotlist ACLs:

Option 1:
Click into your hotlist, then click the pencil next to the hotlist name.
[<img alt="image" src="/for-testers/faq/SS_17.png">](/for-testers/faq/SS_17.png)

Option 2:
Use this URL: https<nolink>//issues.chromium.org/hotlists/**&lt;hotlistid>**/edit

### How do I search by custom fields?

See Issue Tracker’s documentation on [how to search by a custom field value](https://developers.google.com/issue-tracker/concepts/custom-fields#searching_for_issues_by_custom_fields).

### How do I determine the urgency of an issue?

Issue urgency is indicated by Issue Tracker’s required Priority field. See Issue Tracker’s
documentation on [issue priority](https://developers.google.com/issue-tracker/concepts/issues#priority).

### How do I mark an issue as a duplicate?

See Issue Tracker’s documentation on [how to mark an issue as duplicate](https://developers.google.com/issue-tracker/guides/duplicate-issue).

### Will I be able to add labels to issues in Issue Tracker?

Issue Tracker does not support the concept of labels or tags. All labels in Monorail have
been migrated to either a hotlist or custom field in Issue Tracker.

Where you would've used a label in Monorail, we recommend you use a hotlist in Issue Tracker.

### What happened to the auto-CC rules from Monorail?

The auto-CC rules from Monorail were not migrated. However, it is possible in
the Issue Tracker to be cc'd on bugs in a specific component as described below.

### How do I ensure I’m automatically CC’d on all of the things I care about?

Like Monorail, Issue Tracker supports the ability to Auto-CC users on a particular component.
In Issue Tracker, this feature is supported by adding yourself or others to the CC field in
the default template for a given component.

Issue Tracker will automatically CC any users listed in the default template of a component
when issues are filed within that component. Additionally, Issue Tracker will also CC these
users if an existing issue is moved into the component.

If you would like to request a new template or change for an existing template,
please file a bug [here](https://issues.chromium.org/issues/new?component=1456459&template=1935230).

### How do I bulk edit bugs?

See Issue Tracker’s documentation on
[how to edit issues in bulk](https://developers.google.com/issue-tracker/guides/edit-issue-bulk).

### How do I view migrated issues in Monorail post migration?

Post migration, Chromium’s Monorail project will be marked as read-only, and all Chromium bugs
in Monorail will automatically redirect to their counterparts in Issue Tracker. To prevent
this redirect, append the `?no\_tracker\_redirect=true` parameter to a Monorail issue URL.

Example: https://crbug.com/skia/4905?no\_tracker\_redirect=true

### What happened to my Monorail hotlists? How do I find them?

Given hotlists in Monorail are not project-specific (ie. a hotlist can contain issues from
multiple Monorail projects), we will only migrate hotlists that
   * A) are owned by an @google.com or @chromium.org account
AND
   * B) contain at least one Chromium issue.

This is intended to be complete shortly after migration.
Once available, to search for a hotlist, use the following search query: `hotlistid:<hotlistid>`

### Will I be able to star issues in Issue Tracker?

Like Monorail, Issue Tracker allows individual users to star issues. One difference is that
Issue Tracker splits up Monorail's concept of starring into two separate concepts. In Issue
Tracker, users can either +1 an issue or star an issue. A +1 is a publicly visible vote while
a star is a private subscription to an issue. For public users interacting with Issue Tracker,
by default, clicking to star an issue both stars and +1s an issue. See official documentation
[here](https://developers.google.com/issue-tracker/guides/subscribe#subscribing_by_starring_an_issue).

As part of Chromium’s migration, all historical Monorail stars were migrated to Issue Tracker
as +1s <span style="text-decoration:underline;">and</span> stars. This will ensure that users
who have starred issues in Monorail will be subscribed to the same issues in Issue Tracker.

### Will I be able to add multiple components to an issue in Issue Tracker?

No, Issue Tracker does not support adding multiple components to issues. Monorail issues
with multiple components will use heuristics to select a single primary component then list
all additional components in the “Component Tags” custom field.

### Since many labels will be replaced by hotlists, how will hotlists differ from labels?

Here are a few key differences between Monorail labels and Issue Tracker hotlists:

<table>
  <tr>
   <td style="background-color: #4a86e8"></td>
   <td style="color: #ffffff;background-color: #4a86e8;text-align:center"><strong>Monorail Labels</strong>
   </td>
   <td style="color: #ffffff;background-color: #4a86e8;text-align:center"><strong>Issue Tracker Hotlists</strong>
   </td>
  </tr>
  <tr>
   <td><strong>Who can add them to an issue?</strong></td>
   <td>Any issue editor in a Monorail project</td>
   <td>Anyone that has <code>View</code> permission for the issue and <code>Append</code> permission for the Hotlist.</td>
  </tr>
  <tr>
   <td><strong>Who can see them?</strong></td>
   <td>Anyone who can view an issue can see all labels on the issue</td>
   <td>Anyone added as a hotlist viewer, which can include the Public (anyone on the Internet)
   </td>
  </tr>
  <tr>
   <td><strong>Are names unique?</strong></td>
   <td>Yes, labels are uniquely identified in Monorail by their name</td>
   <td>No, hotlists are uniquely identified by their ID</td>
  </tr>
  <tr>
   <td><strong>Can names be updated?</strong></td>
   <td>No, changing the name of a Monorail label changes the label</td>
   <td>Yes, hotlists can be renamed in Issue Tracker without changing the identity of the hotlist or issues in them
   </td>
  </tr>
  <tr>
   <td><strong>How do you search for them?</strong></td>
   <td>label=&#60;labelname></td>
   <td>hotlistid:&#60;hotlistid>
<p>
<p>
<strong>Tip:</strong> Autocomplete will help find the hotlist ID if you start typing the name of the hotlist in the search bar after <em>hotlistid:</em>
   </td>
  </tr>
</table>

### Are there any Bookmark Groups that are important to note?

Bookmark Groups are intended to give users quick access to important hotlists. The following
Bookmark Groups have been created for Chromium users and are available if you’d like quick
access to the following hotlists:

* The [“Chromium Migrated Hotlists” Bookmark group](https://issues.chromium.org/bookmark-groups/835579)
    contains ALL hotlists that have been created as part of Chromium’s migration
    to Issue Tracker.

* The [“Chromium Critical Hotlists” Bookmark group](https://issues.chromium.org/bookmark-groups/860925)
  contains critical hotlists needed to manage Chrome.

---

## Information about Triage workflows
Before trying any of these workflows, Chromium contributors should first click on the
following link for the [Suggested Chromium Triage Workflow](https://issues.chromium.org/bookmark-groups/860874)
Bookmark group and **"star"** it:

Starring the Bookmark group will place it in your sidebar for easy access:

[<img alt="image" src="/for-testers/faq/SS_1.png">](/for-testers/faq/SS_1.png)

Also Chromium contributors should star the
[Chromium Triage Hotlist](https://issues.chromium.org/bookmark-groups/865505)
Bookmark group.  This bookmark group contains a small number of hotlists,
and by starring it you will also be able to access it from the sidebar:

[<img alt="image" src="/for-testers/faq/SS_2.png">](/for-testers/faq/SS_2.png)

Newly created issues default to being in the **“New”** state. Templates that previously set
**"Unconfirmed"** as their default status in Monorail have been configured to add the
**"Unconfirmed"** hotlist.

### How to use hotlists

To mark an issue as triaged mark the issue as `Available`. To do that you click on **"Add Hotlist"**:

[<img alt="image" src="/for-testers/faq/SS_3.png">](/for-testers/faq/SS_3.png)

Now click on the Available checklist item:

[<img alt="image" src="/for-testers/faq/SS_4.png">](/for-testers/faq/SS_4.png)

*Note 1*: `Unconfirmed` is also there and can be checked/unchecked as needed.

*Note 2*: The two hotlists that appear on the popup dialog box are the ones that are in the
Chromium Triage Hotlists Bookmark group. Any hotlists you star, or add to a Bookmark
group will appear in this dialog.

Once you click Save on the dialog the hotlists will be added and will be visible on the
issue, which is also where they can be removed easily by clicking on the **"x"**. In the
following view you will see the hotlists and the Status of the issue:

[<img alt="image" src="/for-testers/faq/SS_5.png">](/for-testers/faq/SS_5.png)

There are two other Bookmark groups that will be useful in populating the hotlist
selection dialog:

*  [Chromium Critical Hotlists](https://issues.chromium.org/bookmark-groups/860925)
     (only hotlists required to manage Chrome)
*  [Chromium Migrated Hotlists](https://issues.chromium.org/bookmark-groups/835579)
     (all hotlists migrated as part of Chromium's migration)

You can add them to your sidebar in the same way as you did above, that is, click on the
links and **"star"** the hostlist. But note that they contain many hotlists, and all the
hotlists that are present in your sidebar, either in Bookmark groups, or hotlists that
you've starred individually, will show up in the Hotlist dialog. In this situation you may
need to use the search bar on the dialog to find the hotlist you want:

[<img alt="image" src="/for-testers/faq/SS_6.png">](/for-testers/faq/SS_6.png)

### Searching

To see `Available` and `Unconfirmed` in lists of issues you can have hotlists show
in the **Title** column:

[<img alt="image" src="/for-testers/faq/SS_7.png">](/for-testers/faq/SS_7.png)

Note you should also select **"Wrap titles"** which is useful in making hotlists visible
in issues with long titles.

[<img alt="image" src="/for-testers/faq/SS_8.png">](/for-testers/faq/SS_8.png)

### Using the Suggested Chromium Triage Workflow Bookmark group

To see all the issues that match the Monorail state you are looking for, click the saved
search in the bookmark group with that name. For example, to see all `Unconfirmed` issues
click on the `Unconfirmed`` in the Bookmark group:

[<img alt="image" src="/for-testers/faq/SS_9.png">](/for-testers/faq/SS_9.png)

The following table shows how Monorail Status values are mapped into Issue Tracker:

<table border="1">
  <tr>
    <td style="color: #ffffff;background-color: #4a86e8;text-align:center"><strong>Monorail</strong>
    </td>
    <td style="color: #ffffff;background-color: #4a86e8;text-align:center"><strong>Issue Tracker</strong>
    </td>
  </tr>
  <tr>
    <td>Unconfirmed</td>
    <td>status:new <code>Unconfirmed</code></td>
  </tr>
  <tr>
    <td>Untriaged</td>
    <td>status:new <code>-Unconfirmed -Available </code></td>
  </tr>
  <tr>
    <td>Available</td>
    <td>status:new <code>Available</code></td>
  </tr>
  <tr>
    <td>Assigned</p></td>
    <td>status:assigned</td>
  </tr>
  <tr>
    <td>Started</td>
    <td>status:accepted</td>
  </tr>
  <tr>
    <td>ExternalDependency</td>
    <td><code>Status_ExternalDependency</code></td>
  </tr>
  <tr>
    <td>Fixed</td>
    <td>status:fixed</td>
  </tr>
  <tr>
    <td>Duplicate</td>
    <td>status:duplicate</td>
  </tr>
  <tr>
    <td>Verified</td>
    <td>status:verified</td>
  </tr>
  <tr>
    <td>WontFix</td>
    <td>status:obsolete</td>
  </tr>
</table>

**Legend**:
<table border="1">
  <tr>
    <td><code>Available</code></td>
    <td>hotlistid:5438642</td>
  </tr>
  <tr>
    <td><code>Unconfirmed</code>
    <td>hotlistid:5437934</td>
  </tr>
  <tr>
    <td><code>Status_ExternalDependency</code>
    <td>hotlistid:5438152</td>
  </tr>
<tr>
    <td><code>Chromium-Regression</code>
    <td>hotlistid:5438261</td>
  </tr>
</table>

### So how can we use the above information to recreate the
[Bug life cycle](/for-testers/bug-reporting-guidelines/#bug-life-cycle)?

The life cycle is copied here for reference:
* When a bug is first logged, it is given **New** status and added to the `Unconfirmed`
   hotlist
* The `Unconfirmed` hotlist is removed when the bug has been verified as a Chromium bug.
* Once a bug has been picked up by a developer, it is marked as **Assigned**
* A status of **In Progress (Accepted)** means a fix is being worked on.


#####  When a bug is first logged, it is given **New** status and added to the `Unconfirmed` hotlist

Templates that previously set **Unconfirmed** as their default status in Monorail have been
configured to add the `Unconfirmed` hotlist.

[<img alt="image" src="/for-testers/faq/SS_10.png">](/for-testers/faq/SS_10.png)

##### The `Unconfirmed` hotlist is removed when the bug has been verified as a Chromium bug.

To move an issue from Unconfirmed to Untriaged just remove the Unconfirmed hotlist:

[<img alt="image" src="/for-testers/faq/SS_11.png">](/for-testers/faq/SS_11.png)

To move a bug from Untriaged to Available just add it to the `Available` hotlist:

[<img alt="image" src="/for-testers/faq/SS_12.png">](/for-testers/faq/SS_12.png)

#####  Once a bug has been picked up by a developer, it is marked as Assigned.

Change the Status to Assigned. **DO NOT** remove the `Available` hotlist. That way if the
issue status moves back to "New" it will then show up in the `Available` search.

[<img alt="image" src="/for-testers/faq/SS_13.png">](/for-testers/faq/SS_13.png)


#####  A status of In Progress (Accepted) means a fix is being worked on.

Change the status to **In Progress (Accepted)**:

[<img alt="image" src="/for-testers/faq/SS_14.png">](/for-testers/faq/SS_14.png)

##### A status of Fixed means that the bug has been fixed, and Verified means that the
fix has been tested and confirmed.

Please note that it will take some time for the "fix" to make it into the various channels
(canary, beta, release) - pay attention to the milestone attached to the bug, and compare
it to chrome://version.

[<img alt="image" src="/for-testers/faq/SS_15.png">](/for-testers/faq/SS_15.png)

### Mark a bug as a regression

To mark a bug as a regression add it to the  `Chromium-Regression` hotlist.

[<img alt="image" src="/for-testers/faq/SS_16-new.png">](/for-testers/faq/SS_16-new.png)

### Further customization

Teams will be able to further fine tune their triage workflows in Issue Trackerpost migration
day if needed. For example a team might need to add a step of triage through another hotlist
by adding a custom field. Saved searches, custom fields, hotlists, components and bookmark
groups are the cornerstones of the system. https://developers.google.com/issue-tracker/ has
more on these concepts.

---
## Data Mapping Callouts

*Notable mappings for Chromium’s migration to Issue Tracker.*

1. All issues with no Priority in Monorail are set to P2 in Issue Tracker.
2. For issues that contain multiple priorities in Monorail, the last set priority is used in
      Issue Tracker. If there is no history of priorities being set, the highest priority
      (P0 being the highest) is selected and applied to the issue.
3. All unassigned issues with the `Fixed` or `Verified` statuses in Monorail are assigned to
      monorail-chromium-migration-no-assignee@google.com in Issue Tracker.
4. All unassigned, open issues in Monorail are marked as `New` in Issue Tracker.
5. All Monorail labels, with some small exceptions, are mapped to the new `Chromium Labels`
      custom field.
6. Issues with multiple components in Monorail have had one primary component selected to be
      the issue's `Component`. Additional components have been added to the `Component Tags`
      custom field.
7. Only hotlists that are owned by a @google.com account or a @chromium.org account and have
      at least one issue were migrated
8. Derived values added from Chromium filter rules are reflected in migrated issues.
9. Monorail stars have been migrated to Issue Tracker +1s and stars. See FAQ above for
    more information on stars, +1s and issue subscriptions.

---

## Known Issues

### Monorail label casing problems

Monorail (mostly) ignored label casing differences (e.g. foobar-baz is mostly the same thing
as FooBar-Baz), but Issue Tracker equivalents (hotlists, custom fields, etc) needed to pick
a consistent casing (e.g. Foobar-Baz). We attempted to automatically decide the most popular
casing in the source data.

### Catch-All Custom Field

Many labels are only used very rarely in Monorail (e.g. the ☂️ emoji is used on 5 issues) and
mapping these labels to individual hotlists would have generated ~60k almost-empty Issue
Tracker hotlists. We have mapped these rare labels to the "Monorail Labels" custom field
to preserve historical data without generating so many hotlists.

### Custom field problems

Custom field casing problems

* Monorail was inconsistent about label casing, and that carried over to some custom fields.

### Hotlist problems

Hotlist casing problems

* Monorail was inconsistent about label casing, and that carried over to some hotlists.

### Intra-issue comment links broken

Monorail’s intra-issue comment links (eg: "comment #c5", "#comment5", "comment 5", "#c5")
will be replaced with a full link to the comment (eg: "https://crbug.com/${project}/${issue}#c5")
in the migrated Issue Tracker issue.

### Changing restricted comments / attachments from non-@google.com accounts
There is a bug where external accounts (accounts using a non-@google.com email) cannot
change the restriction settings on comments / attachments after they have been uploaded.  

As an immediate workaround, if needed, an external account can delete their own
comment / attachment and re-upload it with the expected restriction settings. 
