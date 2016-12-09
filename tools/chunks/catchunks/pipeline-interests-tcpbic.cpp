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

#include "pipeline-interests-tcpbic.hpp"

#include <cmath>

namespace ndn {
namespace chunks {
namespace cwa {
namespace tcpbic {

PipelineInterestsTcpBic::PipelineInterestsTcpBic(Face& face, RttEstimator& rttEstimator,
												 RateEstimator& rateEstimator, const Options& options)
	: PipelineInterestsCwa(face, rttEstimator, rateEstimator, options)
	, m_options(options)
	, is_bic_ss(false)
	, bic_target_win(0)
	, bic_min_win(0)
	, bic_max_win(MAX_INT)
	, bic_ss_cwnd(0)
	, bic_ss_target(0)
{
	if (m_options.isVerbose) {
	 	std::cerr << m_options;
	}
}

void PipelineInterestsTcpBic::doIncreaseWindow()
{
	if (m_cwnd < LOW_WINDOW) {
		// Normal TCP
		if (m_cwnd <= m_ssthresh) {
			m_cwnd = m_cwnd + 1;
		}
		else {
			m_cwnd = m_cwnd + (double) 1.0 / m_cwnd;
		}
	}
	else if (is_bic_ss == false) { // bin. increase
		//      std::cout << "BIC Increase, cwnd: " << m_cwnd << ", bic_target_win: " << bic_target_win << "\n";
		if (bic_target_win - m_cwnd < MAX_INCREMENT) { // binary search
			m_cwnd += (bic_target_win - m_cwnd) / m_cwnd;
		}
		else {
			m_cwnd += MAX_INCREMENT / m_cwnd; // additive increase
		}
		// FIX for equal double values.
		if (m_cwnd + 0.00001 < bic_max_win) {
			//        std::cout << "3 Cwnd: " << m_cwnd << ", bic_max_win: " << bic_max_win << "\n";
			bic_min_win = m_cwnd;
			//        std::cout << "bic_max_win: " << bic_max_win << ", bic_min_win: " << bic_min_win << "\n";
			bic_target_win = (bic_max_win + bic_min_win) / 2;
		}
		else {
			//              std::cout
			//                  << time::duration_cast<time::milliseconds>(time::steady_clock::now().time_since_epoch())
			//                  << " setting BIC SS true\n";
			is_bic_ss = true;
			bic_ss_cwnd = 1;
			bic_ss_target = m_cwnd + 1;
			bic_max_win = MAX_INT;
		}
	}
	else { // slow start
		//      std::cout
		//          << time::duration_cast<time::milliseconds>(time::steady_clock::now().time_since_epoch())
		//          << "Entering BIC slow start! cwnd: " << m_cwnd << "\n";
		//      std::cout << "SS BIC Increase, cwnd: " << m_cwnd << ", bic_target_win: " << bic_target_win << "\n";
		m_cwnd += bic_ss_cwnd / m_cwnd;
		if (m_cwnd >= bic_ss_target) {
			bic_ss_cwnd = 2 * bic_ss_cwnd;
			bic_ss_target = m_cwnd + bic_ss_cwnd;
		}
		if (bic_ss_cwnd >= MAX_INCREMENT) {
			is_bic_ss = false;
		}
	}
}

void PipelineInterestsTcpBic::doDecreaseWindow()
{
  // BIC Decrease
  if (m_cwnd >= LOW_WINDOW) {
    auto prev_max = bic_max_win;
    bic_max_win = m_cwnd;
    m_cwnd = m_cwnd * m_beta;
    bic_min_win = m_cwnd;
    if (prev_max > bic_max_win) //Fast. Conv.
      bic_max_win = (bic_max_win + bic_min_win) / 2;
    bic_target_win = (bic_max_win + bic_min_win) / 2;
  }
  else {
    // Normal TCP
    m_ssthresh = m_cwnd * 0.5;
    m_cwnd = m_ssthresh;
  }

  if (m_options.resetCwndToInit) {
    m_ssthresh = m_cwnd * 0.5;
    m_cwnd = m_initialWindow;
  }
}

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsTcpBicOptions& options)
{
	os << "PipelineInterestsTcpBic initial parameters:" << "\n";
	return os;
}

} // namespace aimd
} // namespace cwa
} // namespace chunks
} // namespace ndn
