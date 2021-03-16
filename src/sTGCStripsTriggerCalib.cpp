//
// to start:
//    Set all TDS to PRBS mode,
//    Except the special TDS (tds1 for Router clock recovery)
//    The special TDS are never touched.
// then:
//   for each SFEB
//     0. TDS connectivity: Check each TDS
//     for each TDS
//       disable PRBS
//       enable PRBS
//     1. Router connectivity: Check multiple Router fibers
//     enable TDS1, TDS2, TDS3
//     disable TDS1, TDS2, TDS3
//

#include "NSWCalibration/sTGCStripsTriggerCalib.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

#include <cstdlib>
#include <unistd.h>
#include <regex>
#include <stdexcept>

#include "ers/ers.h"

nsw::sTGCStripsTriggerCalib::sTGCStripsTriggerCalib(const std::string& calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCStripsTriggerCalib::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  // parse calib type
  if (m_calibType=="sTGCStripConnectivity") {
    ERS_INFO(m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calib in sTGCStripsTriggerCalib: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  ERS_INFO("Making sFEB");
  m_sfebs.clear();
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB8"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB6"))
    m_sfebs.push_back(feb);
  ERS_INFO("Found " << m_sfebs.size() << " sFEBs");

  // sequence of SFEB to probe
  gather_sfebs();

  // sequence of TDS to probe
  m_tdss.clear();
  m_tdss.push_back( {"tds0"} );
  m_tdss.push_back( {"tds1"} );
  m_tdss.push_back( {"tds2"} );
  m_tdss.push_back( {"tds3"} );
  m_tdss.push_back( {"tds1", "tds2", "tds3"} );

  // enable PRBS everywhere (except important TDS)
  for (auto & name: m_sfebs_ordered)
    for (auto tds: m_tdss)
      if (tds.size() == 1)
        configure_tds(name, tds, true, false);

  // set number of loops in the iteration
  setTotal(static_cast<int>(m_sfebs_ordered.size() * m_tdss.size()));
  setToggle(false);
  setWait4swROD(false);
  usleep(1e6);
}

void nsw::sTGCStripsTriggerCalib::configure() {
  ERS_INFO("sTGCStripsTriggerCalib::configure " << counter());
  const int this_sfeb = counter() / m_tdss.size();
  const int this_tdss = counter() % m_tdss.size();
  configure_tds(m_sfebs_ordered.at(this_sfeb), m_tdss.at(this_tdss), false, true);
}

void nsw::sTGCStripsTriggerCalib::unconfigure() {
  ERS_INFO("sTGCStripsTriggerCalib::unconfigure " << counter());
  const int this_sfeb = counter() / m_tdss.size();
  const int this_tdss = counter() % m_tdss.size();
  configure_tds(m_sfebs_ordered.at(this_sfeb), m_tdss.at(this_tdss), true, true);
}

int nsw::sTGCStripsTriggerCalib::configure_tds(const std::string& name,
                                               const std::vector<std::string>& tdss,
                                               bool prbs_e, bool pause) {
  for (auto & sfeb: m_sfebs) {
    if (name == simplified(sfeb.getAddress())) {
      for (auto & tds: tdss) {
        if (dont_touch(name, tds)) {
          ERS_INFO("Skipping " << name << " " << tds << " (Router clk)");
        } else {
          configure_tds(sfeb, tds, prbs_e);
        }
      }
    }
  }
  if (pause) {
    if (simulation())
      usleep(prbs_e ? 1e6 : 1e6);
    else
      usleep(prbs_e ? 15e6 : 15e6);
  }
  return 0;
}

int nsw::sTGCStripsTriggerCalib::configure_tds(const nsw::FEBConfig& feb,
                                               const std::string& tds,
                                               bool prbs_e) {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip = feb.getOpcServerIp();
  auto sca_address = feb.getAddress();
  bool exists = false;
  for (auto tdsi2c : feb.getTdss()) {
    if(tdsi2c.getName() != tds)
      continue;
    exists = true;
    ERS_INFO("Configuring"
             << " " << opc_ip
             << " " << sca_address
             << " " << tds
             << " -> PRBS_e = " << prbs_e
             );
    tdsi2c.setRegisterValue("register12", "PRBS_e", static_cast<int>(prbs_e));
    if (!simulation())
      cs->sendI2cMasterSingle(opc_ip, sca_address, tdsi2c, "register12");
  }
  if (!exists)
    ERS_INFO("Not configuring"
             << " " << opc_ip
             << " " << sca_address
             << " " << tds
             << " (N/A)"
             );
  return 0;
}

