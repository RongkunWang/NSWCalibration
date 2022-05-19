#include "NSWCalibration/ScaCalibration.h"

#include <chrono>

#include <fmt/core.h>

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/Utility.h"

using namespace std::chrono_literals;

nsw::ScaCalibration::ScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                                    std::string outpath,
                                    const std::size_t nSamples,
                                    const std::size_t rmsFactor,
                                    const std::size_t sector,
                                    const int wheel,
                                    const bool debug) :
  m_feb(feb),
  m_feName(feb.get().getScaAddress()),
  m_boardName(feb.get().getFilenameCompatibleGeoId()),
  m_isStgc(m_feName.find("FEB") != std::string::npos || m_feName.find("sTGC") != std::string::npos),
  m_debug(debug),
  m_outPath(std::move(outpath)),
  m_nSamples(nSamples),
  m_rmsFactor(rmsFactor),
  m_wheel(wheel),
  m_sector(sector)
{
  const auto [n_vmms, firstVmm, quarterOfFebChannels] = getBoardVmmConstants();

  m_nVmms = n_vmms;
  m_firstVmm = firstVmm;
  m_quarterOfFebChannels = quarterOfFebChannels;
}

nsw::calib::VMMSampleVector nsw::ScaCalibration::getVmmPdoSamples(const VMMConfig& config,
                                                                  const std::size_t vmmId,
                                                                  const std::size_t samplingFactor)
{
  nsw::calib::VMMSampleVector results{};

  for (std::size_t itry{1}; itry <= nsw::MAX_ATTEMPTS; ++itry) {
    try {
      results = m_feb.get().getVmm(vmmId).samplePdoMonitoringOutput(config, m_nSamples * samplingFactor);

      if (results.size() == m_nSamples * samplingFactor) {
        return results;
      } else if (itry == nsw::MAX_ATTEMPTS) {
        ers::warning(nsw::ScaMaxRetries(
          ERS_HERE,
          m_feName,
          vmmId,
          nsw::MAX_ATTEMPTS,
          fmt::format("DAC sampling was incomplete: collected {}/{} samples",
                      results.size(),
                      m_nSamples * samplingFactor)));
        return results;
      }

      results.clear();
      continue;
    } catch (const std::exception& e) {
      ers::warning(nsw::ScaVmmSamplingIssue(
        ERS_HERE, m_feName, vmmId,
        fmt::format("Can not sample DAC: reason [{}]", e.what())));
      std::this_thread::sleep_for(2000ms);
    }
  }

  nsw::ScaMaxRetries issue(
    ERS_HERE,
    m_feName,
    vmmId,
    nsw::MAX_ATTEMPTS,
    "The connection to the front-end was probably lost. Please check the state of the readout");
  ers::error(issue);
  return results;
}


// FIXME TODO for thresholds, add an RMS check and if too large, resample
nsw::calib::VMMSampleVector nsw::ScaCalibration::sampleVmmChMonDac(const std::size_t vmmId,
                                                                   const std::size_t channelId,
                                                                   const std::size_t samplingFactor)
{
  auto config = m_feb.get().getVmm(vmmId).getConfig();
  config.setMonitorOutput(static_cast<std::uint32_t>(channelId), nsw::vmm::ChannelMonitor);
  config.setChannelMOMode(static_cast<std::uint32_t>(channelId), nsw::vmm::ChannelAnalogOutput);

  return getVmmPdoSamples(config, vmmId, samplingFactor);
}

nsw::calib::VMMSampleVector nsw::ScaCalibration::sampleVmmChTrimDac(const std::size_t vmmId,
                                                                    const std::size_t channelId,
                                                                    const std::size_t thrDac,
                                                                    const std::size_t trimDac,
                                                                    const std::size_t samplingFactor)
{
  auto config = m_feb.get().getVmm(vmmId).getConfig();
  config.setMonitorOutput(static_cast<std::uint32_t>(channelId), nsw::vmm::ChannelMonitor);
  config.setChannelMOMode(static_cast<std::uint32_t>(channelId), nsw::vmm::ChannelTrimmedThreshold);
  config.setChannelTrimmer(static_cast<std::uint32_t>(channelId), static_cast<std::uint32_t>(trimDac));
  config.setGlobalThreshold(static_cast<std::uint32_t>(thrDac));

  return getVmmPdoSamples(config, vmmId, samplingFactor);
}

