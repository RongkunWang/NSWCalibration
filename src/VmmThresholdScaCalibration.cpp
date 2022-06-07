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
}

void nsw::VmmThresholdScaCalibration::readThresholdFull()
{
  std::ofstream full_th(fmt::format("{}/{}_threshold_samples.txt", m_outPath, m_boardName));

  full_th.is_open();

  ERS_DEBUG(2, fmt::format("{} is {}", m_feName, (m_isStgc ? "s/pFEB" : "MMFE8")));

  ERS_INFO(fmt::format("{} Reading threshold [{} VMMs]", m_feName, (m_nVmms-m_firstVmm)));

  for (std::size_t vmmId = m_firstVmm; vmmId < m_nVmms; vmmId++) {
    const auto t0 = std::chrono::high_resolution_clock::now();

    for (std::size_t channelId = 0; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
      auto results = sampleVmmChThreshold(vmmId, channelId);

      const auto mean = cm::takeMean(results);
      const auto rms = cm::takeRms(results, mean);
      const auto mean_mV = cm::sampleTomV(mean, m_isStgc);
      const auto rms_mV = cm::sampleTomV(rms, m_isStgc);

      for (const auto& result : results) {
        writeTabDelimitedLine(full_th,
                              m_wheel,
                              m_sector,
                              m_feName,
                              vmmId,
                              channelId,
                              cm::sampleTomV(result, m_isStgc),
                              rms_mV);
      }

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

    const auto t1 = std::chrono::high_resolution_clock::now();
    const auto t_bl{t1-t0};

    ERS_DEBUG(3, fmt::format("{} VMM{}: done in {:%M:%S} [min]",m_feName , vmmId , t_bl));

  }  // vmm loop ends

  full_th.close();
  ERS_INFO(m_feName << " threshold done");
}
