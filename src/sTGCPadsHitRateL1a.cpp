#include "NSWCalibration/sTGCPadsHitRateL1a.h"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <ers/ers.h>
#include <is/infodynany.h>
#include <is/infodictionary.h>

nsw::sTGCPadsHitRateL1a::sTGCPadsHitRateL1a(std::string calibType,
                                      const hw::DeviceManager& deviceManager):
  CalibAlg(std::move(calibType), deviceManager)
{
  setTotal(m_thresholdAdjustments.size());
}

void nsw::sTGCPadsHitRateL1a::configure() {
  setPadTriggerSector();
  setFebThresholds();
}

void nsw::sTGCPadsHitRateL1a::acquire() {
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    ERS_INFO(fmt::format("Enable readout for {}", dev.getName()));
    dev.writeReadoutEnable();
  }
  ERS_INFO(fmt::format("Acquiring data for {}", m_acquire_time));
  nsw::snooze(std::chrono::seconds{m_acquire_time});
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    ERS_INFO(fmt::format("Disable readout for {}", dev.getName()));
    dev.writeReadoutDisable();
  }
}

void nsw::sTGCPadsHitRateL1a::checkObjects() const {
  const std::size_t npads = getDeviceManager().getPadTriggers().size();
  const std::size_t nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npads));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npads != std::size_t{1}) {
    const auto msg = std::string("Requires 1 pad trigger");
    ers::error(nsw::NSWsTGCPadsHitRateL1aIssue(ERS_HERE, msg));
  }
}

void nsw::sTGCPadsHitRateL1a::setPadTriggerSector() const {
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    ERS_INFO(fmt::format("Set 'sector' {} in {}", counter(), dev.getName()));
    dev.writeFPGARegister(nsw::padtrigger::REG_CONTROL2, counter());
  }
}

void nsw::sTGCPadsHitRateL1a::setFebThresholds() const {
  auto threads = std::vector< std::future<void> >();
  for (const auto& dev: getDeviceManager().getFebs()) {
    threads.push_back(
      std::async(std::launch::async, &nsw::sTGCPadsHitRateL1a::setFebThreshold, this, dev)
    );
  }
  for (auto& thread: threads) {
    thread.get();
  }
}

void nsw::sTGCPadsHitRateL1a::setFebThreshold(const nsw::hw::FEB& dev) const {
  if (nsw::getElementType(dev.getScaAddress()) != "PFEB") {
    return;
  }
  const auto thrAdj = m_thresholdAdjustments.at(counter());
  for (const auto& vmmDev: dev.getVmms()) {
    auto vmmConf = nsw::VMMConfig{vmmDev.getConfig()};
    const auto currentThreshold = vmmConf.getGlobalThreshold();
    ERS_INFO(fmt::format("Setting threshold {} in {}/{}",
                         currentThreshold + thrAdj, dev.getScaAddress(), vmmConf.getName()));
    checkThresholdAdjustment(currentThreshold, thrAdj);
    vmmConf.setGlobalThreshold(currentThreshold + thrAdj);
    vmmDev.writeConfiguration(vmmConf);
  }
}

void nsw::sTGCPadsHitRateL1a::checkThresholdAdjustment(std::uint32_t thr, int adj) const {
  if (adj < 0 and static_cast<std::uint32_t>(std::abs(adj)) > thr) {
    const auto msg = fmt::format("Cannot adjust {} by {}", thr, adj);
    throw std::runtime_error(msg);
  }
}

void nsw::sTGCPadsHitRateL1a::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                                   const std::string& is_db_name) {
  const auto name = fmt::format("{}.Calib.calibParams", is_db_name);
  if (is_dictionary.contains(name)) {
    ISInfoDynAny infoany;
    is_dictionary.getValue(name, infoany);
    const auto val = std::stoul(infoany.getAttributeValue<std::string>(0));
    m_acquire_time = std::chrono::seconds(val);
    ERS_INFO(fmt::format("Found acquire time in IS: {}", m_acquire_time));
  }
}

