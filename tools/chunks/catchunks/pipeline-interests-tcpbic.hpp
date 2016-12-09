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

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_TCPBIC_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_TCPBIC_HPP

#include "options.hpp"
#include "aimd-rtt-estimator.hpp"
#include "aimd-rate-estimator.hpp"
#include "pipeline-interests-cwa.hpp"
#include "segment-info.hpp"

#include <queue>

namespace ndn {
namespace chunks {
namespace cwa {
namespace tcpbic {

class PipelineInterestsTcpBicOptions : public PipelineInterestsCwaOptions
{
public:
  explicit
  PipelineInterestsTcpBicOptions(const PipelineInterestsCwaOptions& options = PipelineInterestsCwaOptions())
    : PipelineInterestsCwaOptions(options)
  {
  }
};

/**
 * @brief Service for retrieving Data via an Interest pipeline
 *
 * Retrieves all segmented Data under the specified prefix by maintaining a dynamic AIMD
 * congestion window combined with a Conservative Loss Adaptation algorithm. For details,
 * please refer to the description in section "Interest pipeline types in ndncatchunks" of
 * tools/chunks/README.md
 *
 * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
 * PipelineInterests' user via callback immediately upon arrival.
 */
class PipelineInterestsTcpBic : public PipelineInterestsCwa
{
public:
  typedef PipelineInterestsTcpBicOptions Options;
  const int MAX_INCREMENT = 16;
  const int MAX_INT = std::numeric_limits<int>::max();
  const int LOW_WINDOW = 14;

public:
  /**
   * @brief create a PipelineInterestsAimd service
   *
   * Configures the pipelining service without specifying the retrieval namespace. After this
   * configuration the method run must be called to start the Pipeline.
   */
  PipelineInterestsTcpBic(Face& face,
                          RttEstimator& rttEstimator,
                          RateEstimator& rateEstimator,
                          const Options& options = Options());

private:
  /**
   * @brief increase congestion window size based on TCP-BIC scheme
   */
  virtual void
  doIncreaseWindow() final;

  /**
   * @brief decrease congestion window size based on TCP-BIC scheme
   */
  virtual void
  doDecreaseWindow() final;

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  const Options m_options;

  // for tcp bic
  bool is_bic_ss;
  double bic_target_win;
  double bic_min_win; /* increase cwnd by 1 after ACKs */
  double bic_max_win; /* last maximum snd_cwnd */
  int bic_ss_cwnd;
  int bic_ss_target;
  double m_beta;
  bool resetToInitial;
  int m_initialWindow;
};

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsTcpBicOptions& options);

} // namespace tcpbic
} // namespace cwa

using cwa::tcpbic::PipelineInterestsTcpBic;

} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_AIMD_HPP
