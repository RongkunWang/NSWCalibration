#include "NSWCalibration/RocPhaseCalibrationBase.h"

#include <algorithm>
#include <ios>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <array>
#include <exception>
#include <fstream>
#include <chrono>
#include <filesystem>

#include <fmt/core.h>

#include <ers/ers.h>

#include <RunControl/Common/OnlineServices.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/ConfigConverter.h"
#include "NSWConfiguration/Utility.h"
#include "NSWConfiguration/hw/ROC.h"

#include "NSWCalibration/CalibAlg.h"
#include "NSWCalibration/Commands.h"
#include "NSWCalibration/Utility.h"
#include "NSWCalibration/RocPhase40MhzCore.h"
#include "NSWCalibration/RocPhase160MhzCore.h"
#include "NSWCalibration/RocPhase160MhzVmm.h"

#include "NSWCalibrationDal/NSWCalibApplication.h"

using namespace std::chrono_literals;

template<typename Specialized>
RocPhaseCalibrationBase<Specialized>::RocPhaseCalibrationBase(
  std::string outputFilenameBase,
  const nsw::hw::DeviceManager& deviceManager,
  const std::vector<std::uint8_t>& values) :
  nsw::CalibAlg("RocPhase", deviceManager),
  m_initTime(nsw::calib::utils::strf_time()),
  m_outputPath(getOutputPath()),
  m_outputFilenameBase(std::move(outputFilenameBase)),
  m_specialized(values)
{
  std::filesystem::create_directories(std::filesystem::path(m_outputPath));
  setTotal(getNumberOfIterations());
}

template<typename Specialized>
void RocPhaseCalibrationBase<Specialized>::setup(const std::string& /*db*/)
{
  // look in header for documentation of executeFunc
  executeFunc([this](const nsw::hw::ROC& roc) {
    m_specialized.configure(roc);
    const auto filename = getFileName(roc);
    ERS_LOG("Opening file " << filename);
    writeFileHeader(filename);
  });
}

template<typename Specialized>
void RocPhaseCalibrationBase<Specialized>::configure()
{
  if (simulation()) {
    return;
  }

  executeFunc([this](const nsw::hw::ROC& roc) { setRegisters(roc); });
  std::this_thread::sleep_for(100ms);
}

template<typename Specialized>
void RocPhaseCalibrationBase<Specialized>::acquire()
{
  std::this_thread::sleep_for(1000ms);

  // check the result
  executeFunc([this](const nsw::hw::ROC& roc) {
    const auto result = checkStatusRegisters(roc);
    saveResult(result, getFileName(roc));
  });
}

template<typename Specialized>
nsw::commands::Commands RocPhaseCalibrationBase<Specialized>::getAltiSequences() const
{
  return {{},
          {nsw::commands::actionSR,
           nsw::commands::actionOCR,
           nsw::commands::actionECR,
           nsw::commands::actionStartPG},
          {nsw::commands::actionStopPG}};
}

template<typename Specialized>
[[nodiscard]] StatusRegisters RocPhaseCalibrationBase<Specialized>::checkStatusRegisters(
  const nsw::hw::ROC& roc)
{
  std::array<uint8_t, nsw::NUM_VMM_PER_MMFE8> vmmStatus{};
  std::array<uint8_t, nsw::NUM_VMM_PER_MMFE8> vmmParity{};
  std::array<uint8_t, nsw::roc::NUM_SROCS> srocStatus{};

  for (std::uint8_t vmmId = 0; vmmId < nsw::MAX_NUMBER_OF_VMM; vmmId++) {
    vmmStatus.at(vmmId) = roc.readVmmCaptureStatus(vmmId);
    vmmParity.at(vmmId) = roc.readVmmParityCounter(vmmId);
  }

  for (std::uint8_t srocId = 0; srocId < nsw::roc::NUM_SROCS; srocId++) {
    srocStatus.at(srocId) = roc.readSrocStatus(srocId);
  }

  return {vmmStatus, vmmParity, srocStatus};
}

