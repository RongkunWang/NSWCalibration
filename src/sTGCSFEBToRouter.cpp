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
#include <fmt/core.h>

#include "ers/ers.h"

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
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB8"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB6"))
    m_sfebs.push_back(feb);
  ERS_INFO("Found " << m_routers.get().size() << " Routers");
  ERS_INFO("Found " << m_sfebs.size()   << " SFEBs");

  // set number of iterations
  gather_sfebs();
  setTotal(m_sfebs_ordered.size());

  nsw::snooze();

  // count the number of routers
  //   at start of run
  m_routers_at_start = count_ready_routers();
  ERS_INFO("At start of run, found " << m_routers_at_start
           << " Routers which have ClkReady = true");

  // crash if highly suspicious
  if (m_routers_at_start == std::size_t{0}) {
    const auto msg{"Found 0 Routers which have ClkReady. Crashing."};
    nsw::NSWsTGCSFEBToRouterIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw issue;
  }
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
    for (const auto & router : m_routers.get())
        threads->push_back( std::async(std::launch::async,
                                       &nsw::sTGCSFEBToRouter::configure_router,
                                       this,
                                       router) );
    for (auto& thread : *threads)
        thread.get();
    return 0;
}

int nsw::sTGCSFEBToRouter::configure_router(const nsw::hw::Router& router) const {
    ERS_INFO("Configuring " << router.getName());
    if (!simulation()) {
        router.writeSoftResetAndCheckGPIO();
    }
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
  if (m_routers.get().size() == 0)
    return 0;

  // output file and announce
  if (open) {
    const auto now = nsw::calib::utils::strf_time();
    m_fname = "router_ClkReady." + std::to_string(runNumber()) + "." + applicationName() + "." + now + ".txt";
    m_myfile.open(m_fname);
    ERS_INFO("Router ClkReady watchdog. Output: " << m_fname);
  }

  // read once
  auto threads = std::make_unique<std::vector< std::future<bool> > >();
  m_myfile << "Time " << nsw::calib::utils::strf_time() << std::endl;
  for (const auto & router : m_routers.get())
    threads->push_back( std::async(std::launch::async,
                                   &nsw::sTGCSFEBToRouter::router_ClkReady,
                                   this,
                                   router) );
  for (size_t ir = 0; ir < m_routers.get().size(); ir++) {
    auto name = m_routers.get().at(ir).getConfig().getAddress();
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
  ERS_INFO(fmt::format("Finished waiting for Routers to be ready: {} of {} ok",
                       count, expectation));
}

size_t nsw::sTGCSFEBToRouter::count_ready_routers() const {
  size_t count = 0;

  // read the router GPIO
  auto threads = std::vector< std::future<bool> >();
  for (const auto & router : m_routers.get()) {
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

bool nsw::sTGCSFEBToRouter::router_ClkReady(const nsw::hw::Router& router) const {
  const auto rx_val = simulation() ? true : router.readGPIO("rxClkReady");
  const auto tx_val = simulation() ? true : router.readGPIO("txClkReady");
  const bool ok = rx_val && tx_val;
  if (!ok) {
    ERS_INFO("ClkReady=0 for " << router.getName());
  }
  return ok;
}