nsw::calib::VMMSampleVector nsw::ScaCalibration::sampleVmmChThreshold(const std::size_t vmmId,
                                                                      const std::size_t channelId)
{
  auto config = m_feb.get().getVmm(vmmId).getConfig();
  config.setMonitorOutput(static_cast<std::uint32_t>(channelId), nsw::vmm::ChannelMonitor);
  config.setChannelMOMode(static_cast<std::uint32_t>(channelId), nsw::vmm::ChannelTrimmedThreshold);

  return getVmmPdoSamples(config, vmmId);
}

nsw::calib::VMMSampleVector nsw::ScaCalibration::sampleVmmThDac(const std::size_t vmmId,
                                                                const std::size_t dacValue,
                                                                const std::size_t samplingFactor)
{
  auto config = m_feb.get().getVmm(vmmId).getConfig();
  config.setMonitorOutput(nsw::vmm::ThresholdDAC, nsw::vmm::CommonMonitor);
  config.setGlobalThreshold(static_cast<std::uint32_t>(dacValue));

  return getVmmPdoSamples(config, vmmId, samplingFactor);
}

nsw::calib::VMMSampleVector nsw::ScaCalibration::sampleVmmTpDac(const std::size_t vmmId,
                                                                const std::size_t dacValue,
                                                                const std::size_t samplingFactor)
{
  auto config = m_feb.get().getVmm(vmmId).getConfig();
  config.setMonitorOutput(nsw::vmm::TestPulseDAC, nsw::vmm::CommonMonitor);
  config.setTestPulseDAC(static_cast<std::uint32_t>(dacValue));

  return getVmmPdoSamples(config, vmmId, samplingFactor);
}


