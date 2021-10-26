#include "NSWCalibration/sTGCTriggerCalib.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Constants.h"

#include <cstdlib>
#include <unistd.h>
#include <stdexcept>

#include "ers/ers.h"

nsw::sTGCTriggerCalib::sTGCTriggerCalib(const std::string& calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCTriggerCalib::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  gather_pfebs();

  // parse calib type
  if (m_calibType=="sTGCPadConnectivity") {
    ERS_INFO(m_calibType);
  } else if (m_calibType=="sTGCPadLatency") {
    ERS_INFO(m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCTriggerCalib: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  ERS_INFO("Making pFEB and Pad Trigger objects");
  m_pfebs = nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "PFEB");
  m_pts   = nsw::ConfigReader::makeObjects<nsw::PadTriggerSCAConfig> (db, "PadTriggerSCA");
  ERS_INFO("Found " << m_pfebs.size() << " pFEBs");
  ERS_INFO("Found " << m_pts.size()   << " pad triggers");

  if (m_pts.size() != 1) {
    std::stringstream msg;
    msg << "I dont know how to process !=1 PadTriggers. You gave: " << m_pts.size();
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // PT details:
  // keep IdleState low during the calib
  // get latency scan parameters from user
  for (auto & pt: m_pts) {
    pt.SetStartIdleState(0);
    set_latencyscan_offset(pt.LatencyScanStart());
    set_latencyscan_nbc(pt.LatencyScanNBC());
  }

  // set number of loops in the iteration
  if (m_calibType=="sTGCPadConnectivity") {
    setTotal(static_cast<int>(m_pfebs.size()));
  } else if (m_calibType=="sTGCPadLatency") {
    setTotal(latencyscan_nbc());
    ERS_INFO("Latency scan start: " << latencyscan_offset());
    ERS_INFO("Latency scan steps: " << latencyscan_nbc());
  }
  setToggle(false);
  setWait4swROD(false);
  usleep(1e6);
}

void nsw::sTGCTriggerCalib::configure() {
  ERS_INFO("sTGCTriggerCalib::configure " << counter());
  if (m_calibType=="sTGCPadConnectivity") {

    // test pulse one pfeb
    if (order_pfebs()) {
      auto next_addr = next_pfeb(false);
      for (auto & pfeb: m_pfebs) {
        if (next_addr == pfeb.getAddress()) {
          configure_vmms(pfeb, true);
          break;
        }
      }
    } else {
      auto & pfeb = m_pfebs.at(static_cast<size_t>(counter()));
      configure_vmms(pfeb, true);
    }

    configure_pad_trigger();
    usleep(500e3);

  } else if (m_calibType=="sTGCPadLatency") {

    // test pulse all pfebs
    if (counter() == 0) {
      ERS_INFO("sTGCTriggerCalib::configure all pFEBs");
      for (auto & pfeb: m_pfebs)
        configure_vmms(pfeb, true);
    }

    // set pad trigger readout latency
    for (auto & pt: m_pts)
      pt.SetL1AReadoutLatency(latencyscan_current());
    configure_pad_trigger();
    usleep(1e6);
    usleep(5e6);
  }
}

void nsw::sTGCTriggerCalib::unconfigure() {
  ERS_INFO("sTGCTriggerCalib::unconfigure " << counter());

  if (m_calibType=="sTGCPadConnectivity") {
    if (order_pfebs()) {
      auto next_addr = next_pfeb(true);
      for (auto & pfeb: m_pfebs) {
        if (next_addr == pfeb.getAddress()) {
          configure_vmms(pfeb, false);
          break;
        }
      }
    } else {
      auto & pfeb = m_pfebs.at(static_cast<size_t>(counter()));
      configure_vmms(pfeb, false);
    }

    usleep(500e3);

  } else if (m_calibType=="sTGCPadLatency") {

    // mask pulse all pfebs
    if (counter() == total()-1) {
      ERS_INFO("sTGCTriggerCalib::unconfigure all pFEBs");
      for (auto & pfeb: m_pfebs)
          configure_vmms(pfeb, false);
    }
  }
}

int nsw::sTGCTriggerCalib::configure_vmms(nsw::FEBConfig feb, bool unmask) {
  ERS_INFO("Configuring " << feb.getOpcServerIp() << " " << feb.getAddress() << " - " << (unmask ? "pulsing" : "masking"));
  for (size_t vmmid = 0; vmmid < feb.getVmms().size(); vmmid++) {
    feb.getVmm(vmmid).setChannelRegisterAllChannels("channel_st", unmask ? 1 : 0);
    feb.getVmm(vmmid).setChannelRegisterAllChannels("channel_sm", unmask ? 0 : 1);
  }
  auto cs = std::make_unique<nsw::ConfigSender>();
  if (!simulation())
    cs->sendVmmConfig(feb);
  return 0;
}

int nsw::sTGCTriggerCalib::configure_pad_trigger() {
  for (auto & pt: m_pts) {
    auto cs = std::make_unique<nsw::ConfigSender>();
    ERS_INFO("Configuring " << pt.getOpcServerIp() << " " << pt.getAddress());

    // enable the L1A readout
    pt.SetL1AReadoutEnable();
    if (!simulation())
        cs->sendPadTriggerSCAControlRegister(pt);

    // pause to collect L1As
    usleep(50e3);

    // disable the L1A readout
    pt.SetL1AReadoutDisable();
    if (!simulation())
        cs->sendPadTriggerSCAControlRegister(pt);
  }
  return 0;
}

void nsw::sTGCTriggerCalib::gather_pfebs() {
  //
  // get the partition environment
  //
  const auto part = std::getenv("TDAQ_PARTITION");
  if (part == nullptr) {
    throw std::runtime_error("Error: TDAQ_PARTITION not defined");
  }
  const std::string partition(part);
  const std::string sector_name = nsw::guessSector(applicationName());
  ERS_INFO("Gather PFEBs: found partition "  << partition);
  ERS_INFO("Gather PFEBs: application "      << applicationName());
  ERS_INFO("Gather PFEBs: sector name "      << sector_name);
  ERS_INFO("Gather PFEBs: sector is large: " << nsw::isLargeSector(sector_name));
  std::uint32_t firmware_dateword{0};
  for (const auto & pt: m_pts) {
    firmware_dateword = pt.firmware_dateword();
    break;
  }
  ERS_INFO("Gather pFEBs: found firmware dateword " << firmware_dateword);
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather pFEBs: VS pFEBs");
    if (firmware_dateword > nsw::padtrigger::DATE_NEW_MAPPING) {
      m_pfebs_ordered.push_back("PFEB_L1Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L2Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L3Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L4Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L1Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L2Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L3Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L4Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L1Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L2Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L3Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L4Q3_IP");
    } else {
      m_pfebs_ordered.push_back("PFEB_L4Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L2Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L4Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L2Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L1Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L2Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L3Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L4Q3_IP");
      m_pfebs_ordered.push_back("PFEB_L1Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L3Q2_IP");
      m_pfebs_ordered.push_back("PFEB_L1Q1_IP");
      m_pfebs_ordered.push_back("PFEB_L3Q1_IP");
    }
  }
  else {
    // 191/P1
    if (firmware_dateword > nsw::padtrigger::DATE_NEW_MAPPING) {
      for (const auto& pfeb: nsw::padtrigger::ORDERED_PFEBS) {
        m_pfebs_ordered.push_back(std::string(pfeb));
      }
    } else {
      const auto& pfebs = (nsw::isLargeSector(sector_name)) ?
        nsw::padtrigger::ORDERED_PFEBS_OLDFW_LS :
        nsw::padtrigger::ORDERED_PFEBS_OLDFW_SS;
      for (const auto& pfeb: pfebs) {
        m_pfebs_ordered.push_back(std::string(pfeb));
      }
    }
  }
}

std::string nsw::sTGCTriggerCalib::next_pfeb(bool pop) {
  if (m_pfebs_ordered.size() == 0) {
    std::string msg = "Error in next_pfeb. vector is empty, cant get next element.";
    nsw::NSWsTGCTriggerCalibIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw std::runtime_error(msg);
  }
  // get the next valid pfeb
  // "valid" means: exists in the config
  auto next_feb = m_pfebs_ordered.front();
  for (auto & pfeb: m_pfebs) {
    if (pfeb.getAddress().find(next_feb) != std::string::npos) {
      if (pop)
        m_pfebs_ordered.erase(m_pfebs_ordered.begin());
      return pfeb.getAddress();
    }
  }
  // if youre here: that pfeb is not in the config db
  // warn the user, then move to the next pfeb
  std::string msg = next_feb + " does not exist in config. Skipping.";
  nsw::NSWsTGCTriggerCalibIssue issue(ERS_HERE, msg);
  ers::warning(issue);

  m_pfebs_ordered.erase(m_pfebs_ordered.begin());
  return next_pfeb(pop);
}
