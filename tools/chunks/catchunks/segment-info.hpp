/**
 * Copyright (c) 2015,  Arizona Board of Regents.
 *
 * This file is part of ndn-tools (Named Data Networking Essential Tools).
 * See AUTHORS.md for complete list of ndn-tools authors and contributors.
 *
 * ndn-tools is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndn-tools is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndn-tools, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Shuo Yang <shuoyang@email.arizona.edu>
 */

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_SEGMENT_INFO_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_SEGMENT_INFO_HPP

#include <ndn-cxx/util/time.hpp>

namespace ndn {
namespace chunks {

/**
 * @brief indicates the state of the segment
 */
enum class SegmentState {
  FirstTimeSent, ///< segment has been sent for the first time
  InRetxQueue,   ///< segment is in retransmission queue
  Retransmitted, ///< segment has been retransmitted
  RetxReceived,  ///< segment has been received after retransmission
};

inline std::ostream&
operator<<(std::ostream& os, SegmentState state)
{
  switch (state) {
  case SegmentState::FirstTimeSent:
    os << "FirstTimeSent";
    break;
  case SegmentState::InRetxQueue:
    os << "InRetxQueue";
    break;
  case SegmentState::Retransmitted:
    os << "Retransmitted";
    break;
  case SegmentState::RetxReceived:
    os << "RetxReceived";
    break;
  }

  return os;
}


/**
 * @brief Wraps up information that's necessary for segment transmission
 */
struct SegmentInfo
{
  const PendingInterestId* interestId; ///< The pending interest ID returned by
                                       ///< ndn::Face::expressInterest. It can be used with
                                       ///< removePendingInterest before retransmitting this Interest.
  SegmentState state;
  Milliseconds rto;
  time::steady_clock::TimePoint timeSent;
};

} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_SEGMENT_INFO_HPP
