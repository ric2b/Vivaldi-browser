// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATIONS_STOMP_CONSTANTS_H_
#define SYNC_INVALIDATIONS_STOMP_CONSTANTS_H_

namespace vivaldi {
namespace stomp {
constexpr char kLf = '\n';
constexpr char kCrLf[] = "\r\n";
constexpr char kNul = '\0';

constexpr char kConnectedCommand[] = "CONNECTED";
constexpr char kReceiptCommand[] = "RECEIPT";
constexpr char kMessageCommand[] = "MESSAGE";
constexpr char kErrorCommand[] = "ERROR";
constexpr char kVersionHeader[] = "version";
constexpr char kStompVersion[] = "1.2";
constexpr char kSessionHeader[] = "session";
constexpr char kHeartBeatHeader[] = "heart-beat";
constexpr char kReceiptIdHeader[] = "receipt-id";
constexpr char kExpectedSubscriptionReceipt[] = "sync-subscribed";
constexpr char kContentLengthHeader[] = "content-length";
}
}

#endif  // SYNC_INVALIDATIONS_STOMP_CONSTANTS_H_
