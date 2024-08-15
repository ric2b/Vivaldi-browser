---
breadcrumbs:
- - /issue-tracking
  - Issue Tracking
page_name: requesting-a-component-or-label
title: Requesting a Component or Label
---

[TOC]

## Requesting a Component or Label

Project members can request the creation of new components by simply filing an issue:

*   [Component
            Request](https://issues.chromium.org/p/chromium/issues/entry?template=Component%20Request)
            (Template)

## Component versus Hotlist

*   Components are meant to track work on a specific feature/ function.
*   Hotlists generally are meant to track effort that cuts across multiple components

## Component Guidelines

*   Guideline 1 (Clarity): Component name should be descriptive beyond
            the core project team (i.e. please avoid using non-industry standard
            abbreviations, code words,project names, etc...)
*   Guideline 2 (Permanence): Component names should describe
            features/functions and not team names, code locations, etc..., which
            are more subject to change and make the hierarchy less predictable
            for people triaging issues.
*   Guideline 3 (Specific): Components are meant to explicitly track
            functional work areas. If you are trying to track a Proj(ect) or an
            on-going effort (e.g. Hotlist-Conops), please create your own
            [hotlist](https://developers.google.com/issue-tracker/concepts/hotlists).
*   Guideline 4 (Discoverable/ Predictable): Components should be
            parented where people would logically expect to find them (i.e.
            follow product decomposition when naming versus team decomposition).

## Hotlist Guidelines

* Guideline 1: Proj(ects) hotlists should be used for efforts that have
            a clear start/ end
* Guideline 2: All hotlists have restricted visibility, if you want to open them up, add
            the following permissions:
  * View and append: committers@chromium.org, trusted-tools@chromium.org
  * View only: Public (non-google.com)
* Guideline 3: Multiple hotlists can share the same name, try not to create a hotlist that
            has a name that already exists

### Description Best Practices (for components and labels):

*   (Best practice) Adding abbreviations (e.g. JS, GC, I18N, L10N, WASM, etc...)
            into the description of your components will give you and your users
            a nice shorthand.
*   (Best practice) If there are synonyms not currently in your description, please
          consider adding them.
