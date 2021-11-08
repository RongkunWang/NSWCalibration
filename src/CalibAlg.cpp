#include "NSWCalibration/CalibAlg.h"

#include <iostream>
#include <iomanip>

#include <ers/ers.h>

void nsw::CalibAlg::progressbar() {
  std::stringstream msg;
  msg << "Iteration " << m_counter+1 << " / " << m_total;
  if (m_counter == 0) {
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
