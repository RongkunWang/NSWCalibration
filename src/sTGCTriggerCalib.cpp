#include "NSWCalibration/sTGCTriggerCalib.h"
using boost::property_tree::ptree;

nsw::sTGCTriggerCalib::sTGCTriggerCalib(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCTriggerCalib::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 1;

  // parse calib type
  if (m_calibType=="sTGCPadConnectivity") {
    ERS_INFO(m_calibType);
  } else {
    throw std::runtime_error("Unknown calibration request. Can't set up sTGCTriggerCalib.");
  }

  // make NSWConfig objects from input db
  m_pfebs = nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "PFEB");
  m_pts   = nsw::ConfigReader::makeObjects<nsw::PadTriggerSCAConfig> (db, "PadTriggerSCA");
  ERS_INFO("Found " << m_pfebs.size() << " pFEBs");
  ERS_INFO("Found " << m_pts.size()   << " pad triggers");
  if (m_pts.size() > 1)
    throw std::runtime_error("I dont know how to process >1 PadTriggers!");

  setTotal((int)(m_pfebs.size()));

  // make ConfigSenders
  for (auto & feb : m_pfebs)
    m_senders.insert( {feb.getAddress(), std::make_unique<nsw::ConfigSender>()} );
  for (auto & pt : m_pts)
    m_senders.insert( {pt.getAddress(),  std::make_unique<nsw::ConfigSender>()} );
}

void nsw::sTGCTriggerCalib::configure() {

  ERS_INFO("sTGCTriggerCalib::configure " << counter() << ", great job Olga!!");
  auto & pfeb = m_pfebs.at((size_t)(counter()));
  configure_vmms(pfeb, 1);
  configure_pad_trigger();
  sleep(1);

}

void nsw::sTGCTriggerCalib::unconfigure() {

  ERS_INFO("sTGCTriggerCalib::unconfigure " << counter());
  auto & pfeb = m_pfebs.at((size_t)(counter()));
  configure_vmms(pfeb, 0);
  sleep(1);

}

int nsw::sTGCTriggerCalib::configure_vmms(nsw::FEBConfig feb, bool unmask) {
  ERS_INFO("Configuring " << feb.getOpcServerIp() << " " << feb.getAddress() << " - " << (unmask ? "pulsing" : "masking"));
  for (size_t vmmid = 0; vmmid < feb.getVmms().size(); vmmid++) {
    feb.getVmm(vmmid).setChannelRegisterAllChannels("channel_st", unmask ? 1 : 0);
    feb.getVmm(vmmid).setChannelRegisterAllChannels("channel_sm", unmask ? 0 : 1);
  }
  auto & cs = m_senders[feb.getAddress()];
  if (!m_dry_run)
    cs->sendVmmConfig(feb);
  return 0;

}

int nsw::sTGCTriggerCalib::configure_pad_trigger() {
  for (auto & pt: m_pts) {
    ERS_INFO("Configuring " << pt.getOpcServerIp() << " " << pt.getAddress());
    // enable  the L1A readout
    // disable the L1A readout
  }
  return 0;
}
