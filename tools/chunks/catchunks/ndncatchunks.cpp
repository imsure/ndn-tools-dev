/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
 * @author Wentao Shang
 * @author Steve DiBenedetto
 * @author Andrea Tosatto
 * @author Davide Pesavento
 * @author Weiwei Liu
 * @author Shuo Yang
 */

#include "core/version.hpp"
#include "options.hpp"
#include "consumer.hpp"
#include "discover-version-fixed.hpp"
#include "discover-version-iterative.hpp"
#include "pipeline-interests-fixed-window.hpp"
#include "pipeline-interests-cwa.hpp"
#include "pipeline-interests-aimd.hpp"
#include "pipeline-interests-cubic.hpp"
#include "pipeline-interests-tcpbic.hpp"
#include "statistics-collector.hpp"

#include <ndn-cxx/security/validator-null.hpp>
#include <fstream>
#include <boost/filesystem.hpp>

namespace ndn {
namespace chunks {

static int
main(int argc, char** argv)
{
  std::string programName(argv[0]);
  Options options;
  std::string discoverType("iterative");
  std::string pipelineType("fixed");
  size_t maxPipelineSize(1);
  int maxRetriesAfterVersionFound(1);
  std::string uri;

  // congestion control parameters, CWA refers to conservative window adaptation,
  // i.e. only reduce window size at most once per RTT
  bool disableCwa(false), resetCwndToInit(false), outputSummary(false);
  double rateInterval(1);
  int initCwnd(1), initSsthresh(std::numeric_limits<int>::max()), k(4);

  double aiStepAimd(1.0), mdCoef(0.5); // parameters for AIMD pipeline
  double aiStepCubic(1.0), cubicScale(0.4), cubicBeta(0.2); // parameters for CUBIC pipeline

  // parameters for RTO calculation
  double alpha(0.125), beta(0.25), minRto(200.0), maxRto(4000.0);

  std::string statsPath, cwndPath, rttPath, ratePath;

  namespace po = boost::program_options;
  po::options_description basicDesc("Basic Options");
  basicDesc.add_options()
    ("help,h",      "print this help message and exit")
    ("discover-version,d",  po::value<std::string>(&discoverType)->default_value(discoverType),
                            "version discovery algorithm to use; valid values are: 'fixed', 'iterative'")
    ("pipeline-type,t",  po::value<std::string>(&pipelineType)->default_value(pipelineType),
                         "type of Interest pipeline to use; valid values are: 'fixed', 'aimd', 'tcpbic', 'cubic'")
    ("fresh,f",     po::bool_switch(&options.mustBeFresh), "only return fresh content")
    ("lifetime,l",  po::value<uint64_t>()->default_value(options.interestLifetime.count()),
                    "lifetime of expressed Interests, in milliseconds")
    ("retries,r",   po::value<int>(&options.maxRetriesOnTimeoutOrNack)->default_value(options.maxRetriesOnTimeoutOrNack),
                    "maximum number of retries in case of Nack or timeout (-1 = no limit)")
    ("verbose,v",   po::bool_switch(&options.isVerbose), "turn on verbose output")
    ("version,V",   "print program version and exit")
    ;

  po::options_description iterDiscoveryDesc("Iterative version discovery options");
  iterDiscoveryDesc.add_options()
    ("retries-iterative,i", po::value<int>(&maxRetriesAfterVersionFound)->default_value(maxRetriesAfterVersionFound),
     "number of timeouts that have to occur in order to confirm a discovered Data "
     "version as the latest one")
    ;

  po::options_description fixedPipeDesc("Fixed pipeline options");
  fixedPipeDesc.add_options()
    ("pipeline-size,s", po::value<size_t>(&maxPipelineSize)->default_value(maxPipelineSize),
     "size of the Interest pipeline")
    ;

  po::options_description ccPipeDesc("Common options for CWA (conservative window adaptation) based "
                                     "congestion control pipelines (aimd, tcpbic and cubic)");
  ccPipeDesc.add_options()
    ("summary,S",     po::bool_switch(&outputSummary), "print summary information after finishing to stderr")
    ("cc-debug-stats", po::value<std::string>(&statsPath),
     "output statistic data (cwnd, rtt, rate) to the given path")
    ("cc-rate-interval", po::value<double>(&rateInterval)->default_value(rateInterval),
     "time interval of measuring transmission rate (in second)")
    ("disable-cwa", po::bool_switch(&disableCwa),
     "disable Conservative Window Adaptation, "
     "i.e. reduce window on each timeout (instead of at most once per RTT)")
    ("initial-cwnd",       po::value<int>(&initCwnd)->default_value(initCwnd), "initial cwnd")
    ("initial-ssthresh",   po::value<int>(&initSsthresh),
     "initial slow start threshold (defaults to infinity)")
    ("aimd-reset-cwnd-to-init", po::bool_switch(&resetCwndToInit),
     "reset cwnd to initial cwnd when loss event occurs, default is "
     "resetting to ssthresh")
    ;

  po::options_description rtoDesc("Options for RTO calculation");
  rtoDesc.add_options()
    ("rto-alpha", po::value<double>(&alpha)->default_value(alpha),
     "alpha value for rto calculation")
    ("rto-beta",  po::value<double>(&beta)->default_value(beta),
     "beta value for rto calculation")
    ("rto-k",     po::value<int>(&k)->default_value(k),
     "k value for rto calculation")
    ("rto-min",   po::value<double>(&minRto)->default_value(minRto),
     "min rto value in milliseconds")
    ("to-max",   po::value<double>(&maxRto)->default_value(maxRto),
     "max rto value in milliseconds")
    ;

  po::options_description ccAimdPipeDesc("AIMD pipeline options");
  ccAimdPipeDesc.add_options()
    ("aimd-aistep",    po::value<double>(&aiStepAimd)->default_value(aiStepAimd),
     "additive-increase step")
    ("aimd-mdcoef",    po::value<double>(&mdCoef)->default_value(mdCoef),
     "multiplicative-decrease coefficient")
    ;

  po::options_description ccCubicPipeDesc("CUBIC pipeline options");
  ccCubicPipeDesc.add_options()
    ("cubic-scale", po::value<double>(&cubicScale)->default_value(cubicScale, std::to_string(cubicScale)),
     "cubic scaling factor")
    ("cubic-beta", po::value<double>(&cubicBeta)->default_value(cubicBeta, std::to_string(cubicBeta)),
     "cubic multiplicative decrease factor after a packet loss event")
    ("cubic-aistep", po::value<double>(&aiStepCubic)->default_value(aiStepCubic),
     "additive-increase step")
    ;

  po::options_description visibleDesc;
  visibleDesc.add(basicDesc).add(iterDiscoveryDesc).
    add(fixedPipeDesc).add(ccPipeDesc).add(rtoDesc).
    add(ccAimdPipeDesc).add(ccCubicPipeDesc);

  po::options_description hiddenDesc;
  hiddenDesc.add_options()
    ("ndn-name,n", po::value<std::string>(&uri), "NDN name of the requested content");

  po::positional_options_description p;
  p.add("ndn-name", -1);

  po::options_description optDesc;
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).positional(p).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }

