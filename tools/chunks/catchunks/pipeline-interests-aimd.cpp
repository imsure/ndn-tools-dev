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

#include "pipeline-interests-aimd.hpp"

#include <cmath>

namespace ndn {
namespace chunks {
namespace cwa {
namespace aimd {

PipelineInterestsAimd::PipelineInterestsAimd(Face& face,
                                             RttEstimator& rttEstimator,
                                             RateEstimator& rateEstimator,
                                             const Options& options)
  : PipelineInterestsCwa(face, rttEstimator, rateEstimator, options)
  , m_options(options)
{
  if (m_options.isVerbose) {
    std::cerr << m_options;
  }
}

void
PipelineInterestsAimd::doIncreaseWindow()
{
  if (m_cwnd < m_ssthresh) {
    m_cwnd += m_options.aiStep; // additive increase
  } else {
    m_cwnd += m_options.aiStep / std::floor(m_cwnd); // congestion avoidance
  }
}

void
PipelineInterestsAimd::doDecreaseWindow()
{
  // please refer to RFC 5681, Section 3.1 for the rationale behind it
  m_ssthresh = std::max(2.0, m_cwnd * m_options.mdCoef); // multiplicative decrease
  m_cwnd = m_options.resetCwndToInit ? m_options.initCwnd : m_ssthresh;
}

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsAimdOptions& options)
{
  os << "PipelineInterestsAimd initial parameters:" << "\n"
     << "\tMultiplicative decrease factor = " << options.mdCoef << "\n"
     << "\tAdditive increase step = " << options.aiStep << "\n";

  return os;
}

} // namespace aimd
} // namespace cwa
} // namespace chunks
} // namespace ndn
