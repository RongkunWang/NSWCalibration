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

  // parse calib type
  if (m_calibType=="sTGCSFEBToRouter"   ||
      m_calibType=="sTGCSFEBToRouterQ1" ||
      m_calibType=="sTGCSFEBToRouterQ2" ||
      m_calibType=="sTGCSFEBToRouterQ3") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCSFEBToRouter: "
        << m_calibType << ". Crashing.";
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

  // set number of iterations
  gather_sfebs();
  setTotal((int)(m_sfebs_ordered.size()));
  setToggle(false);
  setWait4swROD(false);
  std::this_thread::sleep_for(std::chrono::seconds{1});

  // count the number of routers
  //   at start of run
  m_routers_at_start = count_ready_routers();
  ERS_INFO("At start of run, found " << m_routers_at_start
           << " Routers which have ClkReady = true");
}

void nsw::sTGCSFEBToRouter::configure() {
  ERS_INFO("sTGCSFEBToRouter::configure " << counter());
  const bool prbs  = true;
  const bool open  = counter() == 0;
  const bool close = false;
  const auto name = m_sfebs_ordered.at(counter());
  for (const auto & sfeb: m_sfebs)
    if (sfeb.getAddress().find(name) != std::string::npos)
      configure_tds(sfeb, prbs);
  configure_routers();
  wait_for_routers(m_routers_at_start - 1);
  router_watchdog(open, close);
}

void nsw::sTGCSFEBToRouter::unconfigure() {
  ERS_INFO("sTGCSFEBToRouter::unconfigure " << counter());
  const bool prbs  = false;
  const bool open  = false;
  const bool close = counter() == total() - 1;
  const auto name = m_sfebs_ordered.at(counter());
  for (const auto & sfeb: m_sfebs)
    if (sfeb.getAddress().find(name) != std::string::npos)
      configure_tds(sfeb, prbs);
  configure_routers();
  wait_for_routers(m_routers_at_start);
  router_watchdog(open, close);
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
    constexpr std::chrono::seconds reset_hold{0};
    constexpr std::chrono::seconds reset_sleep{1};
    auto cs = std::make_unique<nsw::ConfigSender>();
    if (!simulation())
      cs->sendRouterSoftReset(router, reset_hold, reset_sleep);
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
    if (!simulation())
      cs->sendI2cMasterSingle(opc_ip, sca_address, tds, reg);
  }
  return 0;
}

void nsw::sTGCSFEBToRouter::gather_sfebs() {
  //
  // get the partition environment
  //
  const auto part = std::getenv("TDAQ_PARTITION");
  if (part == nullptr)
    throw std::runtime_error("Error: TDAQ_PARTITION not defined");
  const std::string partition(part);
  const std::string sector_name = nsw::guessSector(applicationName());
  ERS_INFO("Gather SFEBs: found partition "  << partition);
  ERS_INFO("Gather SFEBs: application "      << applicationName());
  ERS_INFO("Gather SFEBs: sector name "      << sector_name);
  ERS_INFO("Gather SFEBs: sector is large: " << nsw::isLargeSector(sector_name));

  //
  // order the FEBs accordingly
  //
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
      msg << "Unknown m_calib for sTGCSFEBToRouter::gather_sfebs: "
          << m_calibType << ". Crashing.";
      ERS_INFO(msg.str());
      throw std::runtime_error(msg.str());
    }
  }
}

int nsw::sTGCSFEBToRouter::router_watchdog(bool open, bool close) {
  //
  // Be forewarned: this function reads Router SCA registers.
  // Dont race elsewhere.
  //
  if (m_routers.size() == 0)
    return 0;

  // output file and announce
  if (open) {
    m_fname = "router_ClkReady_" + nsw::calib::utils::strf_time() + ".txt";
    m_myfile.open(m_fname);
    ERS_INFO("Router ClkReady watchdog. Output: " << m_fname);
  }

  // read once
  auto threads = std::make_unique<std::vector< std::future<bool> > >();
  m_myfile << "Time " << nsw::calib::utils::strf_time() << std::endl;
  for (const auto & router : m_routers)
    threads->push_back( std::async(std::launch::async,
                                   &nsw::sTGCSFEBToRouter::router_ClkReady,
                                   this,
                                   router) );
  for (size_t ir = 0; ir < m_routers.size(); ir++) {
    auto name = m_routers.at(ir).getAddress();
    auto val  = threads ->at(ir).get();
    m_myfile << name << " " << val << std::endl;
  }
  threads->clear();

  // close
  if (close) {
    ERS_INFO("Closing " << m_fname);
    m_myfile.close();
  }

  return 0;
}

void nsw::sTGCSFEBToRouter::wait_for_routers(size_t expectation) const {
  size_t count = 0;
  while (count < expectation) {
    ERS_INFO("Waiting for Routers to be ready. Currently: "
             << count << " of " << expectation << " ok");
    count = count_ready_routers();
    std::this_thread::sleep_for(std::chrono::seconds{1});
  }
}

size_t nsw::sTGCSFEBToRouter::count_ready_routers() const {
  size_t count = 0;

  // read the router GPIO
  auto threads = std::vector< std::future<bool> >();
  for (const auto & router : m_routers) {
    threads.push_back( std::async(std::launch::async,
                                  &nsw::sTGCSFEBToRouter::router_ClkReady,
                                  this,
                                  router) );
  }

  // count the number of ready routers
  for (auto& thr : threads) {
    if (thr.get()) {
      count++;
    }
  }
  threads.clear();

  // return
  return count;
}

bool nsw::sTGCSFEBToRouter::router_ClkReady(const nsw::RouterConfig & router) const {
  const auto cs = std::make_unique<nsw::ConfigSender>();
  const auto opc_ip   = router.getOpcServerIp();
  const auto sca_addr = router.getAddress();
  const auto rx_addr  = sca_addr + ".gpio." + "rxClkReady";
  const auto tx_addr  = sca_addr + ".gpio." + "txClkReady";
  const auto rx_val   = simulation() ? true : cs->readGPIO(opc_ip, rx_addr);
  const auto tx_val   = simulation() ? true : cs->readGPIO(opc_ip, tx_addr);
  bool ok = rx_val && tx_val;
  if (!ok) {
    ERS_INFO("ClkReady=0 for " << opc_ip << "." << sca_addr);
  }
  return ok;
}
