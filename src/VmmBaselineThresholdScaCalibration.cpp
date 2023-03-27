#include "NSWCalibration/VmmBaselineThresholdScaCalibration.h"

#include <chrono>
#include <fstream>

#include <fmt/core.h>
#include <fmt/chrono.h>

#include "NSWCalibration/CalibrationMath.h"

namespace cm = nsw::CalibrationMath;

using namespace std::chrono_literals;

nsw::VmmBaselineThresholdScaCalibration::VmmBaselineThresholdScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                                                          std::string outpath,
                                                          const std::size_t nSamplesBaseline,
                                                          const std::size_t nSamplesThreshold,
                                                          const std::size_t sector,
                                                          const int wheel,
                                                          const bool debug) :
  nsw::ScaCalibration::ScaCalibration(feb,
                                      std::move(outpath),
                                      nSamplesBaseline,
                                      nSamplesThreshold,
                                      sector,
                                      wheel,
                                      debug),
  m_nSamplesBaseline(nSamplesBaseline),
  m_nSamplesThreshold(nSamplesThreshold)
{}

void nsw::VmmBaselineThresholdScaCalibration::runCalibration()
{
    if (m_nSamplesBaseline<1) m_nSamplesBaseline=1;
    ERS_INFO(fmt::format("{}: Reading baselines, thresholds. nSamplesBaseline={} nSamplesThreshold={}", m_feName,m_nSamplesBaseline,m_nSamplesThreshold));

    // Output files
    std::ofstream outputThresholds(fmt::format("{}/{}_threshold_samples.txt", m_outPath, m_boardName));
    std::ofstream outputBaselines(fmt::format("{}/{}_baseline_samples.txt", m_outPath, m_boardName));
    outputThresholds.is_open();
    outputBaselines.is_open();

    ERS_DEBUG(2, fmt::format("{} is {}", m_feName, (m_isStgc ? "s/pFEB" : "MMFE8")));
    ERS_INFO(fmt::format("{} Reading from [{} VMMs]", m_feName, (m_nVmms-m_firstVmm)));

    // Loop over VMMs
    for (std::size_t vmmId = m_firstVmm; vmmId < m_nVmms; vmmId++) {
        // Loop over channels
        ERS_DEBUG(3, fmt::format("{} VMM{}: starting",m_feName , vmmId));
        for (std::size_t channelId = 0; channelId < nsw::vmm::NUM_CH_PER_VMM; channelId++) {
            readChannelThreshold(outputThresholds,vmmId,channelId);
            readChannelBaseline(outputBaselines,vmmId,channelId);
        }
        ERS_DEBUG(3, fmt::format("{} VMM{}: done",m_feName , vmmId));
  }

  outputThresholds.close();
  outputBaselines.close();

  ERS_INFO(fmt::format("{}: Done with baselines, thresholds. Files written to {}", m_feName, m_outPath));
}

void nsw::VmmBaselineThresholdScaCalibration::readChannelThreshold(std::ofstream& outputFile, std::size_t vmmId, std::size_t channelId){
    // Read data (short unsigned int)
    auto results = sampleVmmChThreshold(vmmId, channelId, m_nSamplesThreshold);

    // Calculate mean, rms
    const auto mean    = cm::takeMean(results);
    const auto mean_mV = cm::sampleTomV(mean, m_isStgc);
    const auto rms     = cm::takeRms(results, mean);
    const auto rms_mV  = cm::sampleTomV(rms, m_isStgc);

    // Write output to file
    for (const auto& result : results) {
        float result_mV = cm::sampleTomV(result, m_isStgc);
        writeTabDelimitedLine(outputFile, m_wheel, m_sector, m_feName, vmmId, channelId, result_mV, rms_mV);
    }
}

void nsw::VmmBaselineThresholdScaCalibration::readChannelBaseline(std::ofstream& outputFile, std::size_t vmmId, std::size_t channelId){
    // Read data (short unsigned int)
    auto results = sampleVmmChMonDac(vmmId, channelId, m_nSamplesBaseline);

    // Calculate mean, rms
    const auto mean    = cm::takeMean(results);
    const auto mean_mV = cm::sampleTomV(mean, m_isStgc);
    const auto rms     = cm::takeRms(results, mean);
    const auto rms_mV  = cm::sampleTomV(rms, m_isStgc);

    // Write output to file
    for (const auto& result : results) {
        float result_mV = cm::sampleTomV(result, m_isStgc);
        writeTabDelimitedLine(outputFile, m_wheel, m_sector, m_feName, vmmId, channelId, result_mV, rms_mV);
    }
}