bool nsw::ScaCalibration::checkIfUnconnected(const std::string& feName,
                                             const std::size_t vmmId,
                                             const std::size_t channelId) const
{
  const bool isStgc{feName.find("FEB") != std::string::npos};
  // FIXME TODO works *only* for MMFE8?
  if (isStgc) {
    return false;
  }

  // current naming example - MMFE8_L1P1_IPL
  // temporarily using strict cut on the naming length
  // FIXME TODO this needs to be adapted based on how devices are named in the DB and for sTGC
  // "missed strips" in the MM and sTGC parameter book
  // MMFE8_LXPY_(HO|IP)(L|R)
  // SFEB_LXQY_(HO|IP)(L|R)
  // SFEB8_LXQY_(HO|IP)(L|R)
  // SFEB6_LXQY_(HO|IP)(L|R)
  // PFEB_LXQY_(HO|IP)(L|R)
  // const auto tokens  = tokenize(feName);
  // const auto type = tokens.at(0);
  // const auto pos  = tokens.at(1);
  // const auto side = tokens.at(2);
  // if (type not in known_types)
  //   if (pos.length() == 4) {
  //     if (side.length() == 3) {
  //       const std::size_t layer = std::stoull(pos.substr(1,1)); // L<X>
  //       const std::size_t pcb = std::stoull(pos.substr(3,1)); // P<Y> (MM) Q<Y> (sTGC)
  //       const auto quad = side.at(0); // HO/IP ??
  //       const auto side = side.at(2); // L/R ??

  if (feName.length() != 14) {
    ERS_LOG(fmt::format("{} VMM{}, channel {}: FEB name does not fit the format "
                        "[MMFE8_L#P#_(HO/IP)(L/R)], unable to check for unconnected channels.",
                        feName,
                        vmmId,
                        channelId));
    return false;
  }

  const std::size_t layer = std::stoull(feName.substr(7, 1));
  const std::size_t pcb = std::stoull(feName.substr(9, 1));
  const char quad = feName[11];
  const char side = feName[13];

  ERS_DEBUG(2,
            fmt::format("{} pars: L={}, PCB={}, quad={}, side={}", feName, layer, pcb, quad, side));

  const auto [this_radius, this_vmm, this_chan] = [&]() -> std::tuple<std::size_t, std::size_t, std::size_t> {
    if (((layer == 1 || layer == 3) && quad == 'H') ||
        ((layer == 2 || layer == 4) && quad == 'I')) {
      if (side == 'R') {
        return {2 * (pcb - 1), 7 - vmmId, 63 - channelId};
      } else {
        return {2 * (pcb - 1) + 1, vmmId, channelId};
      }
    } else {  // L and R inverted
      if (side == 'L') {
        return {2 * (pcb - 1), 7 - vmmId, 63 - channelId};
      } else {
        return {2 * (pcb - 1) + 1, vmmId, channelId};
      }
    }
  }();

  // Strip index
  const std::size_t i{this_radius * nsw::mmfe8::NUM_CH_PER_MMFE8 +
                      this_vmm * nsw::vmm::NUM_CH_PER_VMM +
                      this_chan};

  if (i % 1024 == 1023 || i % 1024 == 0) {
    return true;
  }

  // Layer index
  const auto L = [&quad, &layer]() {
    if (quad == 'H') {
      return std::size_t{7 - (layer - 1)};  // HO1 = layer7
    } else {
      return std::size_t{layer - 1};  // IP1 = layer0
    }
  }();

  ERS_DEBUG(2,
            fmt::format("{} parameters : VMM{} mapped chan={} and layer={}", feName, vmmId, i, L));

  if (m_sector % 2 == 0) {
    if ((L == 0 || L == 1 || L == 6 || L == 7) &&
        (i < 42 || (i > 5078 && i < 5149) || i > 8160)) {
      // 42 SM1 nMissedStrip_bottom eta layer MM
      return true;
    } else if ((L == 2 || L == 3 || L == 4 || L == 5) &&
               (i < 35 || (i > 5108 && i < 5121))) {
      // 35 SM1 nMissedStrip_bottom stereo layer MM
      return true;
    }
  } else {
    if ((L == 0 || L == 1 || L == 6 || L == 7) &&
        (i < 72 || (i > 5047 && i < 5167) || i > 8143)) {
      // 72 LM1 nMissedStrip_bottom eta layer MM
      return true;
    } else if ((L == 2 || L == 3 || L == 4 || L == 5) &&
               (i < 86 || (i > 5083 && i < 5162) || i > 8148)) {
      // 86 LM1 nMissedStrip_bottom stereo layer MM
      return true;
    }
  }
  return false;
}

nsw::calib::FEBVMMConstants nsw::ScaCalibration::getBoardVmmConstants()
{
  const std::string feName = m_feb.get().getScaAddress();
  const auto nVmms = m_feb.get().getNumVmms();
  if (nVmms == nsw::ref::NUM_VMM_SFEB6) {
    return {nsw::NUM_VMM_PER_SFEB,
            nsw::SFEB6_FIRST_VMM,
            std::floor((nsw::vmm::NUM_CH_PER_VMM * nsw::ref::NUM_VMM_SFEB6) / 4.0)};
  } else if (nVmms == nsw::NUM_VMM_PER_PFEB) {
    return {nsw::NUM_VMM_PER_PFEB,
            0,
            std::floor((nsw::vmm::NUM_CH_PER_VMM * nsw::NUM_VMM_PER_PFEB) / 4.0)};
  } else if (nVmms == nsw::NUM_VMM_PER_MMFE8) {
    return {nsw::NUM_VMM_PER_MMFE8, 0, std::floor(nsw::mmfe8::NUM_CH_PER_MMFE8 / 4.0)};
  } else {
    nsw::ScaFebCalibrationIssue issue(
      ERS_HERE,
      feName,
      fmt::format("Unrealistic number of VMMs in the configuration [{}]", nVmms));
    ers::error(issue);
    return {0,0,0};
  }
}
