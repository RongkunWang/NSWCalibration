#include "NSWCalibration/sTGCPadVMMTDSChannels.h"
#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Utility.h"
#include "ers/ers.h"
#include <future>

using namespace std::chrono_literals;

void nsw::sTGCPadVMMTDSChannels::setup(const std::string& db) {
  ERS_INFO(fmt::format("setup {}", db));
  checkObjects();
  setTotal(nsw::NUM_PAD_VMM_PER_PFEB * nsw::vmm::NUM_CH_PER_VMM);
}

void nsw::sTGCPadVMMTDSChannels::configure() {
  configurePfebs();
  configurePadTrigger();
}

void nsw::sTGCPadVMMTDSChannels::configurePfebs() {
  auto threads = std::vector< std::future<void> >();
  for (const auto& feb: getDeviceManager().getFebs()) {
    threads.push_back(
      std::async(std::launch::async, &nsw::sTGCPadVMMTDSChannels::configurePfeb, this, feb)
    );
  }
  for (auto& thread : threads) {
    thread.get();
  }
}

void nsw::sTGCPadVMMTDSChannels::configurePfeb(const nsw::hw::FEB& feb) {
  if (nsw::getElementType(feb.getScaAddress()) != "PFEB") {
    return;
  }
  ERS_INFO(fmt::format("Configuring {}", feb.getScaAddress()));

  // choose the VMM channel to pulse
  const std::size_t vmmId = counter() / nsw::vmm::NUM_CH_PER_VMM + nsw::PFEB_FIRST_PAD_VMM;
  const std::size_t chan  = counter() % nsw::vmm::NUM_CH_PER_VMM;
  ERS_INFO(fmt::format("Enable VMM {}, channel {}", vmmId, chan));

  // mask all channels, and
  // test pulse and unmask the channel of interest
  for (const auto& vmmDev: feb.getVmms()) {
    if (vmmDev.getVmmId() == nsw::PFEB_WIRE_VMM) {
      continue;
    }
    auto vmmConf = nsw::VMMConfig{vmmDev.getConfig()};
    vmmConf.setChannelRegisterAllChannels("channel_st", false);
    vmmConf.setChannelRegisterAllChannels("channel_sm", true);
    if (vmmDev.getVmmId() == vmmId) {
      vmmConf.setChannelRegisterOneChannel("channel_st", true,  chan);
      vmmConf.setChannelRegisterOneChannel("channel_sm", false, chan);
    }
    vmmDev.writeConfiguration(vmmConf);
  }
}

void nsw::sTGCPadVMMTDSChannels::configurePadTrigger() {
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    ERS_INFO(fmt::format("Configuring {}", pt.getName()));
    pt.writeReadoutEnableTemporarily(10ms);
  }
}

void nsw::sTGCPadVMMTDSChannels::checkObjects() {
  const auto npads = getDeviceManager().getPadTriggers().size();
  const auto nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npads));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npads != std::size_t{1}) {
    const auto msg = std::string("Requires 1 pad trigger");
    ers::error(nsw::NSWsTGCPadVMMTDSChannelsIssue(ERS_HERE, msg));
  }
}
