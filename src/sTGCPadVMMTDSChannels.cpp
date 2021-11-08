#include "NSWCalibration/sTGCPadVMMTDSChannels.h"
#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Utility.h"
#include "ers/ers.h"
#include <future>

void nsw::sTGCPadVMMTDSChannels::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  // make objects and outputs
  setup_objects(db);

  // set calib options
  setTotal((nsw::NUM_VMM_PER_PFEB - nsw::PFEB_FIRST_PAD_VMM)
           * nsw::vmm::NUM_CH_PER_VMM);

  nsw::snooze();
}

void nsw::sTGCPadVMMTDSChannels::configure() {
  ERS_INFO("sTGCPadVMMTDSChannels::configure " << counter());

  // launch 1 thread per PFEB configuration
  auto threads = std::vector< std::future<void> >();
  for (auto & feb : m_pfebs)
    threads.push_back( std::async(std::launch::async,
                                  &nsw::sTGCPadVMMTDSChannels::configure_pfeb,
                                  this,
                                  feb) );

  // wait on them
  for (auto& thread : threads) {
    thread.get();
  }

}

nsw::commands::Commands nsw::sTGCPadVMMTDSChannels::getAltiSequences() const {
  return {{}, // before configure
          {nsw::commands::actionStartPG}, // during (before acquire)
          {} // after (before unconfigure)
  };
}

void nsw::sTGCPadVMMTDSChannels::configure_pfeb(nsw::FEBConfig feb) {
  ERS_INFO("Configuring "
           << feb.getOpcServerIp()
           << "/"
           << feb.getAddress()
           );

  // choose the VMM channel to pulse
  const size_t vmmId  = counter() / nsw::vmm::NUM_CH_PER_VMM + nsw::PFEB_FIRST_PAD_VMM;
  const size_t chan = counter() % nsw::vmm::NUM_CH_PER_VMM;
  ERS_INFO("Enable VMM " << vmmId << ", channel " << chan);

  // mask all channels
  for (auto& vmm : feb.getVmms()) {
    vmm.setChannelRegisterAllChannels("channel_st", false);
    vmm.setChannelRegisterAllChannels("channel_sm", true);
  }

  // test pulse and unmask the channel of interest
  feb.getVmm(vmmId).setChannelRegisterOneChannel("channel_st", true,  chan);
  feb.getVmm(vmmId).setChannelRegisterOneChannel("channel_sm", false, chan);

  // send configuration
  const auto cs = std::make_unique<nsw::ConfigSender>();
  if (!simulation()) {
    cs->sendVmmConfig(feb);
  }

}

void nsw::sTGCPadVMMTDSChannels::setup_objects(const std::string& db) {
  ERS_INFO("Making pFEB and Pad Trigger objects");

  // create NSWConfiguration objects
  m_pfebs = nsw::ConfigReader::makeObjects<nsw::FEBConfig>(db, "PFEB");
  m_pts   = nsw::ConfigReader::makeObjects<nsw::PadTriggerSCAConfig>(db, "PadTriggerSCA");
  ERS_INFO("Found " << m_pfebs.size() << " pFEBs");
  ERS_INFO("Found " << m_pts.size()   << " pad triggers");

  // require exactly one Pad Trigger
  if (m_pts.size() != 1) {
    std::stringstream msg;
    msg << "Cannot handle !=1 pad triggers."
        << " You gave: "
        << m_pts.size();
    nsw::NSWsTGCPadVMMTDSChannelsIssue issue(ERS_HERE, msg.str());
    ers::fatal(issue);
    throw std::runtime_error(msg.str());
  }

}
