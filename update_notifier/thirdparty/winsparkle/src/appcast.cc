/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include "update_notifier/thirdparty/winsparkle/src/appcast.h"

#include "update_notifier/thirdparty/winsparkle/src/error.h"

#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"

#include <algorithm>
#include <vector>

#include <WebServices.h>
#include <windows.h>

#pragma comment(lib, "WebServices.lib")

namespace vivaldi_update_notifier {

/*--------------------------------------------------------------------------*
                                XML parsing
 *--------------------------------------------------------------------------*/

namespace {

const char kSparkleNS[] = "http://www.andymatuschak.org/xml-namespaces/sparkle";

struct NoNamespaceName {
  const char* name = nullptr;
};
struct SparkleNamespacedName {
  const char* name = nullptr;
};

const NoNamespaceName kNodeChannel{"channel"};
const NoNamespaceName kNodeItem{"item"};
const SparkleNamespacedName kNodeRelNotes{"releaseNotesLink"};
const NoNamespaceName kNodeTitle{"title"};
const NoNamespaceName kNodeDescription{"description"};
const NoNamespaceName kNodeLink{"link"};
const NoNamespaceName kNodeEnclosure{"enclosure"};
const SparkleNamespacedName kNodeVersion{"version"};
const SparkleNamespacedName kNodeMinimumSystemVersion{"minimumSystemVersion"};
const SparkleNamespacedName kNodeDeltas{"deltas"};

const NoNamespaceName kAttrUrl{"url"};
const SparkleNamespacedName kAttrVersion = kNodeVersion;
const SparkleNamespacedName kAttrDeltaFrom{"deltaFrom"};
const SparkleNamespacedName kAttrOs{"os"};

const char kOsMarker[] = "windows";

// Arbitrary limit for XML complexity. It is used to prevent XML nesting depth
// overflow.
constexpr int kMaxXmlNestingDepth = 1000;

std::string WideToUTF8(const wchar_t* s, size_t length) {
  return base::WideToUTF8(std::wstring_view(s, length));
}

// context data for the parser
struct ContextData {
  ContextData() {
    // Create an error object for storing rich error information
    HRESULT hr = WsCreateError(NULL, 0, &ws_error);
    if (FAILED(hr)) {
      error.set(Error::kFormat, "Failed to created WS_ERROR object");
      return;
    }

    // Create an XML reader
    hr = WsCreateReader(NULL, 0, &xml_reader, ws_error);
    if (FAILED(hr)) {
      OnWsError(hr);
      return;
    }
  }

  ~ContextData() {
    if (ws_error) {
      WsFreeError(ws_error);
    }
  }

  Error error;
  WS_ERROR* ws_error = nullptr;
  WS_XML_READER* xml_reader = nullptr;

  // XML depth of various elements if inside those or 0 if outside the element.
  int in_channel = 0;
  int in_item = 0;
  int in_relnotes = 0;
  int in_title = 0;
  int in_description = 0;
  int in_link = 0;
  int in_deltas = 0;
  int in_version = 0;
  int in_min_os_version = 0;

  // Ignore rest of XML while still validating its syntax.
  bool parsing_done = false;

  // parsed <item>s
  std::vector<Appcast> items;

