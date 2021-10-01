#include "NSWCalibration/CalibAlg.h"

#include <iostream>
#include <iomanip>

#include <ers/ers.h>

nsw::CalibAlg::CalibAlg() {
  ERS_INFO("CalibAlg::CalibAlg");
  setCounter(-1);
  setTotal(0);
}

void nsw::CalibAlg::setup(const std::string& db) {
  ERS_INFO("setup");
}

void nsw::CalibAlg::configure() {
  ERS_INFO("configure");
}

void nsw::CalibAlg::unconfigure() {
  ERS_INFO("unconfigure");
}

bool nsw::CalibAlg::next() {
  setCounter(counter() + 1);
  return counter() < total();
}

void nsw::CalibAlg::progressbar() {
  std::stringstream msg;
  msg << "Iteration " << counter()+1 << " / " << total();
  if (counter() == 0) {
    setStartTime();
    ERS_INFO(msg.str());
    return;
  }
  setElapsedSeconds();
  msg << ".";
  msg << std::fixed << std::setprecision(1);
  msg << " " << elapsedSeconds()   / 60.0 << "m elapsed.";
  msg << " " << remainingSeconds() / 60.0 << "m remaining.";
  ERS_INFO(msg.str());
}
