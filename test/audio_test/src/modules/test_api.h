/*
* Wire
* Copyright (C) 2016 Wire Swiss GmbH
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// This class sends all its packet straight to the provided RtpRtcp module.
// with optional packet loss.
class LoopBackTransport : public webrtc::Transport {
 public:
  LoopBackTransport()
    : _count(0),
      _packetLoss(0),
      rtp_payload_registry_(NULL),
      rtp_receiver_(NULL),
      _rtpRtcpModule(NULL) {
  }
  void SetSendModule(RtpRtcp* rtpRtcpModule,
                     RTPPayloadRegistry* payload_registry,
                     RtpReceiver* receiver,
                     ReceiveStatistics* receive_statistics) {
    _rtpRtcpModule = rtpRtcpModule;
    rtp_payload_registry_ = payload_registry;
    rtp_receiver_ = receiver;
    receive_statistics_ = receive_statistics;
  }
  void DropEveryNthPacket(int n) {
    _packetLoss = n;
  }
  virtual int SendPacket(int channel, const void *data, int len) {
    _count++;
    if (_packetLoss > 0) {
      if ((_count % _packetLoss) == 0) {
        return len;
      }
    }
    RTPHeader header;
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    if (!parser->Parse(static_cast<const uint8_t*>(data), len, &header)) {
      return -1;
    }
    PayloadUnion payload_specific;
    if (!rtp_payload_registry_->GetPayloadSpecifics(
        header.payloadType, &payload_specific)) {
      return -1;
    }
    receive_statistics_->IncomingPacket(header, len, false);
    if (!rtp_receiver_->IncomingRtpPacket(header,
                                          static_cast<const uint8_t*>(data),
                                          len, payload_specific, true)) {
      return -1;
    }
    return len;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (_rtpRtcpModule->IncomingRtcpPacket((const uint8_t*)data, len) < 0) {
      return -1;
    }
    return len;
  }
 private:
  int _count;
  int _packetLoss;
  ReceiveStatistics* receive_statistics_;
  RTPPayloadRegistry* rtp_payload_registry_;
  RtpReceiver* rtp_receiver_;
  RtpRtcp* _rtpRtcpModule;
};

class TestRtpReceiver : public NullRtpData {
 public:

  virtual int32_t OnReceivedPayloadData(
      const uint8_t* payloadData,
      const uint16_t payloadSize,
      const webrtc::WebRtcRTPHeader* rtpHeader) {
    EXPECT_LE(payloadSize, sizeof(_payloadData));
    memcpy(_payloadData, payloadData, payloadSize);
    memcpy(&_rtpHeader, rtpHeader, sizeof(_rtpHeader));
    _payloadSize = payloadSize;
    return 0;
  }

  const uint8_t* payload_data() const {
    return _payloadData;
  }

  uint16_t payload_size() const {
    return _payloadSize;
  }

  webrtc::WebRtcRTPHeader rtp_header() const {
    return _rtpHeader;
  }

 private:
  uint8_t _payloadData[1500];
  uint16_t _payloadSize;
  webrtc::WebRtcRTPHeader _rtpHeader;
};

}  // namespace webrtc