  void OnWsError(HRESULT error_code) {
    if (error_code == E_INVALIDARG || error_code == WS_E_INVALID_OPERATION) {
      // Correct use of the APIs should never generate these errors
      error.set(Error::kFormat, "Broken usage of WsXml API");
      return;
    }

    if (!ws_error) {
      error.set(Error::kFormat, "Unknown WsXml error");
      return;
    }

    ULONG error_count;
    HRESULT hr = WsGetErrorProperty(ws_error, WS_ERROR_PROPERTY_STRING_COUNT,
                                    &error_count, sizeof(error_count));
    if (FAILED(hr)) {
      error.set(Error::kFormat, "Failed WsGetErrorProperty(), error_code=" +
                                    std::to_string(error_code));
      return;
    }

    std::string message;
    for (ULONG i = 0; i < error_count; i++) {
      if (i != 0) {
        message += '\n';
      }
      WS_STRING ws_message;
      hr = WsGetErrorString(ws_error, i, &ws_message);
      if (FAILED(hr)) {
        message += "Failed WsGetErrorString()";
      } else {
        message.append(WideToUTF8(ws_message.chars, ws_message.length));
      }
    }
    error.set(Error::kFormat, message);
  }
};

bool is_windows_version_acceptable(const Appcast& item) {
  // TODO(igor@vivaldi.com): Verify the version format.
  if (!item.IsValid())
    return false;
  if (item.MinOSVersion.empty())
    return true;

  OSVERSIONINFOEXW osvi = {sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0};
  DWORDLONG const dwlConditionMask = VerSetConditionMask(
      VerSetConditionMask(
          VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL),
          VER_MINORVERSION, VER_GREATER_EQUAL),
      VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

  (void)sscanf(item.MinOSVersion.c_str(), "%lu.%lu.%hu", &osvi.dwMajorVersion,
               &osvi.dwMinorVersion, &osvi.wServicePackMajor);

  return VerifyVersionInfoW(
             &osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
             dwlConditionMask) != FALSE;
}

template <typename NodeOrAttribute>
bool HasName(NodeOrAttribute* x, NoNamespaceName name) {
  // Follow original winSparkle code and ignore namespace check rather than
  // insisting on an empty namespace.
  size_t n = strlen(name.name);
  if (n != x->localName->length)
    return false;
  if (0 != memcmp(x->localName->bytes, name.name, n))
    return false;
  return true;
}

template <typename NodeOrAttribute>
bool HasName(NodeOrAttribute* x, SparkleNamespacedName name) {
  size_t n = strlen(kSparkleNS);
  if (n != x->ns->length)
    return false;
  if (0 != memcmp(x->ns->bytes, kSparkleNS, n))
    return false;
  n = strlen(name.name);
  if (n != x->localName->length)
    return false;
  if (0 != memcmp(x->localName->bytes, name.name, n))
    return false;
  return true;
}

std::string WsStringToXml(const WS_XML_STRING& ws_string) {
  const char* s = reinterpret_cast<char*>(ws_string.bytes);
  return std::string(s, ws_string.length);
}

std::string WsTextToString(const WS_XML_TEXT* text) {
  switch (text->textType) {
    case WS_XML_TEXT_TYPE_UTF8: {
      const WS_XML_UTF8_TEXT* utf8Text = (const WS_XML_UTF8_TEXT*)text;
      return WsStringToXml(utf8Text->value);
    }
    case WS_XML_TEXT_TYPE_UTF16: {
      const WS_XML_UTF16_TEXT* utf16Text = (const WS_XML_UTF16_TEXT*)text;
      return WideToUTF8(reinterpret_cast<const WCHAR*>(utf16Text->bytes),
                        utf16Text->byteCount / sizeof(WCHAR));
    }
#if 0
      // TODO(igor@vivaldi.com): Confirm that the rest of enum constatnts are
      // not used when parsing XML representing as a string.
    case WS_XML_TEXT_TYPE_BASE64: {
      const WS_XML_BASE64_TEXT* base64Text = (const WS_XML_BASE64_TEXT*)text;
      return implement_base_64_decode();
    }
    case WS_XML_TEXT_TYPE_BOOL: {
      const WS_XML_BOOL_TEXT* boolText = (const WS_XML_BOOL_TEXT*)text;
      return boolText->value ? "true" : "false";
    }
    case WS_XML_TEXT_TYPE_INT32: {
      const WS_XML_INT32_TEXT* int32Text = (const WS_XML_INT32_TEXT*)text;
      return std::to_string(int32Text->value);
    }
    case WS_XML_TEXT_TYPE_INT64: {
      const WS_XML_INT64_TEXT* int64Text = (const WS_XML_INT64_TEXT*)text;
      return std::to_string(int64Text->value);
    }
    case WS_XML_TEXT_TYPE_UINT64: {
      const WS_XML_UINT64_TEXT* uint64Text = (const WS_XML_UINT64_TEXT*)text;
      return std::to_string(uint64Text->value);
    }
    case WS_XML_TEXT_TYPE_FLOAT: {
      const WS_XML_FLOAT_TEXT* floatText = (const WS_XML_FLOAT_TEXT*)text;
      return std::to_string(floatText->value);
    }
    case WS_XML_TEXT_TYPE_DOUBLE: {
      const WS_XML_DOUBLE_TEXT* doubleText = (const WS_XML_DOUBLE_TEXT*)text;
      return std::to_string(doubleText->value);
    }
    case WS_XML_TEXT_TYPE_DECIMAL: {
      const WS_XML_DECIMAL_TEXT* decimalText = (const WS_XML_DECIMAL_TEXT*)text;
      //printf("WS_XML_TEXT_TYPE_DECIMAL(value={%x %x %x, %I64x})",
      //       dec->wReserved, dec->signscale, dec->Hi32, dec->Lo64);
      return implement_decimal_double_conversion();
    }
    case WS_XML_TEXT_TYPE_GUID: {
      WS_XML_GUID_TEXT* guidText = (WS_XML_GUID_TEXT*)text;
      RPC_WSTR w;
      if (UuidToString(&guidText->value, &w) != RPC_S_OK)
        break;
      std::string s = WideToUTF8(w);
      RpcStringFree(&w);
      return s;
    }
    case WS_XML_TEXT_TYPE_UNIQUE_ID: {
      WS_XML_UNIQUE_ID_TEXT* uniqueIdText = (WS_XML_UNIQUE_ID_TEXT*)text;
      RPC_WSTR w;
      if (UuidToString(&uniqueIdText->value, &w) != RPC_S_OK)
        break;
      std::string s = WideToUTF8(w);
      RpcStringFree(&w);
      return s;
    }
    case WS_XML_TEXT_TYPE_DATETIME: {
      const WS_XML_DATETIME_TEXT* dateTimeText =
          (const WS_XML_DATETIME_TEXT*)text;
      WS_DATETIME value = dateTimeText->value;
      // printf("WS_XML_DATETIME_TEXT(ticks='%I64u',format='%d')", value.ticks,
      // value.format);
      return implement_this();
    }
    case WS_XML_TEXT_TYPE_TIMESPAN: {
      const WS_XML_TIMESPAN_TEXT* timeSpanText =
          (const WS_XML_TIMESPAN_TEXT*)text;
      //printf("WS_XML_TIMESPAN_TEXT(value='%I64u')", timeSpanText->value.ticks);
      return implement_this();
    }
    case WS_XML_TEXT_TYPE_QNAME: {
      const WS_XML_QNAME_TEXT* qnameText = (const WS_XML_QNAME_TEXT*)text;
      // PrintString(qnameText->prefix);
      // printf("', localName='");
      // PrintString(qnameText->localName);
      // PrintString(qnameText->ns);
      return implement_this();
    }
    case WS_XML_TEXT_TYPE_LIST: {
      const WS_XML_LIST_TEXT* listText = (const WS_XML_LIST_TEXT*)text;
      std::string s;
      for (ULONG i = 0; i < listText->itemCount; i++) {
        if (i > 0) {
          s == ' ';
        }
        s += WsTextToString(listText->items[i]);
      }
      return s;
    }
#endif
    default:
      break;
  }

  // TODO(igor@vivaldi.com): Consider reporting or logging an error.
  return std::string();
}

base::Version GetAsVersion(const WS_XML_ATTRIBUTE* attr) {
  std::string attr_value = WsTextToString(attr->value);
  return base::Version(attr_value);
}

void OnStartElement(int depth,
                    const WS_XML_ELEMENT_NODE* node,
                    ContextData& ctxt) {
  if (HasName(node, kNodeChannel)) {
    ctxt.in_channel = depth;
  } else if (ctxt.in_channel && HasName(node, kNodeItem)) {
    ctxt.in_item = depth;
    ctxt.items.emplace_back();
  } else if (ctxt.in_item) {
    if (HasName(node, kNodeRelNotes)) {
      ctxt.in_relnotes = depth;
    } else if (HasName(node, kNodeTitle)) {
      ctxt.in_title = depth;
    } else if (HasName(node, kNodeDescription)) {
      ctxt.in_description = depth;
    } else if (HasName(node, kNodeLink)) {
      ctxt.in_link = depth;
    } else if (HasName(node, kNodeVersion)) {
      ctxt.in_version = depth;
    } else if (HasName(node, kNodeMinimumSystemVersion)) {
      ctxt.in_min_os_version = depth;
    } else if (HasName(node, kNodeDeltas)) {
      ctxt.in_deltas = depth;
    } else if (HasName(node, kNodeEnclosure)) {
      Appcast& item = ctxt.items.back();
      Delta* delta = nullptr;
      if (ctxt.in_deltas) {
        item.Deltas.emplace_back();
        delta = &item.Deltas.back();
      }
      for (size_t i = 0; i != node->attributeCount; ++i) {
        WS_XML_ATTRIBUTE* attr = node->attributes[i];
        if (attr->isXmlNs)
          continue;
        if (HasName(attr, kAttrUrl)) {
          GURL* url = &item.DownloadURL;
          if (delta) {
            url = &delta->DownloadURL;
          }
          *url = GURL(WsTextToString(attr->value));
          continue;
        }
        if (delta) {
          if (HasName(attr, kAttrDeltaFrom)) {
            delta->DeltaFrom = GetAsVersion(attr);
          }
        } else {
          if (HasName(attr, kAttrVersion)) {
            item.Version = GetAsVersion(attr);
          } else if (HasName(attr, kAttrOs)) {
            item.Os = WsTextToString(attr->value);
          }
        }
      }
    }
  }
}

/**
 * Returns true if item os is exactly "windows"
 *   or if item is "windows-x64" on 64bit
 *   or if item is "windows-x86" on 32bit
 *   and is above minimum version
 */
bool is_suitable_windows_item(const Appcast& item) {
  if (!is_windows_version_acceptable(item))
    return false;

  if (item.Os == kOsMarker)
    return true;

  size_t n = strlen(kOsMarker);
  if (item.Os.compare(0, n, kOsMarker) != 0)
    return false;

    // Check suffix for matching bitness
#ifdef _WIN64
  return item.Os.compare(n, std::string::npos, "-x64") == 0;
#else
  return item.Os.compare(n, std::string::npos, "-x86") == 0;
#endif
}

void OnEndElement(int depth, std::string text, ContextData& ctxt) {
  // Process text elements.
  if (depth == ctxt.in_relnotes) {
    ctxt.in_relnotes = 0;
    GURL url(std::move(text));
    if (url.is_valid()) {
      ctxt.items.back().ReleaseNotesURL = std::move(url);
    }
  }
  if (depth == ctxt.in_title) {
    ctxt.in_title = 0;
    ctxt.items.back().Title = std::move(text);
  }
  if (depth == ctxt.in_description) {
    ctxt.in_description = 0;
    ctxt.items.back().Description = std::move(text);
  }
  if (depth == ctxt.in_min_os_version) {
    ctxt.in_min_os_version = 0;
    ctxt.items.back().MinOSVersion = std::move(text);
  }
  if (depth == ctxt.in_link) {
    ctxt.in_link = 0;
    ctxt.items.back().WebBrowserURL = std::move(text);
  }
  if (depth == ctxt.in_version) {
    ctxt.in_version = 0;
    ctxt.items.back().Version = base::Version(text);
  }

  // Process structured elements.
  if (depth == ctxt.in_deltas) {
    ctxt.in_deltas = 0;
  }
  if (depth == ctxt.in_item) {
    ctxt.in_item = 0;
    if (is_suitable_windows_item(ctxt.items.back()))
      ctxt.parsing_done = true;
  }
  if (depth == ctxt.in_channel) {
    ctxt.in_channel = 0;
    // we've reached the end of <channel> element,
    // so we stop parsing
    ctxt.parsing_done = true;
  }
}

void ParseAppcast(const std::string& xml_source, ContextData& ctxt) {
  const BYTE* bytes = (BYTE*)xml_source.data();
  ULONG byteCount = (ULONG)xml_source.length();

  // Setup the source input
  WS_XML_READER_BUFFER_INPUT bufferInput;
  ZeroMemory(&bufferInput, sizeof(bufferInput));
  bufferInput.input.inputType = WS_XML_READER_INPUT_TYPE_BUFFER;
  bufferInput.encodedData = const_cast<BYTE*>(bytes);
  bufferInput.encodedDataSize = byteCount;

  // Setup the source encoding
  WS_XML_READER_TEXT_ENCODING textEncoding;
  ZeroMemory(&textEncoding, sizeof(textEncoding));
  textEncoding.encoding.encodingType = WS_XML_READER_ENCODING_TYPE_TEXT;
  textEncoding.charSet = WS_CHARSET_AUTO;

  // Setup the reader
  HRESULT hr = WsSetInput(ctxt.xml_reader, &textEncoding.encoding,
                          &bufferInput.input, NULL, 0, ctxt.ws_error);
  if (FAILED(hr)) {
    ctxt.OnWsError(hr);
    return;
  }

  int depth = 0;
  std::string text_buffer;
  for (;;) {
    // Get the current node of the reader
    const WS_XML_NODE* node;
    hr = WsGetReaderNode(ctxt.xml_reader, &node, ctxt.ws_error);
    if (FAILED(hr)) {
      ctxt.OnWsError(hr);
      return;
    }

    switch (node->nodeType) {
      case WS_XML_NODE_TYPE_ELEMENT: {
        const WS_XML_ELEMENT_NODE* element_node =
            (const WS_XML_ELEMENT_NODE*)node;
        if (depth == kMaxXmlNestingDepth) {
          ctxt.error.set(Error::kFormat, "Too deeply nested XML");
          return;
        }
        ++depth;
        OnStartElement(depth, element_node, ctxt);
        text_buffer.clear();
        break;
      }
      case WS_XML_NODE_TYPE_END_ELEMENT: {
        if (depth == 0) {
          ctxt.error.set(Error::kFormat, "XML end element without start");
          return;
        }
        OnEndElement(depth, std::move(text_buffer), ctxt);
        --depth;
        break;
      }
      case WS_XML_NODE_TYPE_TEXT: {
        const WS_XML_TEXT_NODE* text_node = (const WS_XML_TEXT_NODE*)node;
        text_buffer += WsTextToString(text_node->text);
        break;
      }

      case WS_XML_NODE_TYPE_COMMENT:
      case WS_XML_NODE_TYPE_CDATA:
      case WS_XML_NODE_TYPE_END_CDATA:
      case WS_XML_NODE_TYPE_EOF:
      case WS_XML_NODE_TYPE_BOF:
        break;
    }
    if (ctxt.error)
      return;
    // See if we've reached the end of the document
    if (node->nodeType == WS_XML_NODE_TYPE_EOF) {
      break;
    }
    // Advance the reader
    hr = WsReadNode(ctxt.xml_reader, ctxt.ws_error);
    if (FAILED(hr)) {
      ctxt.OnWsError(hr);
      return;
    }
  }
}

}  // anonymous namespace

/*--------------------------------------------------------------------------*
                               Appcast class
 *--------------------------------------------------------------------------*/
Delta::Delta() = default;
Delta::Delta(const Delta&) = default;
Delta::~Delta() = default;

Appcast::Appcast() = default;
Appcast::Appcast(const Appcast&) = default;
Appcast::~Appcast() = default;

/*static*/
std::unique_ptr<Appcast> Appcast::Load(const std::string& xml, Error& error) {
  if (error)
    return nullptr;

  ContextData ctxt;
  ParseAppcast(xml, ctxt);
  if (ctxt.error) {
    error = std::move(ctxt.error);
    return nullptr;
  }

  // Search for first <item> which specifies with the attribute sparkle:os set
  // to "windows" or "windows-x64"/"windows-x86" based on this modules bitness
  // and meets the minimum os version, if set. If none, use the first item that
  // meets the minimum os version, if set.
  std::vector<Appcast>::iterator it = std::find_if(
      ctxt.items.begin(), ctxt.items.end(), is_suitable_windows_item);
  if (it != ctxt.items.end())
    return std::make_unique<Appcast>(std::move(*it));

  it = std::find_if(ctxt.items.begin(), ctxt.items.end(),
                    is_windows_version_acceptable);
  if (it != ctxt.items.end())
    return std::make_unique<Appcast>(std::move(*it));

  error.set(Error::kFormat, "XML update file with no applicable updates");
  return nullptr;
}

}  // namespace vivaldi_update_notifier
