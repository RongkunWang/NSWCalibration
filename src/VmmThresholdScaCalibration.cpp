#include "NSWCalibration/VmmThresholdScaCalibration.h"

#include <chrono>
#include <fstream>

#include <fmt/core.h>
#include <fmt/chrono.h>

#include "NSWCalibration/CalibrationMath.h"

namespace cm = nsw::CalibrationMath;

using namespace std::chrono_literals;

nsw::VmmThresholdScaCalibration::VmmThresholdScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                                                          std::string outpath,
                                                          const std::size_t nSamples,
                                                          const std::size_t rmsFactor,
                                                          const std::size_t sector,
                                                          const int wheel,
                                                          const bool debug) :
  nsw::ScaCalibration::ScaCalibration(feb,
                                      std::move(outpath),
                                      nSamples,
                                      rmsFactor,
                                      sector,
                                      wheel,
                                      debug)
{}

void nsw::VmmThresholdScaCalibration::runCalibration()
{
  ERS_INFO(fmt::format("{}: Reading thresholds", m_feName));
  readThresholdFull();
  ERS_INFO(fmt::format("{}: Done with thresholds", m_feName));

  //std::this_thread::sleep_for(1000ms);

  //ERS_INFO(fmt::format("{}: Calibrating VMM internal pulser", m_feName));
  //calibPulserDac();
  //ERS_INFO(fmt::format("{}: Done with VMM internal pulser", m_feName));
}

void nsw::VmmThresholdScaCalibration::readThresholdFull()
{
  std::ofstream full_bl(fmt::format("{}/{}_threshold_samples.txt", m_outPath, m_boardName));

  full_bl.is_open();
  std::size_t fault_chan_total = 0;

  ERS_DEBUG(2, fmt::format("{} is {}", m_feName, (m_isStgc ? "s/pFEB" : "MMFE8")));

  ERS_INFO(fmt::format("{} Reading threshold [{} VMMs]", m_feName, (m_nVmms-m_firstVmm)));

  for (std::size_t vmmId = m_firstVmm; vmmId < m_nVmms; vmmId++) {
    const auto t0 = std::chrono::high_resolution_clock::now();

    std::size_t fault_chan{0};
    std::size_t noisy_channels{0};

    for (std::size_t channelId = 0; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
      auto results = sampleVmmChThreshold(vmmId, channelId);

      const auto mean = cm::takeMean(results);
      const auto rms = cm::takeRms(results, mean);
      const auto mean_mV = cm::sampleTomV(mean, m_isStgc);
      const auto rms_mV = cm::sampleTomV(rms, m_isStgc);

      for (const auto& result : results) {
        writeTabDelimitedLine(full_bl,
                              m_wheel,
                              m_sector,
                              m_feName,
                              vmmId,
                              channelId,
                              cm::sampleTomV(result, m_isStgc),
                              rms_mV);
      }

      //if (rms_mV > nsw::ref::RMS_CUTOFF) {
      //  noisy_channels++;
      //}

      //if ((mean_mV >= nsw::ref::BROKEN_HIGH_BASELINE) ||
      //    (mean_mV <= nsw::ref::BROKEN_LOW_BASELINE)) {
      //  ers::warning(nsw::VmmThresholdScaCalibrationIssue(
      //    ERS_HERE,
      //    fmt::format(
      //      "{} VMM{}: channel {} broken threshold V = {:2.4f} mV", m_feName, vmmId, channelId, mean_mV)));
      //}

      // FIXME TODO remove m_debug, but understand if fault_chan can
      // be generally set or only in certain circumstances
      //if (m_debug) {
      //  if (mean_mV > nsw::ref::HIGH_CH_BASELINE) {
      //    fault_chan++;
      //    ERS_DEBUG(2,
      //              fmt::format("Side: {} sector: {} {} VMM{}: channel {} high threshold = {:2.4f} mV",
      //                          m_wheel,
      //                          m_sector,
      //                          m_feName,
      //                          vmmId,
      //                          channelId,
      //                          mean_mV));
      //  } else if (mean_mV < nsw::ref::LOW_CH_BASELINE) {
      //    fault_chan++;
      //    ERS_DEBUG(2,
      //              fmt::format("Side: {} sector: {} {} VMM{}: channel {} low threshold = {:2.4f} mV",
      //                          m_wheel,
      //                          m_sector,
      //                          m_feName,
      //                          vmmId,
      //                          channelId,
      //                          mean_mV));
      //  }
      //}

      std::sort(results.begin(), results.end());

      const auto [max_dev, min_dev] = [&results]() -> std::tuple<std::size_t, std::size_t> {
        if (!results.empty()) {
          return {results.back(), results.front()};
        }
        return {0, 0};
      }();

      // FIXME TODO add these to the output file?
      const auto median = cm::takeMedian(results);
      const auto mode = cm::takeMode(results);
      const auto sample_dev = max_dev - min_dev;
      ERS_DEBUG(
        3,
        fmt::format(
          "{} VMM{}: channel {} thresholds |{:2.4f}/{:2.4f}/{:2.4f} mV (median/mean/mode)| {:2.4f}/{:2.4f} mV (RMS/spread)",
          m_feName,
          vmmId,
          channelId,
          cm::sampleTomV(median, m_isStgc),
          mean_mV,
          cm::sampleTomV(mode, m_isStgc),
          rms_mV,
          cm::sampleTomV(sample_dev, m_isStgc)));
    }  // channel loop ends

    fault_chan_total += fault_chan;
    const auto t1 = std::chrono::high_resolution_clock::now();
    const auto t_bl{t1-t0};

    ERS_DEBUG(3, fmt::format("{} VMM{}: done in {:%M:%S} [min]",m_feName , vmmId , t_bl));

    if (noisy_channels >= nsw::vmm::NUM_CH_PER_VMM / 4) {
      ERS_DEBUG(1,
                fmt::format("{} VMM{}: More than quarter of VMM channels [{}] have noise above 30mV",
                            m_feName,
                            vmmId,
                            noisy_channels));
    }

    if ((fault_chan > nsw::vmm::NUM_CH_PER_VMM / 4) and
        (fault_chan < nsw::vmm::NUM_CH_PER_VMM / 2)) {
      ers::warning(nsw::VmmThresholdScaCalibrationIssue(
        ERS_HERE,
        fmt::format(
          "{} VMM{}: more than quarter faulty channels [{}]", m_feName, vmmId, fault_chan)));
    } else if (fault_chan >= nsw::vmm::NUM_CH_PER_VMM / 2) {
      ers::warning(nsw::VmmThresholdScaCalibrationIssue(
        ERS_HERE,
        fmt::format("{} VMM{}: [ATTENTION] more than HALF of channels [{}] are faulty",
                    m_feName,
                    vmmId,
                    fault_chan)));
    }
  }  // vmm loop ends

  if (fault_chan_total >= m_quarterOfFebChannels) {
    ers::warning(nsw::VmmThresholdScaCalibrationIssue(
      ERS_HERE,
      fmt::format("{}: Large number of faulty channels ({}/512)!",
                  m_feName,
                  fault_chan_total)));
  }

  full_bl.close();
  ERS_INFO(m_feName << " threshold done");
}

