---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides > WebUI
page_name: webui-untrusted
title: Creating and translating Strings
---

This document describes the process of finalizing strings with UX, implementing
them, and working with the translation console for any issues.

## Finalize Strings with UX Writing

For user-facing text, the strings need to be finalized and approved by ChromeOS
UX Writing (go/cros-uxw). Schedule office hours with the appropriate UX writer
and your own team's UX contact, then make a copy of go/cros-string-doc. If the
string you want finalized is new, provide a mock of where the text would show
up. If the string already exists, take a screenshot and provide the current
string.

During office hours you will workshop the string, provide additional information
and context, and finalize strings with the writer(s). Once strings are finalized
you can move forward with implementing them into your surfaces. Note that the
writers may follow-up after finalizing a string; this is fine as strings can
still be updated after implementation.

## Implementing Strings into Chromium

### Strings Locations

Chromium manages translated strings in .grd(p) files. Once you have finalized
strings you need to add them to a corresponding .grd(p) file in English. All
strings should be added and submitted at the latest one day before branch cut in
order to allow enough time for translations. Best practice is to have your
strings submitted by feature freeze. Examples of where you can find some .grd(p)
locations:

-   [//ash/ash_strings.grd](https://source.chromium.org/chromium/chromium/src/+/main:ash/ash_strings.grd)
-   [//chromeos/chromeos_strings.grd](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/chromeos_strings.grd)
-   [Settings Strings](https://source.chromium.org/chromium/chromium/src/+/main:chrome/app/os_settings_strings.grdp)
-   [Nearby Share Strings](https://source.chromium.org/chromium/chromium/src/+/main:chrome/app/nearby_share_strings.grdp)
-   [Printing Strings](https://source.chromium.org/chromium/chromium/src/+/main:chrome/app/printing_strings.grdp)

If you're adding a new entry as opposed to modifying an existing one, use this
format:

```xml
<message name="IDS_YOUR_STRING_TOKEN"
  meaning="Used to disambiguate your string"
  desc="Write a good description to aid translators and other developers.">
  This is my finalized string
</message>

<!-- Another example with a parameter -->
<message name="IDS_SETUP_DEVICE"
  meaning="Set up your device in this context is setting up the ChromeOS device
  (e.g., Chromebook, Chromebox) the user is currently using"
  desc="Text shown over a button to set up your device.">
  Set up your <ph name="DEVICE_NAME">$1<ex>Chromebox</ex></ph>
</message>

<message name="IDS_SETUP_PERIPHERAL"
  meaning="Set up your device in this context is setting up an external device
  (e.g., printer)"
  desc="Text shown over a button to set up your phone through your Chromebook.">
  Set up your <ph name="DEVICE_NAME">$1<ex>printer</ex></ph>
</message>
```

Strings that are unique often don't have a `meaning` tag. It is encouraged to
re-use strings where appropriate in order to not increase the size of the
binary. However, some strings that appear the same but have a different meaning
than each other **must** use the `meaning` tag to disambiguate. Messages with
different meaning tags will be translated separately; adding or modifying a
meaning to an existing string will trigger a retranslation.

[Style notes](https://source.chromium.org/chromium/chromium/src/+/main:ash/ash_strings.grd;l=6-13;drc=e6c06895cfd887729583db3e0c63d2686f243ab1):

-   Most strings are "Sentence case" with the first word capitalized and other
    words not capitalized.
-   Specific features may be capitalized (e.g. "Bluetooth", "Nearby Share") even
    if they appear in the middle of a string.
-   Most strings do not end in a period (e.g. "Show settings").
-   Multi-phrase strings that contain a period in the middle also have a period
    at the end (e.g. "Show Bluetooth settings. Bluetooth is on.").

Note: String token names must start with `IDS_`. For guidance on writing useful
descriptions and meanings, see
[here](https://www.chromium.org/developers/design-documents/ui-localization/#give-context-to-translators).
Additionally, you can always reach out to the UX writers for any guidance with
writing descriptions and meanings.

### Using Strings in Code

Strings added to .grd(p) files can then be called by C++ by including the
appropriate header. Near the top of most .grd(p) files will have an xml tag
`output filename` with an `rc_header` type to denote the header to include. Note
that .grd(p) files that do not have the tag derive it from their parents.
Examples:

```
//ash/ash_strings.grd -> //ash/strings/grit/ash_strings.h
//chromeos/chromeos_strings.grd -> //chromeos/strings/grit/chromeos_strings.h

// All grdp files in chrome/app/ are included in the parent grdp:
// `generated_resources.grd`
chrome/app/*.grdp -> chrome/grit/generated_resources.h
```

Use the
[l10n_util class](https://source.chromium.org/chromium/chromium/src/+/main:ui/base/l10n/l10n_util.h)
to extract the translation using the tokens defined in the .grd(p) files.

```cpp
// In this example your string can be found in ash/ash_strings.grd
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

...

button1->SetTooltipText(l10n_util::GetStringUTF16(IDS_YOUR_STRING_TOKEN));
button2->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
    IDS_SETUP_DEVICE, ui::GetChromeOSDeviceName()));
```

### Localized Strings in WebUI

Similar to using strings in C++ code, you can access localized strings in WebUI
using the string ID token. While there are a few methods for doing this, the
easiest way of achieving this is with the `I18nBehavior` mixin.

1.  **Add the strings in the WebUI controller**

    Like a server serves a webpage in the web, a WebUI controller serves a WebUI
    on ChromeOS. The controller is able to provide context to the page being
    served. `WebUIDataSource` has `AddLocalizedString`,
    [a method](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_ui_data_source.h;l=60)
    which provides the WebUI with a string that can be accessed inside the web
    source. Depending on the component, this will be handled differently. It is
    likely that there will be an array of type `LocalizedString`, mapping the
    GRD ID to an ID that can be used in the webpage. For example,
    `multidevice_setup_localized_string_provider.cc`
    [maps GRD IDs to an ID](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_localized_strings_provider.cc;l=37-72)
    that can be used inside the WebUI:

    ```c++
    constexpr webui::LocalizedString kLocalizedStringsWithoutPlaceholders[] = {
        {"accept", IDS_MULTIDEVICE_SETUP_ACCEPT_LABEL},
        {"back", IDS_MULTIDEVICE_SETUP_BACK_LABEL},
        {"cancel", IDS_CANCEL},
        {"done", IDS_DONE},
        {"noThanks", IDS_NO_THANKS},
        {"passwordPageHeader", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_HEADER},
        {"enterPassword", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_ENTER_PASSWORD_LABEL},
        {"wrongPassword", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_WRONG_PASSWORD_LABEL},
        ...
    };
    ```

    The controller also has logic to add the to the strings to the
    `Context::WebUIDataSource` using the `WebUIDataSource::AddLocalizedString`
    method. For example:

    ```c++
    html_source->AddLocalizedStrings(kLocalizedStringsWithoutPlaceholders);
    ```

    Additional work may be required if you need to use placeholders.
    Placeholders allow you to swap out parts of the string. A common use-case is
    to insert the correct device type in a string (ie, Chromebook, Chromebox,
    etc). The implementation of this depends on the component. The
    [multidevice setup uses](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_localized_strings_provider.cc;l=157-161)
    `LocalizedValuesBuilder::AddF`:

    ```c++
    builder->AddF("startSetupPageMessage",
                  IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MESSAGE,
                  ui::GetChromeOSDeviceName(), kFootnoteMarker,
                  base::UTF8ToUTF16(
                      GetBoardSpecificBetterTogetherSuiteLearnMoreUrl().spec()));
    ```

2.  **Accessing localized strings in WebUI.**

    NOTE: In Polymer, a mixin allows components to inherit behavior without
    explicitly adding a common superclass.

    In your web component, set the `I18nBehavior` interface as a mixin:

    ```ts
    const SettingsMultidevicePageElementBase = mixinBehaviors(
        [
          ...
          I18nBehavior,
        ],
        PolymerElement);

    /** @polymer */
    class SettingsMultidevicePageElement extends
        SettingsMultidevicePageElementBase { ... }
    ```

    Then, to access localized strings, pass the token to the `this.i18n`
    [method](https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/common/resources/i18n_behavior.d.ts;l=10):

    ```ts
    getLabelText_() {
      return this.pageContentData.hostDeviceName ||
          this.i18n('passwordPageHeader');
    }
    ```

    Alternatively, if you do not need additional logic and simply want to use
    the string in an element inside the Polymer component:

    ```html
    <cr-button>
      $i18n{passwordPageHeader}
    </cr-button>
    ```

### Taking/Uploading a Screenshot and Including the Hash to Your CL

Once you've implemented your strings, you need to take a screenshot and upload
it to Google Cloud Storage so translators can view it to gain more context.
Follow the steps at https://g.co/chrome/translation.

Note: You must do this step even if you're just modifying a string as
translators will still need to re-translate them despite the token already
existing in the system.

Some example CLs:

-   https://crrev.com/c/3703190
-   https://crrev.com/c/3524392
-   https://crrev.com/c/3718166

## Automed Translations and Translation Console

Once you submit your CL, automated translation scripts extract the strings from
Chromium and upload them to the Translation Console (go/transconsole). From
there, vendors translate the strings into several languages which get uploaded
back into Chromium. The process takes approximately two weeks after you upload
your CL, but can take longer depending on volume of strings, vendor capacity,
etc.. If translators need more context or guidance on a string, they will file
bugs and contact you.

Fore more in-depth information regarding the timing of strings being translated,
working with strings before branch cutoff, and merge process for strings, see
go/cr-strings-process.

## Other Resources

-   [Adding a new .grd(p) file](https://www.chromium.org/developers/design-documents/ui-localization/#add-a-new-grdp-file):
    should rarely be done.
-   go/chrome-translation-process: More detailed look of the internals of how
    strings are translated/localized.

