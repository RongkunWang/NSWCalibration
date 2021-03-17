#include "NSWCalibration/sTGCSFEBToRouter.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/I2cMasterConfig.h"

#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <stdexcept>

#include "ers/ers.h"

nsw::sTGCSFEBToRouter::sTGCSFEBToRouter(const std::string& calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCSFEBToRouter::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  m_dry_run = false;

  // parse calib type
  if (m_calibType=="sTGCSFEBToRouter"   ||
      m_calibType=="sTGCSFEBToRouterQ1" ||
      m_calibType=="sTGCSFEBToRouterQ2" ||
      m_calibType=="sTGCSFEBToRouterQ3") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCSFEBToRouter: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  // can be SFEB, SFEB8, or SFEB6 :(
  m_routers = nsw::ConfigReader::makeObjects<nsw::RouterConfig> (db, "Router");
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB8"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB6"))
    m_sfebs.push_back(feb);
  ERS_INFO("Found " << m_routers.size() << " Routers");
  ERS_INFO("Found " << m_sfebs.size()   << " SFEBs");

  // start dog
  m_watchdog = std::async(std::launch::async, &nsw::sTGCSFEBToRouter::router_watchdog, this);

  // set number of iterations
  gather_sfebs();
  setTotal((int)(m_sfebs_ordered.size()));
  setToggle(false);
  setWait4swROD(false);
  usleep(1e6);
}

void nsw::sTGCSFEBToRouter::configure() {
  ERS_INFO("sTGCSFEBToRouter::configure " << counter());
  auto name = m_sfebs_ordered.at(counter());
  for (auto & sfeb: m_sfebs)
    if (sfeb.getAddress().find(name) != std::string::npos)
      configure_tds(sfeb, true);
  configure_routers();
  usleep(15e6);
}

void nsw::sTGCSFEBToRouter::unconfigure() {
  ERS_INFO("sTGCSFEBToRouter::unconfigure " << counter());
  auto name = m_sfebs_ordered.at(counter());
  for (auto & sfeb: m_sfebs)
    if (sfeb.getAddress().find(name) != std::string::npos)
      configure_tds(sfeb, false);
  configure_routers();
  usleep(15e6);
}

int nsw::sTGCSFEBToRouter::configure_routers() const {
    auto threads = std::make_unique<std::vector< std::future<int> > >();
    for (auto & router : m_routers)
        threads->push_back( std::async(std::launch::async,
                                       &nsw::sTGCSFEBToRouter::configure_router,
                                       this,
                                       router) );
    for (auto& thread : *threads)
        thread.get();
    return 0;
}

int nsw::sTGCSFEBToRouter::configure_router(const nsw::RouterConfig & router) const {
    ERS_INFO("Configuring " << router.getAddress());
    auto cs = std::make_unique<nsw::ConfigSender>();
    if (!m_dry_run)
        cs->sendRouterConfig(router);
    return 0;
}

int nsw::sTGCSFEBToRouter::configure_tds(const nsw::FEBConfig & feb, bool enable) const {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip = feb.getOpcServerIp();
  auto sca_address = feb.getAddress();
  std::string reg = "register12";
  std::string subreg = "PRBS_e";
  for (auto tds : feb.getTdss()) {
    ERS_INFO("Configuring " << feb.getOpcServerIp()
             << " " << feb.getAddress()
             << " " << tds.getName()
             << " -> " << (enable ? "enable PRBS" : "disable PRBS")
             );
    tds.setRegisterValue(reg, subreg, enable ? 1 : 0);
    if (!m_dry_run)
      cs->sendI2cMasterSingle(opc_ip, sca_address, tds, reg);
  }
  return 0;
}

