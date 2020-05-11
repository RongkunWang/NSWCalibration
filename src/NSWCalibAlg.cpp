#include "NSWCalibration/NSWCalibAlg.h"

nsw::NSWCalibAlg::NSWCalibAlg() {
  ERS_INFO("NSWCalibAlg::NSWCalibAlg");
  setCounter(-1);
  setTotal(0);
}

void nsw::NSWCalibAlg::setup(std::string db) {
  ERS_INFO("setup");
}

void nsw::NSWCalibAlg::configure() {
  ERS_INFO("configure");
}

void nsw::NSWCalibAlg::unconfigure() {
  ERS_INFO("unconfigure");
}

bool nsw::NSWCalibAlg::next() {
  setCounter(counter() + 1);
  return counter() < total();
}
