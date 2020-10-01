#include "NSWCalibration/CalibAlg.h"

nsw::CalibAlg::CalibAlg() {
  ERS_INFO("CalibAlg::CalibAlg");
  setCounter(-1);
  setTotal(0);
}

void nsw::CalibAlg::setup(std::string db) {
  ERS_INFO("setup");
} 

void nsw::CalibAlg::configure() {
  ERS_INFO("configure");
}

void nsw::CalibAlg::configure(int i_par, bool pdo, bool tdo,int chan) { //my addition...
  ERS_INFO("configure");
}

void nsw::CalibAlg::unconfigure() {
  ERS_INFO("unconfigure");
} 

bool nsw::CalibAlg::next() {
  setCounter(counter() + 1);
  return counter() < total();
}
