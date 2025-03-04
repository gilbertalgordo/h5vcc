// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebRequest API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_

namespace extensions {
namespace declarative_webrequest_constants {

// Signals to which WebRequestRulesRegistries are registered.
extern const char kOnRequest[];

// Keys of dictionaries.
extern const char kAgeLowerBoundKey[];
extern const char kAgeUpperBoundKey[];
extern const char kCookieKey[];
extern const char kContentTypeKey[];
extern const char kDomainKey[];
extern const char kExcludeContentTypeKey[];
extern const char kExcludeRequestHeadersKey[];
extern const char kExcludeResponseHeadersKey[];
extern const char kExpiresKey[];
extern const char kFilterKey[];
extern const char kFromKey[];
extern const char kHttpOnlyKey[];
extern const char kInstanceTypeKey[];
extern const char kLowerPriorityThanKey[];
extern const char kMaxAgeKey[];
extern const char kModificationKey[];
extern const char kNameContainsKey[];
extern const char kNameEqualsKey[];
extern const char kNameKey[];
extern const char kNamePrefixKey[];
extern const char kNameSuffixKey[];
extern const char kPathKey[];
extern const char kRedirectUrlKey[];
extern const char kRequestHeadersKey[];
extern const char kResourceTypeKey[];
extern const char kResponseHeadersKey[];
extern const char kSecureKey[];
extern const char kSessionCookieKey[];
extern const char kStagesKey[];
extern const char kThirdPartyKey[];
extern const char kToKey[];
extern const char kUrlKey[];
extern const char kValueContainsKey[];
extern const char kValueEqualsKey[];
extern const char kValueKey[];
extern const char kValuePrefixKey[];
extern const char kValueSuffixKey[];

// Enum string values
extern const char kOnBeforeRequestEnum[];
extern const char kOnBeforeSendHeadersEnum[];
extern const char kOnHeadersReceivedEnum[];
extern const char kOnAuthRequiredEnum[];

// Values of dictionaries, in particular instance types
extern const char kAddRequestCookieType[];
extern const char kAddResponseCookieType[];
extern const char kAddResponseHeaderType[];
extern const char kCancelRequestType[];
extern const char kEditRequestCookieType[];
extern const char kEditResponseCookieType[];
extern const char kIgnoreRulesType[];
extern const char kRedirectByRegExType[];
extern const char kRedirectRequestType[];
extern const char kRedirectToEmptyDocumentType[];
extern const char kRedirectToTransparentImageType[];
extern const char kRemoveRequestCookieType[];
extern const char kRemoveRequestHeaderType[];
extern const char kRemoveResponseCookieType[];
extern const char kRemoveResponseHeaderType[];
extern const char kRequestMatcherType[];
extern const char kSetRequestHeaderType[];

}  // namespace declarative_webrequest_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
