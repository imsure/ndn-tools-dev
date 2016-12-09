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
 */

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_CUBIC_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_CUBIC_HPP

#include "options.hpp"
#include "aimd-rtt-estimator.hpp"
#include "aimd-rate-estimator.hpp"
#include "pipeline-interests.hpp"
#include "segment-info.hpp"
#include "pipeline-interests-cwa.hpp"

#include <queue>

namespace ndn {
namespace chunks {
namespace cwa {
namespace cubic {

class PipelineInterestsCubicOptions : public PipelineInterestsCwaOptions
{
public:
  explicit
  PipelineInterestsCubicOptions(const PipelineInterestsCwaOptions& options = PipelineInterestsCwaOptions())
    : PipelineInterestsCwaOptions(options)
    , aiStep(1.0)
    , cubicScale(0.4)
    , cubicBeta(0.2)
    , cubicFastConvergence(true)
    , cubicTcpFriendliness(false)
  {
  }

public:
  /* CUBIC specific options */
  double aiStep; ///< additive increase step during slow start (unit: segment)
  double cubicScale; ///< cubic scaling factor
  double cubicBeta; ///< multiplicative decrease factor after a packet loss event
  bool cubicFastConvergence;
  bool cubicTcpFriendliness;
};

/**
 * @brief Service for retrieving Data via an Interest pipeline
 *
 * Retrieves all segmented Data under the specified prefix by maintaining a dynamic CUBIC
 * congestion window combined with a Conservative Loss Adaptation algorithm. For details,
 * please refer to the description in section "Interest pipeline types in ndncatchunks" of
 * tools/chunks/README.md
 *
 * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
 * PipelineInterests' user via callback immediately upon arrival.
 */
class PipelineInterestsCubic : public PipelineInterestsCwa
{
public:
  typedef PipelineInterestsCubicOptions Options;

public:
  /**
   * @brief create a PipelineInterestsCubic service
   *
   * Configures the pipelining service without specifying the retrieval namespace. After this
   * configuration the method run must be called to start the Pipeline.
   */
  PipelineInterestsCubic(Face& face, RttEstimator& rttEstimator,
                         RateEstimator& rateEstimator,
                         const Options& options = Options());

private:
  /**
   * @brief increase congestion window size based on AIMD scheme
   */
  virtual void
  doIncreaseWindow() final;

  /**
   * @brief decrease congestion window size based on AIMD scheme
   */
  virtual void
  doDecreaseWindow() final;

  void
  cubicUpdate();

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  const Options m_options;
  /* CUBIC specific parameters */
  time::steady_clock::TimePoint m_cubicEpochStart; ///< beginning of an epoch
  double m_cubicLastMaxCwnd; ///< Wmax
  double m_cubicK;
  double m_cubicOriginPoint;
  double m_cubicTcpCwnd;
  Milliseconds m_cubicMinRtt;
};

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsCubicOptions& options);

} // namespace cubic
} // namespace cwa

using cwa::cubic::PipelineInterestsCubic;

} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_CUBIC_HPP
