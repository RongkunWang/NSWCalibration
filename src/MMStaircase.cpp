#include "NSWCalibration/MMStaircase.h"
using boost::property_tree::ptree;

nsw::MMStaircase::MMStaircase(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::MMStaircase::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 0;

  // parse calib type
  if (m_calibType=="MMStaircase") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for MMStaircase: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  // UPDATE THIS TO PICK UP ADDC FROM JSON FILE //
  m_addcs = nsw::ConfigReader::makeObjects<nsw::ADDCConfig> (db, "ADDC");
  ERS_INFO("Found " << m_addcs.size() << " ADDC");

  // set number of iterations
  setTotal((int)(m_addcs.size()));
  setToggle(0);
  setWait4swROD(0);
  usleep(1e6);
}

// UPDATE THIS TO L351-360 in https://gitlab.cern.ch/atlas-muon-nsw-daq/NSWCalibration/-/blob/master/src/MMTriggerCalib.cpp
void nsw::MMStaircase::configure() {
  ERS_INFO("MMStaircase::configure " << counter());
  int wait = 10;
  bool success = 0;
  for (auto & addc : m_addcs) {
    auto name  = addc.getAddress();
    auto layer = "L" + std::to_string(counter());
    if (name.find(layer) != std::string::npos) {
      configure_addc(addc, wait);
      success = 1;
      break;
    }
  }
  if (!success) {
    ERS_INFO("Warning: no ADDC for Layer " << counter());
    usleep(wait * 1e6);
  }
  usleep(wait * 1e6);
}

void nsw::MMStaircase::unconfigure() {
  ERS_INFO("MMStaircase::unconfigure " << counter());
}

int nsw::MMStaircase::configure_addc(const nsw::ADDCConfig & addc, int hold_reset) {
  //ERS_INFO("Configuring " << addc.getAddress());
  auto cs = std::make_unique<nsw::ConfigSender>();
  ERS_LOG("Writing ADDC config: " << addc.getAddress());
  if (!m_dry_run) {
    ERS_INFO("Line inside of configure_addc commented out for real run.");
    cs->sendAddcConfig(addc);
    // cs->sendRouterSoftReset(addc, hold_reset);
  }
  return 0;
}

std::string nsw::MMStaircase::strf_time() {
  std::stringstream ss;
  std::string out;
  std::time_t result = std::time(nullptr);
  std::tm tm = *std::localtime(&result);
  ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
  ss >> out;
  return out;
}
