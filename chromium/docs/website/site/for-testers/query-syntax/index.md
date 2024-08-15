---
breadcrumbs:
- - /for-testers/
  - For Testers
page_name: issue-query-syntax-for-monorail-users
title: Issue Tracker Query Syntax for Monorail Users
---

<table>
  <tr>
   <td style="color: #ffffff;background-color:#ADD8E6;text-align:center"><strong>Use case</strong>
   </td>
   <td style="color: #ffffff;background-color:#ADD8E6;text-align:center"><strong>Monorail syntax</strong>
   </td>
   <td style="color: #ffffff;background-color:#ADD8E6;text-align:center"><strong>Issue Tracker syntax</strong>
   </td>
  </tr>
  <tr>
   <td rowspan="7">Monorail canned queries
   </td>
   <td>"Open issues"
   </td>
   <td><em>is:open</em>
   </td>
  </tr>
  <tr>
   <td>"Open and owned by me"
   </td>
   <td><em>is:open assignee:me</em>
   </td>
  </tr>
  <tr>
   <td>"Open and reported by me"
   </td>
   <td><em>is:open reporter:me</em>
   </td>
  </tr>
  <tr>
   <td>"Open and starred by me"
   </td>
   <td><em>is:open star:true</em>
   </td>
  </tr>
  <tr>
   <td>"Open with comment by me"
   </td>
   <td><em>is:open commenter:me</em>
   </td>
  </tr>
  <tr>
   <td>"New issues"
   </td>
   <td><em>status:new</em>
   </td>
  </tr>
  <tr>
   <td>"Issues to verify"
   </td>
   <td><em>status:fixed</em>
   </td>
  </tr>
  <tr>
   <td>Full text search
   </td>
   <td><em>any text you want</em>
   </td>
   <td><em>any text you want</em>
   </td>
  </tr>
  <tr>
   <td>Issue title search
   </td>
   <td><em>summary:calculation</em>
   </td>
   <td><em>title:calculation</em>
   </td>
  </tr>
  <tr>
   <td>Issues assigned to someone
   </td>
   <td><em>owner:user@chromium.org</em>
   </td>
   <td><em>assignee:user@chromium.org</em>
   </td>
  </tr>
  <tr>
   <td>Issues assigned to yourself
   </td>
   <td><em>owner:me</em>
   </td>
   <td><em>assignee:me</em>
   </td>
  </tr>
  <tr>
   <td>Issues without an owner
   </td>
   <td><em>-has:owner</em>
   </td>
   <td><em>assignee:none</em>
   </td>
  </tr>
  <tr>
   <td>Issues with an owner
   </td>
   <td><em>has:owner</em>
   </td>
   <td><em>-assignee:none</em>
   </td>
  </tr>
  <tr>
   <td>Issues with a particular label
   </td>
   <td><em>label:security</em>
   </td>
   <td><em>hotlistid:1234</em>
<p>
<p>
* Note: Most Monorail labels will be mapped to custom fields but some may be mapped to other
concepts like hotlists.
   </td>
  </tr>
  <tr>
   <td>Issues with a particular priority
   </td>
   <td><em>label:Priority-High</em>
<p>
<em>Priority:High</em>
   </td>
   <td><em>priority:p1</em>
   </td>
  </tr>
  <tr>
   <td>Search for issues in a component, including child components
   </td>
   <td><em>component:UI</em>
   </td>
   <td><em>componentid:1234+</em>
   </td>
  </tr>
  <tr>
   <td>Search for issues in a component, excluding child components
   </td>
   <td><em>component=UI</em>
   </td>
   <td><em>componentid:1234</em>
   </td>
  </tr>
  <tr>
   <td>Negating a search
   </td>
   <td><em>-component=UI</em>
   </td>
   <td><em>-componentid:1234</em>
   </td>
  </tr>
  <tr>
   <td>Combining search operators
   </td>
   <td><em>status!=New owner:me component:UI</em>
   </td>
   <td><em>-status:new assignee:me componentid:1234+</em>
   </td>
  </tr>
  <tr>
   <td>Searching for issues in a hotlist
   </td>
   <td><em>hotlist=username@domain:hotlistname</em>
   </td>
   <td><em>hotlistid:1234</em>
   </td>
  </tr>
  <tr>
   <td>Find issues with a component (other than the Tracker root component) set
   </td>
   <td><em>has:component</em>
   </td>
   <td><em>-componentid:1363614 componentid:1363614+</em>
