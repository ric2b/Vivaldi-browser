---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
- - /blink/launching-features
  - Launching Features
page_name: webview-deprecations
title: Web Platform Deprecations with Associated WebView APIs
---

## Introduction

Deprecating Web Platform APIs that change the behavior of an [Android WebView
API](https://developer.android.com/reference/android/webkit/WebSettings) can
cause compatibility risks to Android applications. Therefore these deprecations
must follow the Android deprecation process before changes are made on the Web
Platform.

## Background

### WebView

WebView is an Android component that allows developers to embed web content in
their app. Since Android L, it is developed in the Chromium repository and gets
updates through the Play Store. This means that it shares a lot of code with
Chrome, and that a phone running Android M received updates from launch (2015)
until WebView decided to drop support for Android M (2022).

This is different from how most of the Android framework works, where the code
that shipped with a version of Android doesn’t receive updates outside of the
Android release process. Android developers expect behavior changes to only
occur with dessert releases.

WebView provides an Android API through
[android.webkit](https://developer.android.com/reference/android/webkit/package-summary).
Web platform related APIs are likely to be found in the
[WebSettings](https://developer.android.com/reference/android/webkit/WebSettings)
class.

### The Problem

The problem occurs when a change is made to WebView that changes the behavior of
(existing) Android applications. While the WebView team makes changes to WebView
frequently, they are constantly aware of the potential ecosystem consequences.

On the other hand, changes to the shared Chromium code that WebView depends on
are generally made by developers with less awareness of WebView, this is where
unintentional breakages can happen and are hard to catch.

## Android WebView API Deprecation Process

**NOTE:** This process is complementary to the [Web Platform feature deprecation
process](/blink/launching-features/#feature-deprecations).

**Non-Googlers:** Non-Googlers should have a Google Champion to assist with this
deprecation. Many of the tools and processes required for working with the
Android team on deprecations will not be available to developers external to
Google. Reach out to chrome-wp-webview-deprecations@google.com on how to find a
Google Champion if you don't have one already.

**Googlers:** Googlers should follow steps enumerated in the
[internal doc](https://docs.google.com/document/d/1ENnLwl9M87-0CZ8IU9TERRID55Kv_g0kdJ8cE5xwggg/edit?tab=t.0#heading=h.p4kk1qt5tvoq).

## Contact

Contact chrome-wp-webview-deprecations@google.com for any questions on your feature deprecation.