  if (vm.count("help") > 0) {
    std::cout << "Usage: " << programName << " [options] ndn:/name" << std::endl;
    std::cout << visibleDesc;
    return 0;
  }

  if (vm.count("version") > 0) {
    std::cout << "ndncatchunks " << tools::VERSION << std::endl;
    return 0;
  }

  if (vm.count("ndn-name") == 0) {
    std::cerr << "Usage: " << programName << " [options] ndn:/name" << std::endl;
    std::cerr << visibleDesc;
    return 2;
  }

  Name prefix(uri);
  if (discoverType == "fixed" && (prefix.empty() || !prefix[-1].isVersion())) {
    std::cerr << "ERROR: The specified name must contain a version component when using "
                 "fixed version discovery" << std::endl;
    return 2;
  }

  if (maxPipelineSize < 1 || maxPipelineSize > 1024) {
    std::cerr << "ERROR: pipeline size must be between 1 and 1024" << std::endl;
    return 2;
  }

  if (options.maxRetriesOnTimeoutOrNack < -1 || options.maxRetriesOnTimeoutOrNack > 1024) {
    std::cerr << "ERROR: retries value must be between -1 and 1024" << std::endl;
    return 2;
  }

  if (maxRetriesAfterVersionFound < 0 || maxRetriesAfterVersionFound > 1024) {
    std::cerr << "ERROR: retries iterative value must be between 0 and 1024" << std::endl;
    return 2;
  }

  options.interestLifetime = time::milliseconds(vm["lifetime"].as<uint64_t>());

