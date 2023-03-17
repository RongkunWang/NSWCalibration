#include "NSWCalibration/ThresholdCalibration.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <thread>
#include <tuple>

#include <fmt/core.h>
#include <fmt/chrono.h>
#include <nlohmann/json.hpp>

#include <RunControl/Common/OnlineServices.h>
#include <is/infoT.h>
#include <is/infodictionary.h>

#include "NSWCalibration/CalibAlg.h"
#include "NSWCalibration/Issues.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"

using json = nlohmann::json;

nsw::ThresholdCalibration::ThresholdCalibration(std::string calibName,
                                                const nsw::hw::DeviceManager& deviceManager) :
  nsw::CalibAlg(std::move(calibName), deviceManager)
{
}

nsw::ThresholdCalibration::~ThresholdCalibration()
{
  saveParameters();
}

void nsw::ThresholdCalibration::configure()
{
  ERS_LOG("Configuring...");
  const auto step = m_steps.at(counter());
  for (const auto& feb : getDeviceManager().getFebs()) {
    m_threadPool.addJob([&feb, step]() { configureFeb(feb, step); });
  }
  ERS_LOG("Configured FEBS, waiting for configure job completion...");
  m_threadPool.waitForCompletion();
}

void nsw::ThresholdCalibration::acquire()
{
  ERS_LOG("Acquiring lumiblock...");
  m_lbStart = getLumiBlock();
  ERS_LOG("m_lbStart acquired, sleeping for acquisitionTime...");
  std::this_thread::sleep_for(m_acquisitionTime);
}

void nsw::ThresholdCalibration::unconfigure()
{
  ERS_LOG("Unconfiguring...");
  const auto lbEnd = getLumiBlock();
  ERS_LOG("lb_End acquired, updating lumiblock map");
  updateLumiBlockMap(m_lbStart, lbEnd);
}

nsw::commands::Commands nsw::ThresholdCalibration::getAltiSequences() const
{
  return {{},
          {nsw::commands::actionSR,
           nsw::commands::actionOCR,
           nsw::commands::actionECR,
           nsw::commands::actionStartPG},
          {nsw::commands::actionStopPG}};
}

void nsw::ThresholdCalibration::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                                     const std::string& is_db_name)
{
  constexpr static std::string_view IS_KEY_ACQUISITION_TIME{"acquisitionTime"};
  const auto keyAcquisitionTime{fmt::format("{}.Calib.{}", is_db_name, IS_KEY_ACQUISITION_TIME)};

  if (is_dictionary.contains(keyAcquisitionTime)) {
    ISInfoInt acquisitionTime;
    is_dictionary.getValue(keyAcquisitionTime, acquisitionTime);
    if (acquisitionTime.getValue() < 0) {
      ers::error(
        calib::Issue(ERS_HERE,
                     fmt::format("Cannot set acquisition time to {}s ", acquisitionTime.getValue()),
                     "Value is negative"));
    } else {
      m_acquisitionTime = std::chrono::seconds{acquisitionTime.getValue()};
      ERS_INFO(fmt::format(
        "Found {} in IS. Will acquire data for {}", IS_KEY_ACQUISITION_TIME, m_acquisitionTime));
    }
  } else {
    ERS_INFO(fmt::format("Did not find {} in IS. Will acquire data for {}",
                         IS_KEY_ACQUISITION_TIME,
                         m_acquisitionTime));
  }

  // constexpr static std::string_view IS_KEY_STPS{"steps"};
  // const auto keySteps{fmt::format("{}.Calib.{}", is_db_name, IS_KEY_ACQUISITION_TIME)};

  // if (is_dictionary.contains(keySteps)) {
  //   ISInfoInt acquisitionTime;
  //   is_dictionary.getValue(keySteps, acquisitionTime);
  //   m_acquisitionTime = std::chrono::seconds{acquisitionTime.getValue()};
  //   ERS_INFO(fmt::format("Found {} in IS. Will acquire data for {}", IS_KEY_ACQUISITION_TIME,
  //   m_acquisitionTime));
  // } else {
  //   ERS_INFO(fmt::format("Did not find {} in IS. Will acquire data for {}",
  //   IS_KEY_ACQUISITION_TIME, m_acquisitionTime));
  // }
  setTotal(std::size(m_steps));
}

void nsw::ThresholdCalibration::saveParameters() const
{
  ERS_LOG("Saving parameters...");
  const auto output = json{
    {"LBs", getLbJson()},
    {"Steps", m_steps},
  };
  std::filesystem::create_directories(getOutputDir());
  std::ofstream outstream(getOutputPath("threshold_calibration.json"));
  outstream << std::setw(4) << output << std::endl;
}

std::uint32_t nsw::ThresholdCalibration::computeTreshold(const nsw::hw::VMM& vmm, int step)
{
  constexpr auto MIN{0};
  constexpr auto MAX{1023};
  const auto current = vmm.getConfig().getGlobalThreshold();  // is sanitized by VMMConfig
  const auto result = static_cast<int>(current) + step;
  if (result < MIN) {
    return MIN;
  }
  if (result > MAX) {
    return MAX;
  }
  ERS_LOG(fmt::format("Compute threshold result: {}", result));
  return static_cast<std::uint32_t>(result);
}

void nsw::ThresholdCalibration::configureFeb(const nsw::hw::FEB& feb, int step)
{
  ERS_LOG("Configuring single FEB...");
  for (const auto& vmm : feb.getVmms()) {
    const auto threshold = nsw::ThresholdCalibration::computeTreshold(vmm, step);
    nsw::ThresholdCalibration::setThreshold(vmm, threshold);
  }
}

void nsw::ThresholdCalibration::setThreshold(const nsw::hw::VMM& vmm, std::uint32_t threshold)
{
  ERS_LOG(fmt::format("Setting vmm threshold to {}", threshold));
  auto config = vmm.getConfig();
  config.setGlobalThreshold(threshold);
  vmm.writeConfiguration(config);
}
