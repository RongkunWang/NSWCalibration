#include "NSWCalibration/CalibAlg.h"

#include <iostream>
#include <iomanip>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <ers/ers.h>

#include <is/infodynany.h>
#include <is/infodictionary.h>

#include <RunControl/Common/OnlineServices.h>

#include "NSWCalibrationDal/NSWCalibApplication.h"

nsw::CalibAlg::CalibAlg(std::string calibType, const hw::DeviceManager& deviceManager) :
  m_calibType(std::move(calibType)),
  m_deviceManager{deviceManager}
{
  auto& runCtlOnlServ = daq::rc::OnlineServices::instance();
  const auto& rcBase = runCtlOnlServ.getApplication();
  const auto* calibApp = rcBase.cast<nsw::dal::NSWCalibApplication>();

  m_out_path = calibApp->get_CalibOutput();
}

void nsw::CalibAlg::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                         const std::string& is_db_name)
{}

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

void nsw::CalibAlg::setCurrentRunParameters(const std::pair<std::uint32_t, std::time_t>& runParams)
{
  m_run_number = runParams.first;
  m_run_start  = runParams.second;

  // If we have a run number, use that as the unique tag, otherwise use the start of run time
  if (m_run_number != 0) {
    m_run_string = fmt::format("run{:08d}", m_run_number);
  } else {
    m_run_string = fmt::format("{:%Y_%m_%d_%Hh%Mm%Ss}",fmt::localtime(m_run_start));
  }
  ERS_LOG(fmt::format("Using run identifier {}", m_run_string));
}
