#include "NSWCalibration/sTGCsFEBToRouter.h"
using boost::property_tree::ptree;

nsw::sTGCsFEBToRouter::sTGCsFEBToRouter(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCsFEBToRouter::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 1;

  // parse calib type
  if (m_calibType=="sTGCsFEBToRouter") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCsFEBToRouter: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  // can be sFEB, sFEB8, or sFEB6 :(
  m_routers = nsw::ConfigReader::makeObjects<nsw::RouterConfig> (db, "Router");
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB8"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB6"))
    m_sfebs.push_back(feb);
  ERS_INFO("Found " << m_routers.size() << " Routers");
  ERS_INFO("Found " << m_sfebs.size()   << " sFEBs");

  // start dog
  m_watchdog = std::async(std::launch::async, &nsw::sTGCsFEBToRouter::router_watchdog, this);

  // set number of iterations
  gather_sfebs();
  setTotal((int)(m_sfebs_ordered.size()));
  setToggle(0);
  setWait4swROD(0);
  usleep(1e6);
}

void nsw::sTGCsFEBToRouter::configure() {
  ERS_INFO("sTGCsFEBToRouter::configure " << counter());
  auto name = m_sfebs_ordered.at(counter());
  for (auto & sfeb: m_sfebs)
    if (name == sfeb.getAddress())
      configure_tds(sfeb, 1);
  usleep(5e6);
}

void nsw::sTGCsFEBToRouter::unconfigure() {
  ERS_INFO("sTGCsFEBToRouter::unconfigure " << counter());
  auto name = m_sfebs_ordered.at(counter());
  for (auto & sfeb: m_sfebs)
    if (name == sfeb.getAddress())
      configure_tds(sfeb, 0);
  usleep(5e6);
}

int nsw::sTGCsFEBToRouter::configure_tds(const nsw::FEBConfig & feb, bool enable) {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip = feb.getOpcServerIp();
  auto sca_address = feb.getAddress();
  for (auto tds : feb.getTdss()) {
    ERS_INFO("Configuring " << feb.getOpcServerIp()
             << " " << feb.getAddress()
             << " " << tds.getName()
             << " -> " << (enable ? "enable PRBS" : "disable PRBS")
             );
    tds.setRegisterValue("register12", "PRBS_e", (int)(enable));
    if (!m_dry_run)
      cs->sendI2cMasterSingle(opc_ip, sca_address, tds, "register12");
  }
  return 0;
}

void nsw::sTGCsFEBToRouter::gather_sfebs() {
  std::string partition(std::getenv("TDAQ_PARTITION"));
  ERS_INFO("Gather sFEBs: found partition " << partition);
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather sfebs: VS sfebs");
    m_sfebs_ordered.push_back("SFEB_L1Q1_IPL");
    m_sfebs_ordered.push_back("SFEB_L2Q1_IPR");
    m_sfebs_ordered.push_back("SFEB_L3Q1_IPL");
    m_sfebs_ordered.push_back("SFEB_L4Q1_IPR");
  } else {
    // 191
    m_sfebs_ordered.push_back("SFEB_L1Q1_IPL");
    m_sfebs_ordered.push_back("SFEB_L2Q1_IPR");
    m_sfebs_ordered.push_back("SFEB_L3Q1_IPL");
    m_sfebs_ordered.push_back("SFEB_L4Q1_IPR");
    m_sfebs_ordered.push_back("SFEB_L1Q1_HOL");
    m_sfebs_ordered.push_back("SFEB_L2Q1_HOR");
    m_sfebs_ordered.push_back("SFEB_L3Q1_HOL");
    m_sfebs_ordered.push_back("SFEB_L4Q1_HOR");
  }
}

int nsw::sTGCsFEBToRouter::router_watchdog() {
    //
    // Be forewarned: this function reads Router SCA registers.
    // Dont race elsewhere.
    //
  if (m_routers.size() == 0)
    return 0;

  // sleep time
  size_t slp = 1e5;

  // output file and announce
  std::string fname = "router_ClkReady_" + strf_time() + ".txt";
  std::ofstream myfile;
  myfile.open(fname);
  ERS_INFO("Router ClkReady watchdog. Output: " << fname << ". Sleep: " << slp << "s");

  // monitor
  auto threads = std::make_unique<std::vector< std::future<bool> > >();
  while (counter() < total()) {
    myfile << "Time " << strf_time() << std::endl;
    for (auto & router : m_routers)
      threads->push_back( std::async(std::launch::async,
                                     &nsw::sTGCsFEBToRouter::router_ClkReady,
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

bool nsw::sTGCsFEBToRouter::router_ClkReady(const nsw::RouterConfig & router) {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip   = router.getOpcServerIp();
  auto sca_addr = router.getAddress();
  auto rx_addr  = sca_addr + ".gpio." + "rxClkReady";
  auto tx_addr  = sca_addr + ".gpio." + "txClkReady";
  auto rx_val   = m_dry_run ? 0 : cs->readGPIO(opc_ip, rx_addr);
  auto tx_val   = m_dry_run ? 0 : cs->readGPIO(opc_ip, tx_addr);
  return rx_val && tx_val;
}

std::string nsw::sTGCsFEBToRouter::strf_time() {
    std::stringstream ss;
    std::string out;
    std::time_t result = std::time(nullptr);
    std::tm tm = *std::localtime(&result);
    ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
    ss >> out;
    return out;
}
