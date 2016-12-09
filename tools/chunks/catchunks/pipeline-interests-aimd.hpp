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

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_AIMD_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_AIMD_HPP

#include "pipeline-interests-cwa.hpp"

namespace ndn {
namespace chunks {
namespace cwa {
namespace aimd {

class PipelineInterestsAimdOptions : public PipelineInterestsCwaOptions
{
public:
  explicit
  PipelineInterestsAimdOptions(const PipelineInterestsCwaOptions& options = PipelineInterestsCwaOptions())
    : PipelineInterestsCwaOptions(options)
    , mdCoef(0.5)
    , aiStep(1.0)
  {
  }

public:
  double mdCoef; ///< multiplicative decrease coefficient
  double aiStep; ///< additive increase step (unit: segment)
};

/**
 * @brief Service for retrieving Data via an Interest pipeline with AIMD
 * congestion window control algorithm.
 *
 * Retrieves all segmented Data under the specified prefix by maintaining a dynamic AIMD
 * congestion window combined with a Conservative Loss Adaptation algorithm. For details,
 * please refer to the description in section "Interest pipeline types in ndncatchunks" of
 * tools/chunks/README.md
 *
 * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
 * PipelineInterests' user via callback immediately upon arrival.
 */
class PipelineInterestsAimd : public PipelineInterestsCwa
{
public:
  typedef PipelineInterestsAimdOptions Options;

public:
  /**
   * @brief create a PipelineInterestsAimd service
   *
   * Configures the pipelining service without specifying the retrieval namespace. After this
   * configuration the method run must be called to start the Pipeline.
   */
  PipelineInterestsAimd(Face& face, RttEstimator& rttEstimator,
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

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  const Options m_options;
};

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsAimdOptions& options);

} // namespace aimd
} // namespace cwa

using cwa::aimd::PipelineInterestsAimd;

} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_AIMD_HPP
