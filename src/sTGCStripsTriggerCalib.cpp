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

#include <regex>
#include "NSWCalibration/sTGCStripsTriggerCalib.h"

nsw::sTGCStripsTriggerCalib::sTGCStripsTriggerCalib(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCStripsTriggerCalib::setup(std::string db) {
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
  for (auto & sfeb: m_sfebs)
   for (auto tds: {"tds0", "tds1", "tds2", "tds3"})
     configure_tds(sfeb, tds, 1);

  // set number of loops in the iteration
  setTotal((int)(m_sfebs_ordered.size() * m_tdss.size()));
  setToggle(simulation() ? 0 : 1);
  setWait4swROD(0);
  usleep(1e6);
}

void nsw::sTGCStripsTriggerCalib::configure() {
  ERS_INFO("sTGCStripsTriggerCalib::configure " << counter());
  int this_sfeb = counter() / m_tdss.size();
  int this_tdss = counter() % m_tdss.size();
  configure_tds(m_sfebs_ordered.at(this_sfeb), m_tdss.at(this_tdss), 0);
}

void nsw::sTGCStripsTriggerCalib::unconfigure() {
  ERS_INFO("sTGCStripsTriggerCalib::unconfigure " << counter());
  int this_sfeb = counter() / m_tdss.size();
  int this_tdss = counter() % m_tdss.size();
  configure_tds(m_sfebs_ordered.at(this_sfeb), m_tdss.at(this_tdss), 1);
}

int nsw::sTGCStripsTriggerCalib::configure_tds(std::string name,
                                               std::vector<std::string> tdss,
                                               bool prbs_e) {
  for (auto & sfeb: m_sfebs) {
    if (name == simplified(sfeb.getAddress())) {
      for (auto & tds: tdss) {
        if (dont_touch(name, tds)) {
          ERS_INFO("Skipping " << name << " " << tds);
        } else {
          configure_tds(sfeb, tds, prbs_e);
        }
      }
    }
  }
  usleep(prbs_e ? 1e6 : 1e6);
  return 0;
}

int nsw::sTGCStripsTriggerCalib::configure_tds(nsw::FEBConfig feb,
                                               std::string tds,
                                               bool prbs_e) {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip = feb.getOpcServerIp();
  auto sca_address = feb.getAddress();
  for (auto tdsi2c : feb.getTdss()) {
    if(tdsi2c.getName() != tds)
      continue;
    ERS_INFO("Configuring" 
             << " " << opc_ip
             << " " << sca_address
             << " " << tdsi2c.getName()
             << " -> PRBS_e = " << prbs_e
             );
    tdsi2c.setRegisterValue("register12", "PRBS_e", (int)(prbs_e));
    if (!simulation())
      cs->sendI2cMasterSingle(opc_ip, sca_address, tdsi2c, "register12");
  }
  return 0;
}

std::string nsw::sTGCStripsTriggerCalib::simplified(std::string name) {
  std::string ret = std::string(name);
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
  if (!part)
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

bool nsw::sTGCStripsTriggerCalib::dont_touch(std::string name, std::string tds) {
  for (auto & sfeb_tds: router_recovery_tds())
    if (sfeb_tds.first == name && sfeb_tds.second == tds)
      return 1;
  return 0;
}