// FIXME TODO why do this in a threshold run?
void nsw::VmmThresholdScaCalibration::calibPulserDac()
{
  ERS_INFO("Calibrating " << m_feName);

  const std::vector tpDacPoints = {
    100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800};

  std::ofstream tp_file(fmt::format("{}/{}_TPDAC_samples.txt", m_outPath, m_boardName));
  std::ofstream tp_var_file(fmt::format("{}/{}_TPDAC_data.txt", m_outPath, m_boardName));

  tp_file.is_open();
  tp_var_file.is_open();

  for (std::size_t vmmId = m_firstVmm; vmmId < m_nVmms; vmmId++) {
    std::vector<float> sample_mean_v;

    for (const auto& tpDacPoint : tpDacPoints) {
      ERS_DEBUG(3,
                fmt::format("{} VMM{}: reading pulser ADC at [{}]", m_feName, vmmId, tpDacPoint));

      const auto results = sampleVmmTpDac(vmmId, tpDacPoint);

      const auto sample_mean = cm::takeMean(results);
      const auto sample_mean_mV = cm::sampleTomV(sample_mean, m_isStgc);

      for (auto& res : results) {
        writeTabDelimitedLine(tp_file,
                              m_wheel,
                              m_sector,
                              m_feName,
                              vmmId,
                              tpDacPoint,
                              res,
                              cm::sampleTomV(res, m_isStgc));
      }

      sample_mean_v.push_back(sample_mean);

      std::stringstream line;
      writeTabDelimitedLine(
        line, m_wheel, m_sector, m_feName, vmmId, tpDacPoint, sample_mean, sample_mean_mV);

      tp_var_file << line.str();

      ERS_DEBUG(3, fmt::format("{} VMM{}: calib_puserDAC: {}", m_feName, vmmId, line.str()));
    }

    // Do linear fit between TP DAC value and measured ADC count
    /*
    const auto tpdac_point_mean = cm::takeMean(tpDacPoints);
    const auto point_vect_sample_mean = cm::takeMean(sample_mean_v);
    auto num = 0.f;
    auto denom = 0.f;

    for (std::size_t i = 0; i < tpDacPoints.size(); i++) {
      num +=
        (tpDacPoints.at(i) - tpdac_point_mean) * (sample_mean_v.at(i) - point_vect_sample_mean);
      denom += std::pow((tpDacPoints.at(i) - tpdac_point_mean), 2);
    }

    const auto slope = num / denom;
    const auto intercept = point_vect_sample_mean - (slope * tpdac_point_mean);
    */
    const auto [slope, intercept] = cm::fitLine(tpDacPoints, sample_mean_v);
    ERS_DEBUG(
      2,
      fmt::format(
        "{} VMM{}: Linear fit yields: [a = {}] - [b = {}]", m_feName, vmmId, intercept, slope));
  }

  tp_var_file.close();
  tp_file.close();
  ERS_INFO(fmt::format("{} Pulser DAC calibrated!", m_feName));
}
