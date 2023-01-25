#include "NSWCalibration/CalibAlg.h"

#include <iostream>
#include <iomanip>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <ers/ers.h>

#include <is/infodynany.h>
#include <is/infodictionary.h>

#include <RunControl/Common/OnlineServices.h>
#include <TTCInfo/LumiBlockNamed.h>

#include "NSWCalibration/Issues.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"

nsw::CalibAlg::CalibAlg(std::string calibType, const hw::DeviceManager& deviceManager) :
  m_calibType(std::move(calibType)),
  m_deviceManager{deviceManager},
  m_ipcPartition{[]() {
    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    return rcSvc.getIPCPartition();
  }()},
  m_isDict{m_ipcPartition}
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

json nsw::CalibAlg::getLbJson() const
{
  std::map<int, std::map<std::string, int>> lbsTransformed{};
  std::ranges::transform(m_lbs,
                         std::inserter(lbsTransformed, std::end(lbsTransformed)),
                         [](const auto& pair) -> decltype(lbsTransformed)::value_type {
                           return {pair.first,
                                   std::map<std::string, int>{{"start", pair.second.first},
                                                              {"end", pair.second.second}}};
                         });
  return json{lbsTransformed};
}

int nsw::CalibAlg::getLumiBlock() const
{
  const auto getLb = [this](const std::string& key) -> std::optional<int> {
    if (m_isDict.contains(key)) {
      LumiBlockNamed lumipars(m_ipcPartition, key);
      m_isDict.getValue(key, lumipars);
      if (lumipars.LumiBlockNumber > std::numeric_limits<int>::max()) {
        ers::fatal(calib::Issue(ERS_HERE, "Cannot read LB", "LB is larger than max size of int"));
        return {};
      }
      return static_cast<int>(lumipars.LumiBlockNumber);
    }
    return {};
  };
  const auto key1 = std::string{"RunParams.LumiBlock"};
  const auto val1 = getLb(key1);
  if (val1) {
    return *val1;
  }
  const auto key2 = std::string{"RunParams.LuminosityInfo"};
  const auto val2 = getLb(key2);
  if (val2) {
    return *val2;
  }
  ers::fatal(calib::Issue(ERS_HERE, "Cannot read LB", "Value does not exist in IS"));
  return 0;
}

void nsw::CalibAlg::updateLumiBlockMap(const int lumiBlockStart, const int lumiBlockEnd)
{
  m_lbs.try_emplace(counter(), lumiBlockStart, lumiBlockEnd);
}
