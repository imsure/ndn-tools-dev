/**
 * Copyright (c) 2016,  Regents of the University of California,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University.
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
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 *
 * @author Shuo Yang
 * @author Weiwei Liu
 */

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_CWA_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_CWA_HPP

#include "options.hpp"
#include "rtt-estimator.hpp"
#include "rate-estimator.hpp"
#include "pipeline-interests.hpp"
#include "segment-info.hpp"

#include <queue>

namespace ndn {
namespace chunks {
namespace cwa {

class PipelineInterestsCwaOptions : public Options
{
public:
  explicit
  PipelineInterestsCwaOptions(const Options& options = Options())
    : Options(options)
    , initCwnd(1.0)
    , initSsthresh(std::numeric_limits<double>::max())
    , rtoCheckInterval(time::milliseconds(10))
    , rateInterval(1)
    , disableCwa(false)
    , resetCwndToInit(false)
    , outputSummary(false)
  {
  }

public:
  double initCwnd; ///< initial congestion window size
  double initSsthresh; ///<  initial slow start threshold
  time::milliseconds rtoCheckInterval; ///<  time interval for checking retransmission timer
  double rateInterval; ///<  time interval (in second) for checking transmission rate
  bool disableCwa; ///< disable Conservative Window Adaptation
  bool resetCwndToInit; ///< reduce cwnd to initCwnd when loss event occurs
  bool outputSummary; ///< print summary information after finishing to stderr
};

/**
 * @brief Service for retrieving Data via an Interest pipeline with an
 * CWA (Conservative Loss Adaptation) based congestion control algorithm.
 * The sub-class of this class is responsible of implementing its own
 * window adaption algorithm.
 *
 * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
 * PipelineInterests' user via callback immediately upon arrival.
 */
class PipelineInterestsCwa : public PipelineInterests
{
public:
  typedef PipelineInterestsCwaOptions Options;

public:
  /**
   * @brief create a PipelineInterestsCwa service
   *
   * Configures the pipelining service without specifying the retrieval namespace. After this
   * configuration the method run must be called to start the Pipeline.
   */
  PipelineInterestsCwa(Face& face, RttEstimator& rttEstimator,
                       RateEstimator& rateEstimator,
                       const Options& options = Options());

  ~PipelineInterestsCwa();

  /**
   * @brief Signals when cwnd changes
   *
   * The callback function should be: void(Milliseconds age, double cwnd) where age is the
   * duration since pipeline starts, and cwnd is the new congestion window size (in segments).
   */
  signal::Signal<PipelineInterestsCwa, Milliseconds, double> afterCwndChange;

private:
  /**
   * @brief fetch all the segments between 0 and lastSegment of the specified prefix
   *
   * Starts the pipeline with an AIMD algorithm to control the window size. The pipeline will fetch
   * every segment until the last segment is successfully received or an error occurs.
   * The segment with segment number equal to m_excludedSegmentNo will not be fetched.
   */
  virtual void
  doRun() final;

  /**
   * @brief stop all fetch operations
   */
  virtual void
  doCancel() final;

  /**
   * @brief check RTO for all sent-but-not-acked segments.
   */
  void
  checkRto();

  void
  checkRate();

  /**
   * @param segNo the segment # of the to-be-sent Interest
   * @param isRetransmission true if this is a retransmission
   */
  void
  sendInterest(uint64_t segNo, bool isRetransmission);

  void
  schedulePackets();

  void
  handleData(const Interest& interest, const Data& data);

  void
  handleNack(const Interest& interest, const lp::Nack& nack);

  void
  handleLifetimeExpiration(const Interest& interest);

  void
  handleTimeout(int timeoutCount);

  void
  handleFail(uint64_t segNo, const std::string& reason);

  /**
   * @brief increase congestion window size
   */
  void
  increaseWindow();

  /**
   * @brief decrease congestion window size
   */
  void
  decreaseWindow();

  /**
   * @brief for sub-class to implement how to increase congestion window size
   */
  virtual void
  doIncreaseWindow() = 0;

  /**
   * @brief for sub-class to implement how to decrease congestion window size
   */
  virtual void
  doDecreaseWindow() = 0;

  /** \return next segment number to retrieve
   *  \post m_nextSegmentNo == return-value + 1
   */
  uint64_t
  getNextSegmentNo();

  void
  cancelInFlightSegmentsGreaterThan(uint64_t segmentNo);

  void
  printSummary() const;

PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  const Options m_options;
  RttEstimator& m_rttEstimator;
  RateEstimator& m_rateEstimator;
  Scheduler m_scheduler;
  uint64_t m_nextSegmentNo;
  size_t m_receivedSize;

  uint64_t m_highData; ///< the highest segment number of the Data packet the consumer has received so far
  uint64_t m_highInterest; ///< the highest segment number of the Interests the consumer has sent so far
  uint64_t m_recPoint; ///< the value of m_highInterest when a packet loss event occurred
                       ///< It remains fixed until the next packet loss event happens

  uint64_t m_nInFlight; ///< # of segments in flight
  uint64_t m_nReceived; ///< # of segments received
  uint64_t m_nLossEvents; ///< # of loss events occurred
  uint64_t m_nRetransmitted; ///< # of segments retransmitted

  time::steady_clock::TimePoint m_startTime; ///<  start time of pipelining

  double m_cwnd; ///< current congestion window size (in segments)
  double m_ssthresh; ///< current slow start threshold

  std::queue<uint64_t> m_retxQueue;

  std::unordered_map<uint64_t, SegmentInfo> m_segmentInfo; ///< the map keeps all the internal information
                                                           ///< of the sent but not ackownledged segments

  std::unordered_map<uint64_t, int> m_retxCount; ///< maps segment number to its retransmission count.
                                                 ///< if the count reaches to the maximum number of
                                                 ///< timeout/nack retries, the pipeline will be aborted
  bool m_hasFailure;
  uint64_t m_failedSegNo;
  std::string m_failureReason;

  //for Rate measurement
  uint64_t m_nPackets;
  uint64_t m_nBits;
};

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsCwaOptions& options);

} // namespace cwa

using cwa::PipelineInterestsCwa;

} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_CWA_HPP