void nsw::sTGCSFEBToRouter::gather_sfebs() {
  std::string partition(std::getenv("TDAQ_PARTITION"));
  ERS_INFO("Gather SFEBs: found partition " << partition);
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather sfebs: VS sfebs");
    m_sfebs_ordered.push_back("L1Q1_IP");
    m_sfebs_ordered.push_back("L2Q1_IP");
    m_sfebs_ordered.push_back("L3Q1_IP");
    m_sfebs_ordered.push_back("L4Q1_IP");
  } else {
    // 191
    if (m_calibType=="sTGCSFEBToRouter" ||
        m_calibType=="sTGCSFEBToRouterQ1") {
      m_sfebs_ordered.push_back("L1Q1_IP");
      m_sfebs_ordered.push_back("L2Q1_IP");
      m_sfebs_ordered.push_back("L3Q1_IP");
      m_sfebs_ordered.push_back("L4Q1_IP");
      m_sfebs_ordered.push_back("L1Q1_HO");
      m_sfebs_ordered.push_back("L2Q1_HO");
      m_sfebs_ordered.push_back("L3Q1_HO");
      m_sfebs_ordered.push_back("L4Q1_HO");
    } else if (m_calibType=="sTGCSFEBToRouterQ2") {
      m_sfebs_ordered.push_back("L1Q2_IP");
      m_sfebs_ordered.push_back("L2Q2_IP");
      m_sfebs_ordered.push_back("L3Q2_IP");
      m_sfebs_ordered.push_back("L4Q2_IP");
      m_sfebs_ordered.push_back("L1Q2_HO");
      m_sfebs_ordered.push_back("L2Q2_HO");
      m_sfebs_ordered.push_back("L3Q2_HO");
      m_sfebs_ordered.push_back("L4Q2_HO");
    } else if (m_calibType=="sTGCSFEBToRouterQ3") {
      m_sfebs_ordered.push_back("L1Q3_IP");
      m_sfebs_ordered.push_back("L2Q3_IP");
      m_sfebs_ordered.push_back("L3Q3_IP");
      m_sfebs_ordered.push_back("L4Q3_IP");
      m_sfebs_ordered.push_back("L1Q3_HO");
      m_sfebs_ordered.push_back("L2Q3_HO");
      m_sfebs_ordered.push_back("L3Q3_HO");
      m_sfebs_ordered.push_back("L4Q3_HO");
    } else {
      std::stringstream msg;
      msg << "Unknown m_calib for sTGCSFEBToRouter::gather_sfebs: " << m_calibType << ". Crashing.";
      ERS_INFO(msg.str());
      throw std::runtime_error(msg.str());
    }
  }
}

int nsw::sTGCSFEBToRouter::router_watchdog() const {
    //
    // Be forewarned: this function reads Router SCA registers.
    // Dont race elsewhere.
    //
  if (m_routers.size() == 0)
    return 0;

  // sleep time
  size_t slp = 1e5;

  // output file and announce
  std::string fname = "router_ClkReady_" + nsw::calib::utils::strf_time() + ".txt";
  std::ofstream myfile;
  myfile.open(fname);
  ERS_INFO("Router ClkReady watchdog. Output: " << fname << ". Sleep: " << slp << "s");

  // monitor
  auto threads = std::make_unique<std::vector< std::future<bool> > >();
  while (counter() < total()) {
    myfile << "Time " << nsw::calib::utils::strf_time() << std::endl;
    for (auto & router : m_routers)
      threads->push_back( std::async(std::launch::async,
                                     &nsw::sTGCSFEBToRouter::router_ClkReady,
                                     this,
                                     router) );
    for (size_t ir = 0; ir < m_routers.size(); ir++) {
      auto name = m_routers.at(ir).getAddress();
      auto val  = threads ->at(ir).get();
      myfile << name << " " << val << std::endl;
    }
    threads->clear();
    usleep(slp);
  }

  // close
  ERS_INFO("Closing " << fname);
  myfile.close();
  return 0;
}

bool nsw::sTGCSFEBToRouter::router_ClkReady(const nsw::RouterConfig & router) const {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip   = router.getOpcServerIp();
  auto sca_addr = router.getAddress();
  auto rx_addr  = sca_addr + ".gpio." + "rxClkReady";
  auto tx_addr  = sca_addr + ".gpio." + "txClkReady";
  auto rx_val   = m_dry_run ? false : cs->readGPIO(opc_ip, rx_addr);
  auto tx_val   = m_dry_run ? false : cs->readGPIO(opc_ip, tx_addr);
  return rx_val && tx_val;
}
