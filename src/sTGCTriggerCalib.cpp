#include "NSWCalibration/sTGCTriggerCalib.h"
using boost::property_tree::ptree;

nsw::sTGCTriggerCalib::sTGCTriggerCalib(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCTriggerCalib::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 0;
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
  if (m_pts.size() > 1)
    throw std::runtime_error("I dont know how to process >1 PadTriggers!");

  // set number of loops in the iteration
  if (m_calibType=="sTGCPadConnectivity") {
    setTotal((int)(m_pfebs.size()));
  } else if (m_calibType=="sTGCPadLatency") {
    setTotal((int)(m_nbc_for_latency));
  }
  setToggle(0);
  setWait4swROD(0);

  // make ConfigSenders
  for (auto & feb : m_pfebs)
    m_senders.insert( {feb.getAddress(), std::make_unique<nsw::ConfigSender>()} );
  for (auto & pt : m_pts)
    m_senders.insert( {pt.getAddress(),  std::make_unique<nsw::ConfigSender>()} );
  usleep(1e6);
}

void nsw::sTGCTriggerCalib::configure() {
  ERS_INFO("sTGCTriggerCalib::configure " << counter());
  if (m_calibType=="sTGCPadConnectivity") {

    // test pulse one pfeb
    if (order_pfebs()) {
      auto next_addr = next_pfeb(0);
      for (auto & pfeb: m_pfebs) {
        if (next_addr.compare(pfeb.getAddress()) == 0) {
          configure_vmms(pfeb, 1);
          break;
        }
      }
    } else {
      auto & pfeb = m_pfebs.at((size_t)(counter()));
      configure_vmms(pfeb, 1);
    }

    configure_pad_trigger();
    usleep(500e3);

  } else if (m_calibType=="sTGCPadLatency") {

    // test pulse all pfebs
    if (counter() == 0) {
      ERS_INFO("sTGCTriggerCalib::configure all pFEBs");
      for (auto & pfeb: m_pfebs)
        configure_vmms(pfeb, 1);
    }

    // set pad trigger readout latency
    for (auto & pt: m_pts)
      pt.SetUserL1AReadoutLatency(latencyscan_current());
    configure_pad_trigger();
    usleep(1e6);

  }
}

void nsw::sTGCTriggerCalib::unconfigure() {
  ERS_INFO("sTGCTriggerCalib::unconfigure " << counter());

  if (m_calibType=="sTGCPadConnectivity") {
    if (order_pfebs()) {
      auto next_addr = next_pfeb(1);
      for (auto & pfeb: m_pfebs) {
        if (next_addr.compare(pfeb.getAddress()) == 0) {
          configure_vmms(pfeb, 0);
          break;
        }
      }
    } else {
      auto & pfeb = m_pfebs.at((size_t)(counter()));
      configure_vmms(pfeb, 0);
    }

    usleep(500e3);

  } else if (m_calibType=="sTGCPadLatency") {

    // mask pulse all pfebs
    if (counter() == total()-1) {
      ERS_INFO("sTGCTriggerCalib::unconfigure all pFEBs");
      for (auto & pfeb: m_pfebs)
          configure_vmms(pfeb, 0);
    }
  }
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
    auto & cs = m_senders[pt.getAddress()];
    ERS_INFO("Configuring " << pt.getOpcServerIp() << " " << pt.getAddress());

    // enable the L1A readout
    pt.SetUserL1AReadoutEnable();
    if (!m_dry_run)
        cs->sendPadTriggerSCAControlRegister(pt);

    // pause to collect L1As
    usleep(50e3);

    // disable the L1A readout
    pt.SetUserL1AReadoutDisable();
    if (!m_dry_run)
        cs->sendPadTriggerSCAControlRegister(pt);
  }
  return 0;
}

