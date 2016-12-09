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

#include "pipeline-interests-cubic.hpp"

#include <cmath>

namespace ndn {
namespace chunks {
namespace cwa {
namespace cubic {

PipelineInterestsCubic::PipelineInterestsCubic(Face& face,
                                               RttEstimator& rttEstimator,
                                               RateEstimator& rateEstimator,
                                               const Options& options)
  : PipelineInterestsCwa(face, rttEstimator, rateEstimator, options)
  , m_options(options)
  , m_cubicEpochStart(time::milliseconds::zero())
  , m_cubicLastMaxCwnd(0)
  , m_cubicK(0)
  , m_cubicOriginPoint(0)
  , m_cubicTcpCwnd(0)
{
  if (m_options.isVerbose) {
    std::cerr << m_options;
  }
}

void
PipelineInterestsCubic::cubicUpdate()
{
  if (m_cubicEpochStart == time::steady_clock::TimePoint(time::milliseconds::zero())) {
    // start a new congestion avoidance epoch
    m_cubicEpochStart = time::steady_clock::now();
    if (m_cwnd < m_cubicLastMaxCwnd) {
      m_cubicK = std::pow((m_cubicLastMaxCwnd - m_cwnd) / m_options.cubicScale, 1.0/3);
      m_cubicOriginPoint = m_cubicLastMaxCwnd;
    }
    else {
      m_cubicK = 0;
      m_cubicOriginPoint = m_cwnd;
    }
    m_cubicTcpCwnd = m_cwnd;
  }

  Milliseconds t = (time::steady_clock::now() - m_cubicEpochStart) + m_rttEstimator.getMinRtt();
  double target = m_cubicOriginPoint +
    m_options.cubicScale * std::pow(t.count()/1000.0 - m_cubicK, 3);
  double cubic_update = 0;
  if (target > m_cwnd) {
    cubic_update = m_cwnd + (target - m_cwnd) / m_cwnd;
  }
  else {
    cubic_update = m_cwnd + 0.01 / m_cwnd; // only a small increment
  }

  if (m_options.cubicTcpFriendliness) { // window grows at least at the speed of TCP
    m_cubicTcpCwnd += ((3 * m_options.cubicBeta) / (2 - m_options.cubicBeta)) *
      (t.count() / m_rttEstimator.getSmoothedRtt().count());
    if (m_cubicTcpCwnd > m_cwnd && m_cubicTcpCwnd > target) {
      cubic_update = m_cwnd + (m_cubicTcpCwnd - m_cwnd) / m_cwnd;
    }
  }

  m_cwnd = cubic_update;
}

void
PipelineInterestsCubic::doIncreaseWindow()
{
  if (m_cwnd < m_ssthresh) {
    m_cwnd += m_options.aiStep; // slow start
  } else {
    //m_cwnd += m_options.aiStep / std::floor(m_cwnd); // congestion avoidance
    cubicUpdate(); // congestion avoidance
  }
}

void
PipelineInterestsCubic::doDecreaseWindow()
{
  // reset cubic epoch start
  m_cubicEpochStart = time::steady_clock::TimePoint(time::milliseconds::zero());

  if (m_cwnd < m_cubicLastMaxCwnd && m_options.cubicFastConvergence) {
    // release more bandwidth for new flows to catch up
    m_cubicLastMaxCwnd = m_cwnd * (2 - m_options.cubicBeta) / 2;
  }
  else {
    m_cubicLastMaxCwnd = m_cwnd;
  }

  m_cwnd = m_cwnd * (1 - m_options.cubicBeta);
  m_ssthresh = std::max(2.0, m_cwnd); // multiplicative decrease
}

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsCubicOptions& options)
{
  os << "PipelineInterestsCubic initial parameters:" << "\n"
     << "\tAdditive increase step for slow start= " << options.aiStep << "\n"
     << "\tCubic multiplicative decrease factor = " << options.cubicBeta << "\n"
     << "\tCubic scaling factor = " << options.cubicScale << "\n";
  return os;
}

} // namespace cubic
} // namespace cwa
} // namespace chunks
} // namespace ndn
