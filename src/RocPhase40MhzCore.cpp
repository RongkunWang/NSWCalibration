#include "NSWCalibration/RocPhase40MhzCore.h"

#include <numeric>

RocPhase40MhzCore::RocPhase40MhzCore(const std::vector<std::uint8_t>& input) :
  m_inputValues(createInputVals(input))
{}

[[nodiscard]] RocPhase40MhzCore::ValueMap RocPhase40MhzCore::createInputVals(
  const std::vector<std::uint8_t>& input)
{
  // use lambda function to fix ValueMap value, initialized in conditional statement
  const auto ePllPhase40MHzVals = [&input]() {
    if (input.empty()) {
      // 40MHz clock has 128 possible different values
      const int nEntries{128};

      // 40MHz has values 0 - 127
      const int clock40MHzStart{0};

      auto vec = std::vector<std::uint8_t>(nEntries);

      std::generate(vec.begin(), vec.end(), [i = clock40MHzStart]() mutable { return i++; });

      return vec;
    }
    return input;
  }();

  const auto ePllPhase160MHzVals = [&ePllPhase40MHzVals]() {
    constexpr auto max160MhzPhase = 32;
    auto vec = std::vector<std::uint8_t>(ePllPhase40MHzVals.size());

    std::transform(ePllPhase40MHzVals.begin(),
                   ePllPhase40MHzVals.end(),
                   std::begin(vec),
                   [max160MhzPhase](const auto val) { return val % max160MhzPhase; });

    return vec;
  }();

  return {{"ePllCore.ePllPhase160MHz_0", ePllPhase160MHzVals},
          {"ePllCore.ePllPhase160MHz_1", ePllPhase160MHzVals},
          {"ePllCore.ePllPhase160MHz_2", ePllPhase160MHzVals},
          {"ePllCore.ePllPhase40MHz_0", ePllPhase40MHzVals},
          {"ePllCore.ePllPhase40MHz_1", ePllPhase40MHzVals},
          {"ePllCore.ePllPhase40MHz_2", ePllPhase40MHzVals}};
}

void RocPhase40MhzCore::configure(const nsw::hw::ROC& roc)
{
  constexpr auto disable = 0xFF;
  roc.writeRegister("reg019fakeVmmFailure", disable);
  roc.writeValue("bypassMode", 0);
}

int RocPhase40MhzCore::getValueOfIteration(const std::size_t iteration) const
{
  return m_inputValues.at("ePllCore.ePllPhase40MHz_0").at(iteration);
}

RocPhase40MhzCore::ValueMap RocPhase40MhzCore::getInputVals() const
{
  return m_inputValues;
}
