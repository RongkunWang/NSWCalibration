#include "NSWCalibration/sTGCStripsTriggerCalib.h"
using boost::property_tree::ptree;

nsw::sTGCStripsTriggerCalib::sTGCStripsTriggerCalib(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCStripsTriggerCalib::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 1;
  gather_sfebs();

  // parse calib type
  if (m_calibType=="sTGCFakeStripConnectivity") {
    ERS_INFO(m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCStripsTriggerCalib: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  ERS_INFO("Making sFEB");
  m_sfebs = nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB");
  ERS_INFO("Found " << m_sfebs.size() << " sFEBs");

  // set number of loops in the iteration
  m_patterns = patterns();
  write_json("strips.json", m_patterns);
  setTotal((int)(m_patterns.size()));
  setToggle(0);
  setWait4swROD(0);
  usleep(1e6);
}

void nsw::sTGCStripsTriggerCalib::configure() {
  ERS_INFO("sTGCStripsTriggerCalib::configure " << counter());
  for (auto toppattkv : m_patterns) {
    int ipatt = pattern_number(toppattkv.first);
    if (ipatt != counter())
      continue;
    configure_tds(toppattkv.second, 1);
  }
}

void nsw::sTGCStripsTriggerCalib::unconfigure() {
  ERS_INFO("sTGCStripsTriggerCalib::unconfigure " << counter());
  for (auto toppattkv : m_patterns) {
    int ipatt = pattern_number(toppattkv.first);
    if (ipatt != counter())
      continue;
    configure_tds(toppattkv.second, 0);
  }
}

int nsw::sTGCStripsTriggerCalib::configure_tds(ptree tr, bool unmask) {
  auto name = tr.get<std::string>("sfeb");
  auto tds  = tr.get<int>("tds");
  ERS_INFO("Configure " << name << ", TDS " << std::to_string(tds));
  for (auto & sfeb: m_sfebs)
    if (name == sfeb.getAddress())
      configure_tds(sfeb, tds, unmask);
  usleep(unmask ? 5e6 : 1e6);
  return 0;
}

int nsw::sTGCStripsTriggerCalib::configure_tds(nsw::FEBConfig feb, int i_tds, bool unmask) {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip = feb.getOpcServerIp();
  auto sca_address = feb.getAddress();
  for (auto tds : feb.getTdss()) {
    if(tds.getName().find(std::to_string(i_tds)) == std::string::npos)
      continue;
    ERS_INFO("Configuring " << feb.getOpcServerIp()
             << " " << feb.getAddress()
             << " " << tds.getName()
             << " -> " << (unmask ? "sending" : "masking")
             );
    tds.setRegisterValue("register12", "test_frame2Router_enable", (int)(unmask));
    if (!m_dry_run)
      cs->sendI2cMasterSingle(opc_ip, sca_address, tds, "register12");
  }
  return 0;
}

void nsw::sTGCStripsTriggerCalib::gather_sfebs() {
  std::string partition(std::getenv("TDAQ_PARTITION"));
  ERS_INFO("Gather sFEBs: found partition " << partition);
  if (partition.find("VS") != std::string::npos) {
    // VS
    ERS_INFO("Gather sfebs: VS sfebs");
    m_sfebs_ordered.push_back("SFEB_L3Q2_IPL");
    m_sfebs_ordered.push_back("SFEB_L1Q2_IPL");
    m_sfebs_ordered.push_back("SFEB_L3Q1_IPL");
    m_sfebs_ordered.push_back("SFEB_L1Q1_IPL");
    m_sfebs_ordered.push_back("SFEB_L3Q3_IPL");
    m_sfebs_ordered.push_back("SFEB_L4Q3_IPR");
    m_sfebs_ordered.push_back("SFEB_L1Q3_IPL");
    m_sfebs_ordered.push_back("SFEB_L2Q3_IPR");
    m_sfebs_ordered.push_back("SFEB_L4Q2_IPR");
    m_sfebs_ordered.push_back("SFEB_L2Q2_IPR");
    m_sfebs_ordered.push_back("SFEB_L2Q1_IPR");
    m_sfebs_ordered.push_back("SFEB_L4Q1_IPR");
  }
}

int nsw::sTGCStripsTriggerCalib::pattern_number(std::string name) {
  return std::stoi( std::regex_replace(name, std::regex("pattern_"), "") );
}

ptree nsw::sTGCStripsTriggerCalib::patterns() {
  ptree patts;
  int ipatts = 0;
  int n_tds = 4;
  for (auto febname: m_sfebs_ordered) {
    for (int i_tds = 0; i_tds < n_tds; i_tds++) {
      ptree top_patt;
      top_patt.put("sfeb", febname);
      top_patt.put("tds", i_tds);
      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
      ipatts++;
    }
  }
  return patts;
}
