#include "NSWCalibration/sTGCTriggerCalib.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Constants.h"

#include <cstdlib>
#include <unistd.h>
#include <stdexcept>

#include "ers/ers.h"

void nsw::sTGCTriggerCalib::setup(const std::string& db) {
  ERS_INFO("setup " << db);

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
  for (auto pt: nsw::ConfigReader::makeObjects<nsw::hw::PadTrigger>(db, "PadTrigger")) {
    m_pts.emplace_back(pt);
  }
  ERS_INFO("Found " << m_pfebs.size() << " pFEBs");
  ERS_INFO("Found " << m_pts.size()   << " pad triggers");
  if (m_pts.size() != 1) {
    std::stringstream msg;
    msg << "I dont know how to process !=1 PadTriggers. You gave: " << m_pts.size();
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // get list of ordered PFEBs
  gatherPFEBs();

  // get latency scan parameters from user
  for (const auto& pt: m_pts) {
    setLatencyScanOffset(pt.LatencyScanStart());
    setLatencyScanNBC(pt.LatencyScanNBC());
  }

  // set number of loops in the iteration
  if (m_calibType=="sTGCPadConnectivity") {
    setTotal(static_cast<int>(m_pfebs.size()));
  } else if (m_calibType=="sTGCPadLatency") {
    setTotal(latencyScanNBC());
    ERS_INFO("Latency scan start: " << latencyScanOffset());
    ERS_INFO("Latency scan steps: " << latencyScanNBC());
  }

  nsw::snooze();
}

void nsw::sTGCTriggerCalib::configure() {
  ERS_INFO("sTGCTriggerCalib::configure " << counter());
  if (m_calibType=="sTGCPadConnectivity") {

    // test pulse one pfeb
    if (orderPFEBs()) {
      auto next_addr = nextPFEB(false);
      for (auto & pfeb: m_pfebs) {
        if (next_addr == pfeb.getAddress()) {
          configureVMMs(pfeb, true);
          break;
        }
      }
    } else {
      auto & pfeb = m_pfebs.at(static_cast<size_t>(counter()));
      configureVMMs(pfeb, true);
    }

    configurePadTrigger();
    usleep(500e3);

  } else if (m_calibType=="sTGCPadLatency") {

    // test pulse all pfebs
    if (counter() == 0) {
      ERS_INFO("sTGCTriggerCalib::configure all pFEBs");
      for (auto & pfeb: m_pfebs)
        configureVMMs(pfeb, true);
    }

    // set readout latency
    for (const auto& pt: m_pts) {
      if (!simulation()) {
        pt.writeReadoutBCOffset(latencyScanCurrent());
      }
    }
    configurePadTrigger();
    usleep(1e6);
    usleep(5e6);
  }
}

void nsw::sTGCTriggerCalib::unconfigure() {
  ERS_INFO("sTGCTriggerCalib::unconfigure " << counter());

  if (m_calibType=="sTGCPadConnectivity") {
    if (orderPFEBs()) {
      auto next_addr = nextPFEB(true);
      for (auto & pfeb: m_pfebs) {
        if (next_addr == pfeb.getAddress()) {
          configureVMMs(pfeb, false);
          break;
        }
      }
    } else {
      auto & pfeb = m_pfebs.at(static_cast<size_t>(counter()));
      configureVMMs(pfeb, false);
    }

    usleep(500e3);

  } else if (m_calibType=="sTGCPadLatency") {

    // mask pulse all pfebs
    if (counter() == total()-1) {
      ERS_INFO("sTGCTriggerCalib::unconfigure all pFEBs");
      for (auto & pfeb: m_pfebs)
          configureVMMs(pfeb, false);
    }
  }
}

int nsw::sTGCTriggerCalib::configureVMMs(nsw::FEBConfig feb, bool unmask) {
  ERS_INFO("Configuring " << feb.getOpcServerIp() << " " << feb.getAddress() << " - " << (unmask ? "pulsing" : "masking"));
  for (auto& vmm : feb.getVmms()) {
    vmm.setChannelRegisterAllChannels("channel_st", unmask ? 1 : 0);
    vmm.setChannelRegisterAllChannels("channel_sm", unmask ? 0 : 1);
  }
  auto cs = std::make_unique<nsw::ConfigSender>();
  if (!simulation())
    cs->sendVmmConfig(feb);
  return 0;
}

int nsw::sTGCTriggerCalib::configurePadTrigger() const {
  for (const auto& pt: m_pts) {
    ERS_INFO("Configuring " << pt.getName());

    // enable the L1A readout
    if (!simulation()) {
      pt.writeReadoutEnable();
    }

    // pause to collect L1As
    nsw::snooze(std::chrono::milliseconds(50));

    // disable the L1A readout
    if (!simulation()) {
      pt.writeReadoutDisable();
    }
  }
  return 0;
}

void nsw::sTGCTriggerCalib::gatherPFEBs() {
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
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather pFEBs: VS pFEBs");
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
  }
  else {
    // 191/P1
    for (const auto& pfeb: nsw::padtrigger::ORDERED_PFEBS) {
      m_pfebs_ordered.push_back(std::string(pfeb));
    }
  }
}

std::string nsw::sTGCTriggerCalib::nextPFEB(bool pop) {
  if (m_pfebs_ordered.size() == 0) {
    const auto msg = "Error in nextPFEB. vector is empty, cant get next element.";
    nsw::NSWsTGCTriggerCalibIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw std::runtime_error(msg);
  }
  // get the next valid pfeb
  // "valid" means: exists in the config
  const auto next_feb = m_pfebs_ordered.front();
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
  return nextPFEB(pop);
}
