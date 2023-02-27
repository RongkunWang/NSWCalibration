#include "NSWCalibration/RocPhase160MhzVmm.h"

#include <numeric>

#include <optional>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <fstream>

RocPhase160MhzVmm::RocPhase160MhzVmm(const std::vector<std::uint8_t>& input) :
  m_inputValues(createInputVals(input))
{}

[[nodiscard]] RocPhase160MhzVmm::ValueMap RocPhase160MhzVmm::createInputVals(
  const std::vector<std::uint8_t>& input)
{
  // use lambda function to fix ValueMap value, initialized in conditional statement
  const auto ePllPhase160MHzVals = [&input]() {
    if (input.empty()) {
      // 160MHz clock has 32 possible different values
      const int nEntries{32};

      // 160MHz has values 0 - 31
      const int clock160MHzStart{0};

      auto vec = std::vector<std::uint8_t>(nEntries);

      std::generate(vec.begin(), vec.end(), [i = clock160MHzStart]() mutable { return i++; });

      return vec;
    }
    return input;
  }();

  return {{"ePllVmm0.ePllPhase160MHz_0", ePllPhase160MHzVals},
          {"ePllVmm0.ePllPhase160MHz_1", ePllPhase160MHzVals},
          {"ePllVmm0.ePllPhase160MHz_2", ePllPhase160MHzVals},
          {"ePllVmm0.ePllPhase160MHz_3", ePllPhase160MHzVals},
          {"ePllVmm1.ePllPhase160MHz_0", ePllPhase160MHzVals},
          {"ePllVmm1.ePllPhase160MHz_1", ePllPhase160MHzVals},
          {"ePllVmm1.ePllPhase160MHz_2", ePllPhase160MHzVals},
          {"ePllVmm1.ePllPhase160MHz_3", ePllPhase160MHzVals}};
}

void RocPhase160MhzVmm::configure(const nsw::hw::ROC& roc) const
{
  if (m_calibJson.has_value()){
    std::ifstream f(m_calibJson.value());
    nlohmann::json new_core_phases = nlohmann::json::parse(f);
    for (const auto& [key,value] : new_core_phases.at(roc.getScaAddress()).items()){
      const auto before = roc.readValue(key);
      roc.writeValue(key, value);
      // ERS_INFO(fmt::format("Updated {} for board {}: {} -> {}", key, roc.getScaAddress(), before, roc.readValue(key)));
    }
  }
  roc.writeValue("bypassMode", 0);
}

int RocPhase160MhzVmm::getValueOfIteration(const std::size_t iteration) const
{
  return m_inputValues.at("ePllVmm0.ePllPhase160MHz_0").at(iteration);
}

RocPhase160MhzVmm::ValueMap RocPhase160MhzVmm::getInputVals() const
{
  return m_inputValues;
}

void RocPhase160MhzVmm::getJson(const ISInfoDictionary& is_dictionary, const std::string& is_db_name)
{
  const auto calib_param_is_name = fmt::format("{}.Calib.calibJson", is_db_name);
  if (is_dictionary.contains(calib_param_is_name)) {
    ISInfoDynAny calib_params_from_is;
    is_dictionary.getValue(calib_param_is_name, calib_params_from_is);
    const auto calibParams = calib_params_from_is.getAttributeValue<std::string>(0);
    ERS_INFO(fmt::format("Updating core phases with {} for VMM calib", calibParams));
    m_calibJson = std::move(calibParams);
  } else {
    ERS_INFO(fmt::format("No calibration JSON in IS: warning VMM phases depend on core phases"));
  }
}