std::string nsw::sTGCStripsTriggerCalib::simplified(const std::string& name) const {
  auto ret = name;
  ret = std::regex_replace(ret, std::regex("SFEB8"), "SFEB");
  ret = std::regex_replace(ret, std::regex("SFEB6"), "SFEB");
  ret = std::regex_replace(ret, std::regex("IPR"),   "IP");
  ret = std::regex_replace(ret, std::regex("IPL"),   "IP");
  ret = std::regex_replace(ret, std::regex("HOR"),   "HO");
  ret = std::regex_replace(ret, std::regex("HOL"),   "HO");
  return ret;
}

void nsw::sTGCStripsTriggerCalib::gather_sfebs() {
  auto part = std::getenv("TDAQ_PARTITION");
  if (part == nullptr)
    throw std::runtime_error("Error: TDAQ_PARTITION not defined");
  std::string partition(part);
  ERS_INFO("Gather sFEBs: found partition " << partition);
  m_sfebs_ordered.clear();
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather sfebs: VS sfebs");
    m_sfebs_ordered.push_back("SFEB_L1Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L2Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L3Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L4Q1_IP");
  } else {
    // 191
    m_sfebs_ordered.push_back("SFEB_L1Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L1Q2_IP");
    m_sfebs_ordered.push_back("SFEB_L1Q3_IP");
    m_sfebs_ordered.push_back("SFEB_L2Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L2Q2_IP");
    m_sfebs_ordered.push_back("SFEB_L2Q3_IP");
    m_sfebs_ordered.push_back("SFEB_L3Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L3Q2_IP");
    m_sfebs_ordered.push_back("SFEB_L3Q3_IP");
    m_sfebs_ordered.push_back("SFEB_L4Q1_IP");
    m_sfebs_ordered.push_back("SFEB_L4Q2_IP");
    m_sfebs_ordered.push_back("SFEB_L4Q3_IP");
    m_sfebs_ordered.push_back("SFEB_L1Q1_HO");
    m_sfebs_ordered.push_back("SFEB_L1Q2_HO");
    m_sfebs_ordered.push_back("SFEB_L1Q3_HO");
    m_sfebs_ordered.push_back("SFEB_L2Q1_HO");
    m_sfebs_ordered.push_back("SFEB_L2Q2_HO");
    m_sfebs_ordered.push_back("SFEB_L2Q3_HO");
    m_sfebs_ordered.push_back("SFEB_L3Q1_HO");
    m_sfebs_ordered.push_back("SFEB_L3Q2_HO");
    m_sfebs_ordered.push_back("SFEB_L3Q3_HO");
    m_sfebs_ordered.push_back("SFEB_L4Q1_HO");
    m_sfebs_ordered.push_back("SFEB_L4Q2_HO");
    m_sfebs_ordered.push_back("SFEB_L4Q3_HO");
  }
}

std::vector<std::pair <std::string, std::string > > nsw::sTGCStripsTriggerCalib::router_recovery_tds() {
  if (m_router_recovery_tds.size() == 0) {
    // if A02, or logic like that
    // how to tell calib about the sector?
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L1Q1_IP", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L2Q1_IP", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L3Q1_IP", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L4Q1_IP", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L1Q1_HO", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L2Q1_HO", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L3Q1_HO", "tds1") );
    m_router_recovery_tds.push_back( std::make_pair("SFEB_L4Q1_HO", "tds1") );
  };
  return m_router_recovery_tds;
}

bool nsw::sTGCStripsTriggerCalib::dont_touch(const std::string& name, const std::string& tds) {
  for (auto & sfeb_tds: router_recovery_tds())
    if (sfeb_tds.first == name && sfeb_tds.second == tds)
      return true;
  return false;
}

