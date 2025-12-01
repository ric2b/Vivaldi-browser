---
breadcrumbs:
- - /administrators
  - Documentation for Administrators
page_name: mac-quick-start
title: Mac Quick Start
---

This page describes how to test managing Google Chrome on a Mac network using
Apple’s “defaults” command to test out browser policies. Note that the policy
will be set at the recommended level, not mandatory.

Note that it is not recommended to use this method for
[production](https://support.google.com/chrome/a/answer/9020077]) purposes.

**Examples**

Open Terminal to configure some default Chrome policy values:
```
defaults write com.google.Chrome DefaultPopupsSetting -int 1
defaults write com.google.Chrome PrintingEnabled -bool true
defaults write com.google.Chrome RelaunchNotification -int 2
defaults write com.google.Chrome ChromeVariations -int 2
defaults write com.google.Chrome TotalMemoryLimitMb -int 3072
defaults write com.google.Chrome ShowFullUrlsInAddressBar -bool true
defaults read com.google.Chrome
```

**Restart Browser**

Please navigate to `chrome://policy` and click the "Reload policies" button.
 If you still don't see your policy, you should probably restart it.

**Policies**

To verify that the policies have been configured navigate to `chrome://policy`
and view the policy values.


**Unknown issues**

Please file a bug report at <http://new.crbug.com>, enter
“[Enterprise] .. subject” in the Title, and submit the issue.