void nsw::sTGCTriggerCalib::gather_pfebs() {
  std::string partition(std::getenv("TDAQ_PARTITION"));
  ERS_INFO("Gather pFEBs: found partition " << partition);
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather pFEBs: VS pFEBs");
    m_pfebs_ordered.push_back("PFEB_L4Q2_IPR");
    m_pfebs_ordered.push_back("PFEB_L2Q2_IPR");
    m_pfebs_ordered.push_back("PFEB_L4Q1_IPR");
    m_pfebs_ordered.push_back("PFEB_L2Q1_IPR");
    // m_pfebs_ordered.push_back("PFEB_L2Q2_HOR");
    // m_pfebs_ordered.push_back("PFEB_L4Q2_HOR");
    // m_pfebs_ordered.push_back("PFEB_L2Q1_HOR");
    // m_pfebs_ordered.push_back("PFEB_L4Q1_HOR");
    // m_pfebs_ordered.push_back("PFEB_L2Q3_HOR");
    // m_pfebs_ordered.push_back("PFEB_L1Q3_HOL");
    // m_pfebs_ordered.push_back("PFEB_L4Q3_HOR");
    // m_pfebs_ordered.push_back("PFEB_L3Q3_HOL");
    m_pfebs_ordered.push_back("PFEB_L1Q3_IPL");
    m_pfebs_ordered.push_back("PFEB_L2Q3_IPR");
    m_pfebs_ordered.push_back("PFEB_L3Q3_IPL");
    m_pfebs_ordered.push_back("PFEB_L4Q3_IPR");
    // m_pfebs_ordered.push_back("PFEB_L3Q2_HOL");
    // m_pfebs_ordered.push_back("PFEB_L1Q2_HOL");
    // m_pfebs_ordered.push_back("PFEB_L3Q1_HOL");
    // m_pfebs_ordered.push_back("PFEB_L1Q1_HOL");
    m_pfebs_ordered.push_back("PFEB_L1Q2_IPL");
    m_pfebs_ordered.push_back("PFEB_L3Q2_IPL");
    m_pfebs_ordered.push_back("PFEB_L1Q1_IPL");
    m_pfebs_ordered.push_back("PFEB_L3Q1_IPL");
  }
  else {
    // 191
    ERS_INFO("Gather pFEBs: 191 pFEBs");
    m_pfebs_ordered.push_back("PFEB_L4Q2_IPL");
    m_pfebs_ordered.push_back("PFEB_L2Q2_IPL");
    m_pfebs_ordered.push_back("PFEB_L4Q1_IPL");
    m_pfebs_ordered.push_back("PFEB_L2Q1_IPL");

    m_pfebs_ordered.push_back("PFEB_L2Q2_HOL");
    m_pfebs_ordered.push_back("PFEB_L4Q2_HOL");
    m_pfebs_ordered.push_back("PFEB_L2Q1_HOL");
    m_pfebs_ordered.push_back("PFEB_L4Q1_HOL");

    m_pfebs_ordered.push_back("PFEB_L2Q3_HOL");
    m_pfebs_ordered.push_back("PFEB_L1Q3_HOR");
    m_pfebs_ordered.push_back("PFEB_L4Q3_HOL");
    m_pfebs_ordered.push_back("PFEB_L3Q3_HOR");

    m_pfebs_ordered.push_back("PFEB_L1Q3_IPR");
    m_pfebs_ordered.push_back("PFEB_L2Q3_IPL");
    m_pfebs_ordered.push_back("PFEB_L3Q3_IPR");
    m_pfebs_ordered.push_back("PFEB_L4Q3_IPL");

    m_pfebs_ordered.push_back("PFEB_L3Q2_HOR");
    m_pfebs_ordered.push_back("PFEB_L1Q2_HOR");
    m_pfebs_ordered.push_back("PFEB_L3Q1_HOR");
    m_pfebs_ordered.push_back("PFEB_L1Q1_HOR");

    m_pfebs_ordered.push_back("PFEB_L1Q2_IPR");
    m_pfebs_ordered.push_back("PFEB_L3Q2_IPR");
    m_pfebs_ordered.push_back("PFEB_L1Q1_IPR");
    m_pfebs_ordered.push_back("PFEB_L3Q1_IPR");
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
    if (next_feb.compare(pfeb.getAddress()) == 0) {
      if (pop)
        m_pfebs_ordered.erase(m_pfebs_ordered.begin());
      return next_feb;
    }
  }
  // crash
  std::string msg = next_feb + " does not exist in config. Crashing.";
  nsw::NSWsTGCTriggerCalibIssue issue(ERS_HERE, msg);
  ers::error(issue);
  throw std::runtime_error(msg);
  return "";
}
