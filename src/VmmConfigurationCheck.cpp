#include "NSWCalibration/VmmConfigurationCheck.h"

#include <mutex>
#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <ers/ers.h>
#include <is/infoT.h>
#include <is/infodictionary.h>

nsw::VmmConfigurationCheck::VmmConfigurationCheck(const std::string& calibType,
                                                  const nsw::hw::DeviceManager& deviceManager) :
  nsw::CalibAlg(calibType, deviceManager)
{}

void nsw::VmmConfigurationCheck::setup(const std::string& /*db*/)
{
  setTotal(m_iterations);
  // look in header for documentation of executeFunc
  executeFunc([](const nsw::hw::FEB& feb) { feb.getRoc().writeConfiguration(); });
}

void nsw::VmmConfigurationCheck::configure()
{
  if (simulation()) {
    return;
  }

  executeFunc([](const nsw::hw::FEB& feb) {
    for (const auto& vmm : feb.getVmms()) {
      vmm.writeConfiguration();
    }
  });
}

void nsw::VmmConfigurationCheck::acquire()
{
  if (simulation()) {
    return;
  }

  executeFunc([this](const nsw::hw::FEB& feb) {
    std::map<std::string, bool> results{};
    for (const auto& vmm : feb.getVmms()) {
      results.try_emplace(fmt::format("{}/vmm{}", vmm.getScaAddress(), vmm.getVmmId()),
                          vmm.validateConfiguration());
    }
    std::lock_guard lock{m_mutex};
    for (const auto& [name, result] : results) {
      if (not m_stats.contains(name)) {
        m_stats.try_emplace(name, 0);
      }
      if (not result) {
        ++m_stats.at(name);
      }
    }
  });

  if (counter() == total() - 1) {
    dumpResult();
  }
}

void nsw::VmmConfigurationCheck::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                                      const std::string& is_db_name)
{
  constexpr static std::string_view IS_KEY{"numIterations"};
  const auto key{fmt::format("{}.Calib.{}", is_db_name, IS_KEY)};

  if (is_dictionary.contains(key)) {
    ISInfoUnsignedLong numIterations;
    is_dictionary.getValue(key, numIterations);
    m_iterations = numIterations.getValue();
    setTotal(m_iterations);
    ERS_INFO(fmt::format("Found {} in IS. Will run {} iterations", IS_KEY, m_iterations));
  } else {
    ERS_INFO(fmt::format("Did not find {} in IS. Will run {} iterations", IS_KEY, m_iterations));
  }
}

void nsw::VmmConfigurationCheck::dumpResult() const
{
  ERS_INFO(fmt::format("Total:{}\nResult:{}", m_iterations, m_stats));
}
