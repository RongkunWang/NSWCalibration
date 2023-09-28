#include "NSWCalibration/sTGCTriggerCalib.h"

#include "NSWConfiguration/Constants.h"

#include <cstdlib>
#include <unistd.h>
#include <stdexcept>

#include "ers/ers.h"

using namespace std::chrono_literals;

nsw::sTGCTriggerCalib::sTGCTriggerCalib(std::string calibType, const nsw::hw::DeviceManager& deviceManager):
  CalibAlg(std::move(calibType), deviceManager),
  m_latencyScan{m_calibType == "sTGCPadLatency"}
{
  checkCalibType();
  checkObjects();
  setTotal(m_latencyScan ? getLatencyScanNBC() : nsw::padtrigger::NUM_PFEBS);
}

void nsw::sTGCTriggerCalib::configure() {
  ERS_INFO("Current feb: " << getCurrentFebName());
  if (m_latencyScan) {
    // test pulse all pfebs
    if (counter() == 0) {
      ERS_LOG("sTGCTriggerCalib::configure all pFEBs");
      for (const auto& feb: getDeviceManager().getFebs()) {
        configureVMMs(feb, m_unmask);
      }
    }
    configurePadTrigger();
    nsw::snooze();
  } else {
    // mask everything to start
    if (counter() == 0) {
      ERS_LOG("Masking all PFEB VMM channels");
      for (const auto& feb: getDeviceManager().getFebs()) {
        configureVMMs(feb, m_mask);
      }
    }
    // test pulse one pfeb
    try {
      configureVMMs(getCurrentFeb(), m_unmask);
    } catch (const std::exception& ex) {
      ERS_LOG(fmt::format("{} is absent, continuing",
                          getCurrentFebName()));
    }
    configurePadTrigger();
    nsw::snooze();
  }
}

void nsw::sTGCTriggerCalib::unconfigure() {
  ERS_LOG("sTGCTriggerCalib::unconfigure " << counter());
  if (m_latencyScan) {
    // mask pulse all pfebs
    if (counter() == total() - 1) {
      ERS_LOG("sTGCTriggerCalib::unconfigure all PFEBs");
      for (const auto& feb: getDeviceManager().getFebs()) {
        configureVMMs(feb, m_mask);
      }
    }
  } else {
    try {
      configureVMMs(getCurrentFeb(), m_mask);
      nsw::snooze();
    } catch (const std::exception& ex) {
      ERS_LOG(fmt::format("Current feb {} is absent, continuing", getCurrentFebName()));
    }
  }
}

std::string_view nsw::sTGCTriggerCalib::getCurrentFebName() const {
  return nsw::padtrigger::ORDERED_PFEBS_GEOID.at(counter());
}

const nsw::hw::FEB& nsw::sTGCTriggerCalib::getCurrentFeb() const {
  for (const auto& feb: getDeviceManager().getFebs()) {
    if (feb.getGeoInfo().resourceType() != "PFEB") {
      continue;
    }
    if (nsw::contains(feb.getScaAddress(), getCurrentFebName())) {
      return feb;
    }
  }
  throw std::runtime_error("Couldnt get current FEB");
}

void nsw::sTGCTriggerCalib::configureVMMs(const nsw::hw::FEB& feb, bool unmask) const {
  if (feb.getGeoInfo().resourceType() != "PFEB") {
    return;
  }
  if (not feb.getRoc().reachable()) {
    ERS_LOG("Skipping unreachable " << feb.getScaAddress());
    return;
  }
  ERS_LOG(fmt::format("Configuring {} ({})", feb.getScaAddress(), (unmask ? "pulsing" : "masking")));
  for (const auto& vmmDev: feb.getVmms()) {
    if (vmmDev.getVmmId() == nsw::PFEB_WIRE_VMM) {
      continue;
    }
    auto vmmConf = nsw::VMMConfig{vmmDev.getConfig()};
    vmmConf.setGlobalRegister("sdp_dac", m_pulse);
    vmmConf.setGlobalRegister("sdt_dac", m_threshold);
    vmmConf.setChannelRegisterAllChannels("channel_st", unmask ? true : false);
    vmmConf.setChannelRegisterAllChannels("channel_sm", unmask ? false : true);
    try {
      vmmDev.writeConfiguration(vmmConf);
    } catch (const std::exception& ex) {
      ers::warning(nsw::NSWsTGCTriggerCalibIssue(ERS_HERE, ex.what()));
    }
  }
}

void nsw::sTGCTriggerCalib::configurePadTrigger() const {
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    ERS_LOG("Configuring " << pt.getName());
    if (m_latencyScan) {
      pt.writeReadoutBCOffset(getLatencyScanCurrent());
    }
    // L1A based
    if(m_calibType == "sTGCPadConnectivity" or
       m_calibType == "sTGCPadLatency") {
      pt.writeReadoutEnableTemporarily(50ms);
    // SCA based
    } else {
      nsw::snooze(3s);
      const auto rates = simulation() ?
        std::vector<std::uint32_t>(nsw::padtrigger::NUM_PFEBS) : pt.readPFEBRates();
      writeToFile(rates);
    }
  }
}

void nsw::sTGCTriggerCalib::writeToFile(const std::vector<std::uint32_t>& rates) const {
  const auto fname = fmt::format("{}.{}.{}.txt", m_calibType, runNumber(), applicationName());
  if (counter() == 0) {
    ERS_LOG("Opening " << fname);
  }
  std::ofstream myfile;
  myfile.open(fname, std::ios_base::app);
  std::size_t pfeb{0};
  for (const auto& rate: rates) {
    myfile << fmt::format("Iteration {:02} PFEB {:02} {}", counter(), pfeb, rate) << std::endl;
    ++pfeb;
  }
  myfile.close();
}

std::uint32_t nsw::sTGCTriggerCalib::getLatencyScanOffset() const {
  if (not m_latencyScan) {
    return 0;
  }
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    return pt.LatencyScanStart();
  }
  const std::string msg{"No pad triggers in DB"};
  ers::error(nsw::NSWsTGCTriggerCalibIssue(ERS_HERE, msg));
  throw std::runtime_error(msg);
}

std::uint32_t nsw::sTGCTriggerCalib::getLatencyScanNBC() const {
  if (not m_latencyScan) {
    return 0;
  }
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    return pt.LatencyScanNBC();
  }
  const std::string msg{"No pad triggers in DB"};
  ers::error(nsw::NSWsTGCTriggerCalibIssue(ERS_HERE, msg));
  throw std::runtime_error(msg);
}

void nsw::sTGCTriggerCalib::checkCalibType() const {
  if (m_calibType == "sTGCPadConnectivity" or
      m_calibType == "sTGCPadConnectivitySca" or
      m_calibType == "sTGCPadLatency") {
    ERS_INFO(m_calibType);
  } else {
    const auto msg = fmt::format("Unknown calibration: {}", m_calibType);
    ers::error(nsw::NSWsTGCTriggerCalibIssue(ERS_HERE, msg));
    throw std::runtime_error(msg);
  }
}

void nsw::sTGCTriggerCalib::checkObjects() const {
  const auto npts  = getDeviceManager().getPadTriggers().size();
  const auto nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npts));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npts > 1) {
    const auto msg = fmt::format("sTGCTriggerCalib only works with 1 PT");
    ers::error(nsw::NSWsTGCTriggerCalibIssue(ERS_HERE, msg));
  }
}

