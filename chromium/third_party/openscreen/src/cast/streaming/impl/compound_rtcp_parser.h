// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_COMPOUND_RTCP_PARSER_H_
#define CAST_STREAMING_IMPL_COMPOUND_RTCP_PARSER_H_

#include <chrono>
#include <optional>
#include <vector>

#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/impl/rtcp_common.h"
#include "cast/streaming/impl/rtp_defines.h"
#include "platform/base/span.h"

namespace openscreen::cast {

class RtcpSession;

// Parses compound RTCP packets from a Receiver, invoking client callbacks when
// information of interest to a Sender (in the current process) is encountered.
class CompoundRtcpParser {
 public:
  // Callback interface used while parsing RTCP packets of interest to a Sender.
  // The implementation must take into account:
  //
  //   1. Some/All of the data could be stale, as it only reflects the state of
  //      the Receiver at the time the packet was generated. A significant
  //      amount of time may have passed, depending on how long it took the
  //      packet to reach this local instance over the network.
  //   2. The data shouldn't necessarily be trusted blindly: Some may be
  //      inconsistent (e.g., the same frame being ACKed and NACKed; or a frame
  //      that has not been sent yet is being NACKed). While that would indicate
  //      a badly-behaving Receiver, the Sender should be robust to such things.
  class Client {
   public:
    Client();

    // Called when a Receiver Reference Time Report has been parsed.
    virtual void OnReceiverReferenceTimeAdvanced(
        Clock::time_point reference_time);

    // Called when a Receiver Report with a Report Block has been parsed.
    virtual void OnReceiverReport(const RtcpReportBlock& receiver_report);

    // Called when a group of Cast Receiver frame log messages has been parsed.
    virtual void OnCastReceiverFrameLogMessages(
        std::vector<RtcpReceiverFrameLogMessage> messages);

    // Called when the Receiver has encountered an unrecoverable error in
    // decoding the data. The Sender should provide a key frame as soon as
    // possible.
    virtual void OnReceiverIndicatesPictureLoss();

    // Called when the Receiver indicates that all of the packets for all frames
    // up to and including |frame_id| have been successfully received (or
    // otherwise do not need to be re-transmitted). The |playout_delay| is the
    // Receiver's current end-to-end target playout delay setting, which should
    // reflect any changes the Sender has made by using the "Cast Adaptive
    // Latency Extension" in RTP packets.
    virtual void OnReceiverCheckpoint(FrameId frame_id,
                                      std::chrono::milliseconds playout_delay);

    // Called to indicate the Receiver has successfully received all of the
    // packets for each of the given |acks|. The argument's elements are in
    // monotonically increasing order.
    virtual void OnReceiverHasFrames(std::vector<FrameId> acks);

    // Called to indicate the Receiver is missing certain specific packets for
    // certain specific frames. Any elements where the packet_id is
    // kAllPacketsLost indicates that all the packets are missing for a frame.
    // The argument's elements are in monotonically increasing order.
    virtual void OnReceiverIsMissingPackets(std::vector<PacketNack> nacks);

   protected:
    virtual ~Client();
  };

  // |session| and |client| must be non-null and must outlive the
  // CompoundRtcpParser instance.
  CompoundRtcpParser(RtcpSession& session, Client& client);
  ~CompoundRtcpParser();

  // Parses the packet, invoking the Client callback methods when appropriate.
  // Returns true if the |packet| was well-formed, or false if it was corrupt.
  // Note that none of the Client callback methods will be invoked until a
  // packet is known to be well-formed.
  //
  // |max_feedback_frame_id| is the maximum-valued FrameId that could possibly
  // be ACKnowledged by the Receiver, if there is Cast Feedback in the |packet|.
  // This is needed for expanding truncated frame IDs correctly.
  bool Parse(ByteView packet, FrameId max_feedback_frame_id);

 private:
  // These return true if the input was well-formed, and false if it was
  // invalid/corrupt. The true/false value does NOT indicate whether the data
  // contained within was ignored. Output arguments are only modified if the
  // input contained the relevant field(s).
  bool ParseReceiverReport(ByteView in,
                           int num_report_blocks,
                           std::optional<RtcpReportBlock>& receiver_report);
  bool ParseApplicationDefined(
      RtcpSubtype subtype,
      ByteView in,
      std::vector<RtcpReceiverFrameLogMessage>& messages);
  bool ParseFrameLogMessages(
      ByteView in,
      std::vector<RtcpReceiverFrameLogMessage>& messages);
  bool ParseFeedback(ByteView in,
                     FrameId max_feedback_frame_id,
                     FrameId* checkpoint_frame_id,
                     std::chrono::milliseconds* target_playout_delay,
                     std::vector<FrameId>* received_frames,
                     std::vector<PacketNack>& packet_nacks);
  bool ParseExtendedReports(ByteView in,
                            Clock::time_point& receiver_reference_time);
  bool ParsePictureLossIndicator(ByteView in, bool& picture_loss_indicator);

  RtcpSession& session_;
  Client& client_;

  // Tracks the latest timestamp seen from any Receiver Reference Time Report,
  // and uses this to ignore stale RTCP packets that arrived out-of-order and/or
  // late from the network.
  Clock::time_point latest_receiver_timestamp_;

  // Tracks the last parsed RTP timestamp seen from any Cast receiver frame log.
  RtpTimeTicks latest_frame_log_rtp_timestamp_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_COMPOUND_RTCP_PARSER_H_