<p>
<p>
* Note: Issue Tracker does not have a concept of "componentless" issues, so we are treating issues attached directly to the root Chromium component (and not its children) as the equivalent of "componentless" issues in Monorail.
   </td>
  </tr>
  <tr>
   <td>Find issues without a component
   </td>
   <td><em>-has:component</em>
   </td>
   <td><em>componentid:1363614</em>
   </td>
  </tr>
  <tr>
   <td>Find issues inside the Chromium Tracker (from outside the Chromium Tracker)
   </td>
   <td>N/A, outside of Tracker search is not supported in Monorail
   </td>
   <td><em> trackerid:157</em>
   </td>
  </tr>
  <tr>
   <td>Search by multiple different possible priority values
   </td>
   <td><em>Priority:High,Medium</em>
   </td>
   <td><em>priority:(p1|p2)</em>
   </td>
  </tr>
  <tr>
   <td>OR syntax
   </td>
   <td><em>Priority:High OR -has:owner</em>
   </td>
   <td><em>priority:p1 OR -assignee:none</em>
   </td>
  </tr>
  <tr>
   <td>Complex query example
   </td>
   <td><em>Pri:0,1 (status:Untriaged OR -has:owner)</em>
   </td>
   <td><em>priority:(p0|p1) (status:New OR assignee:none)</em>
   </td>
  </tr>
  <tr>
   <td>Custom field value search
   </td>
   <td><em>Milestone=2009</em>
   </td>
   <td><em>customfield1234:5678</em>
<p>
<p>
* Note: This mapping assumes "Milestone" is a custom field. Some custom fields in Monorail may be mapped differently.
   </td>
  </tr>
  <tr>
   <td>Find issues you've starred
   </td>
   <td><em>is:starred</em>
   </td>
   <td><em>star:true</em>
   </td>
  </tr>
  <tr>
   <td>Find issues with a certain number of stars
   </td>
   <td><em>stars>10</em>
   </td>
   <td><em>votecount>10</em>
<p>
<p>
* Note: Issue Tracker implements stars and +1 votes as separate concepts. On public issues, Issue Trackers allows users to both +1 and star an issue with one click, creating an experience similar to starring in Monorail.
   </td>
  </tr>
  <tr>
   <td>Find issues with attachments
   </td>
   <td><em>has:attachments</em>
   </td>
   <td>N/A, not supported
   </td>
  </tr>
  <tr>
   <td>Find attachment by name (partial)
   </td>
   <td><em>attachment:screenshot</em>
   </td>
   <td><em>attachment:screenshot</em>
   </td>
  </tr>
  <tr>
   <td>Find attachment by file extension
   </td>
   <td><em>attachment:png</em>
   </td>
   <td><em>attachment:png</em>
   </td>
  </tr>
  <tr>
   <td>Find issues with a certain number of attachments
   </td>
   <td><em>attachments>10</em>
   </td>
   <td>N/A, not supported
   </td>
  </tr>
  <tr>
   <td>Find issues opened after a certain date
   </td>
   <td><em>opened>2009/4/1</em>
   </td>
   <td><em>created>2009-04-01</em>
   </td>
  </tr>
  <tr>
   <td>Find issues modified in the last 20 days
   </td>
   <td><em>modified&lt;today-20</em>
   </td>
   <td><em>modified:20d</em>
   </td>
  </tr>
  <tr>
   <td>Find issues modified by the owner in the last 20 days
   </td>
   <td><em>ownermodified>today-20</em>
   </td>
   <td>N/A, not supported
   </td>
  </tr>
</table>