template<typename Specialized>
void RocPhaseCalibrationBase<Specialized>::saveResult(const StatusRegisters& result,
                                                      const std::string& filename) const
{
  std::ofstream filestream;
  filestream.open(filename, std::ios::app);
  const auto& vmmStatuses = result.m_vmmStatus;
  const auto& parities = result.m_vmmParity;
  const auto& srocStatuses = result.m_srocStatus;
  const auto setting = m_specialized.getValueOfIteration(counter());
  // Check registers
  // (https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ROC/ROC_Reg_digital_analog_combined_annotated.xlsx&action=default)
  filestream << setting;
  for (const auto vmmStatus : vmmStatuses) {
    filestream << fmt::format(";{:#x}", vmmStatus);
  }
  for (const auto parity : parities) {
    filestream << fmt::format(";{:#x}", parity);
  }
  for (const auto srocStatus : srocStatuses) {
    filestream << fmt::format(";{:#x}", srocStatus);
  }
  filestream << '\n';
  filestream.close();
}

template<typename Specialized>
void RocPhaseCalibrationBase<Specialized>::writeFileHeader(const std::string& filename) const
{
  std::ofstream filestream;
  filestream.open(filename);
  filestream << "Value";
  for (std::size_t vmmId = 0; vmmId < nsw::MAX_NUMBER_OF_VMM; vmmId++) {
    filestream << fmt::format(";Capture Status VMM {}", vmmId);
  }
  for (std::size_t vmmId = 0; vmmId < nsw::MAX_NUMBER_OF_VMM; vmmId++) {
    filestream << fmt::format(";Parity Counter VMM {}", vmmId);
  }
  for (std::size_t srocId = 0; srocId < nsw::roc::NUM_SROCS; srocId++) {
    filestream << fmt::format(";Status sROC {}", srocId);
  }
  filestream << '\n';
  filestream.close();
}

template<typename Specialized>
std::string RocPhaseCalibrationBase<Specialized>::getOutputPath() const
{
  auto& runCtlOnlServ = daq::rc::OnlineServices::instance();
  const auto& rcBase = runCtlOnlServ.getApplication();
  const auto* calibApp = rcBase.cast<nsw::dal::NSWCalibApplication>();

  return calibApp->get_CalibOutput();
}

template<typename Specialized>
std::string RocPhaseCalibrationBase<Specialized>::getFileName(const nsw::hw::ROC& roc) const
{
  auto name = roc.getScaAddress();
  const auto replace_all = [] (std::string& inout, std::string_view what, std::string_view with)
  {
      std::size_t count{};
      for (std::string::size_type pos{};
          (pos = inout.find(what.data(), pos, what.length())) != std::string::npos;
          pos += with.length(), ++count) {
          inout.replace(pos, what.length(), with.data(), with.length());
      }
      return count;
  };
  replace_all(name, "/", "_");
  return fmt::format("{}/{}_{}_{}.csv", m_outputPath, m_outputFilenameBase, m_initTime, name);
}

template<typename Specialized>
std::size_t RocPhaseCalibrationBase<Specialized>::getNumberOfIterations() const
{
  return m_specialized.getInputVals().begin()->second.size();
}

template<typename Specialized>
std::map<std::string, unsigned int> RocPhaseCalibrationBase<Specialized>::createValueMap(
  const ValueMap& inputValues,
  const std::size_t iteration)
{
  std::map<std::string, unsigned int> valueMap{};
  std::transform(std::cbegin(inputValues),
                 std::cend(inputValues),
                 std::inserter(valueMap, std::begin(valueMap)),
                 [&iteration](const auto& pair) {
                   const auto& name = pair.first;
                   return std::make_pair(name, pair.second.at(iteration));
                 });
  return valueMap;
}

template<typename Specialized>
void RocPhaseCalibrationBase<Specialized>::setRegisters(const nsw::hw::ROC& roc) const
{
  const auto valueMap = createValueMap(m_specialized.getInputVals(), counter());
  roc.writeValues(valueMap);
}

// instantiate templates
template class RocPhaseCalibrationBase<RocPhase40MhzCore>;
template class RocPhaseCalibrationBase<RocPhase160MhzCore>;
template class RocPhaseCalibrationBase<RocPhase160MhzVmm>;