  try {
    Face face;

    unique_ptr<DiscoverVersion> discover;
    if (discoverType == "fixed") {
      discover = make_unique<DiscoverVersionFixed>(prefix, face, options);
    }
    else if (discoverType == "iterative") {
      DiscoverVersionIterative::Options optionsIterative(options);
      optionsIterative.maxRetriesAfterVersionFound = maxRetriesAfterVersionFound;
      discover = make_unique<DiscoverVersionIterative>(prefix, face, optionsIterative);
    }
    else {
      std::cerr << "ERROR: discover version type not valid" << std::endl;
      return 2;
    }

    unique_ptr<PipelineInterests> pipeline;
    unique_ptr<StatisticsCollector> statsCollector;
    unique_ptr<RttEstimator> rttEstimator;
    unique_ptr<RateEstimator> rateEstimator;
    std::ofstream statsFileCwnd;
    std::ofstream statsFileRtt;
    std::ofstream statsFileRate;

    if (pipelineType == "fixed") {
      PipelineInterestsFixedWindow::Options optionsPipeline(options);
      optionsPipeline.maxPipelineSize = maxPipelineSize;
      pipeline = make_unique<PipelineInterestsFixedWindow>(face, optionsPipeline);
    }
    else if (pipelineType == "cubic" || pipelineType == "aimd" || pipelineType == "tcpbic") {
      /* construct a map for using with switch */
      std::map<std::string, int> pipelines;
      pipelines["aimd"] = 0; pipelines["tcpbic"] = 1; pipelines["cubic"] = 2;

      /* set up RTT Estimator & Rate Estimator */
      RttEstimator::Options optionsRttEst;
      optionsRttEst.isVerbose = options.isVerbose;
      optionsRttEst.alpha = alpha;
      optionsRttEst.beta = beta;
      optionsRttEst.k = k;
      optionsRttEst.minRto = Milliseconds(minRto);
      optionsRttEst.maxRto = Milliseconds(maxRto);

      rttEstimator = make_unique<RttEstimator>(optionsRttEst);
      rateEstimator = make_unique<RateEstimator>(rateInterval);

      /* set up options for CWA pipelines */
      PipelineInterestsCwa::Options optionsCwa(options);
      optionsCwa.disableCwa = disableCwa;
      optionsCwa.initCwnd = static_cast<double>(initCwnd);
      optionsCwa.initSsthresh = static_cast<double>(initSsthresh);
      optionsCwa.rateInterval = rateInterval;
      optionsCwa.outputSummary = outputSummary;

      if (!statsPath.empty()) { // set up output files for stats if specified
        // construct stats file paths
        cwndPath = statsPath + '/' + "cwnd_" + pipelineType + ".txt";
        rttPath = statsPath + '/' + "rtt_" + pipelineType + ".txt";
        ratePath = statsPath + '/' + "rate_" + pipelineType + ".txt";

        // open stats files
        statsFileCwnd.open(cwndPath);
        if (statsFileCwnd.fail()) {
          std::cerr << "ERROR: failed to open " << cwndPath << std::endl;
          return 4;
        }
        statsFileRtt.open(rttPath);
        if (statsFileRtt.fail()) {
          std::cerr << "ERROR: failed to open " << rttPath << std::endl;
          return 4;
        }
        statsFileRate.open(ratePath);
        if (statsFileRate.fail()) {
          std::cerr << "ERROR: failed to open " << ratePath << std::endl;
          return 4;
        }
      }

      switch (pipelines[pipelineType]) {
      case 0: // aimd
        {
          PipelineInterestsAimd::Options optionsAimd(optionsCwa);
          optionsAimd.aiStep = aiStepAimd;
          optionsAimd.mdCoef = mdCoef;
          auto aimdPipeline = make_unique<PipelineInterestsAimd>(face, *rttEstimator, *rateEstimator, optionsAimd);
          if (!statsPath.empty()) {
            statsCollector = make_unique<StatisticsCollector>(*aimdPipeline, *rttEstimator, *rateEstimator,
                                                              statsFileCwnd, statsFileRtt, statsFileRate);
          }
          pipeline = std::move(aimdPipeline);
        }
        break;
      case 1: // tcpbic
        {
          PipelineInterestsTcpBic::Options optionsTcpBic(optionsCwa);
          auto tcpBicPipeline = make_unique<PipelineInterestsTcpBic>(face, *rttEstimator, *rateEstimator, optionsTcpBic);
          if (!statsPath.empty()) {
            statsCollector = make_unique<StatisticsCollector>(*tcpBicPipeline, *rttEstimator, *rateEstimator,
                                                              statsFileCwnd, statsFileRtt, statsFileRate);
          }
          pipeline = std::move(tcpBicPipeline);
        }
        break;
      case 2: // cubic
        {
          PipelineInterestsCubic::Options optionsCubic(optionsCwa);
          optionsCubic.aiStep = aiStepCubic;
          optionsCubic.cubicScale = cubicScale;
          optionsCubic.cubicBeta = cubicBeta;
          auto cubicPipeline = make_unique<PipelineInterestsCubic>(face, *rttEstimator, *rateEstimator, optionsCubic);
          if (!statsPath.empty()) {
            statsCollector = make_unique<StatisticsCollector>(*cubicPipeline, *rttEstimator, *rateEstimator,
                                                              statsFileCwnd, statsFileRtt, statsFileRate);
          }
          pipeline = std::move(cubicPipeline);
        }
        break;
      }
    }
    else {
      std::cerr << "ERROR: Interest pipeline type {" << pipelineType << "} not valid" << std::endl;
      return 2;
    }

    ValidatorNull validator;
    Consumer consumer(validator, options.isVerbose);

    BOOST_ASSERT(discover != nullptr);
    BOOST_ASSERT(pipeline != nullptr);
    consumer.run(std::move(discover), std::move(pipeline));
    face.processEvents();
  }
  catch (const Consumer::ApplicationNackError& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 3;
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

} // namespace chunks
} // namespace ndn

int
main(int argc, char** argv)
{
  return ndn::chunks::main(argc, argv);
}
