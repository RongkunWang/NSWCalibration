#include "NSWCalibration/ROCCalib.h"
using boost::property_tree::ptree;

nsw::ROCCalib::ROCCalib(std::string calibType) {
  setCounter(-1);
  setTotal(1);
  setToggle(0);
  m_calibType = calibType;
}

void nsw::ROCCalib::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 0;

  // parse calib type
  if (m_calibType=="ROCPhase") {
    ERS_INFO(m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for ROCCalib: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

}


void nsw::ROCCalib::configure() {
  ERS_INFO("ROCCalib::configure " << counter());
  if (m_calibType=="ROCPhase") {
    ERS_INFO("Hi :)");
  }
}

void nsw::ROCCalib::unconfigure() {
  ERS_INFO("ROCCalib::unconfigure " << counter());
}

