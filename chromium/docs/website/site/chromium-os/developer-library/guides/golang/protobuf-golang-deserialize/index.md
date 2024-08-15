---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: protobuf-golang-deserialize
title: Deserialize objects in golang using protobuf
---

## Problem

I want to deserialize an object in golang using protobuf.
For example, we want to deserialize the feedback report object in golang
using protobuf to check the `feedbackUserCtlConsent` value in it.

## Solution

Key steps:

1.  Add `go package` for the proto.
    For example, the feedback report proto has proto files under

    ```
    ~/chromiumos/src/platform2/feedback/components/feedback/proto
    ```
    We add the following `go package` to all of the files in this directory.

    ``` go
    option go_package = "chromiumos/tast/local/bundles/cros/feedback/proto";
    ```
    The `go package` directory is the place to generate the pb.go files.

2.  Go to correct directory to run command line and generate the pb.go files.
    For example, I want to put the pb.go files in

    ```
    src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/feedback/proto
    ```
    I should go to the directory where the proto files located

    ```
    ~/chromiumos/src/platform2/feedback/components/feedback/proto
    ```
    and run this command line.

    ```go
    protoc -I . --go_out=../../../../../platform/tast-tests/src ./*.proto
    ```

3.  Import

    ```go
    "github.com/golang/protobuf/proto"
    ```
    and go package directory to the test file. For example, I add

    ```go
    "github.com/golang/protobuf/proto"
    ```
    and

    ```go
    fpb "chromiumos/tast/local/bundles/cros/feedback/proto"
    ```
    to the `VerifyFeedbackUserCtlConsentValue` test.

4.  Use `Unmarshal` to deserialize the object.

    ```go
    report := &fpb.ExtensionSubmit{}
    if err = proto.Unmarshal(content, report); err != nil {
      s.Fatal("Failed to parse report: ", err)
    }
    ```

5.  Call the getter function in pb.go files to get the value in the object.

    ```go
    // Loop through the report array to get the feedbackUserCtlConsent value.
    feedbackUserCtlConsentValue := ""
    reportArray := report.GetWebData().GetProductSpecificData()
    for _, element := range reportArray {
      if element.GetKey() == "feedbackUserCtlConsent" {
        feedbackUserCtlConsentValue = element.GetValue()
        break
      }
    }
    ```
## Example CL

*   https://crrev.com/c/3943209
*   https://crrev.com/c/3940029

## References

*   [Protocol Buffer Basics: Go](https://developers.google.com/protocol-buffers/docs/gotutorial#where-to-find-the-example-code){.external}
*   [Go Generated Code](https://developers.google.com/protocol-buffers/docs/reference/go-generated#package){.external}
