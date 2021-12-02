#include "NSWCalibration/RocPhase160MhzVmm.h"

#include <numeric>

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

void RocPhase160MhzVmm::configure([[maybe_unused]] const nsw::hw::ROC& roc) {}

int RocPhase160MhzVmm::getValueOfIteration(const std::size_t iteration) const
{
  return m_inputValues.at("ePllVmm0.ePllPhase160MHz_0").at(iteration);
}

RocPhase160MhzVmm::ValueMap RocPhase160MhzVmm::getInputVals() const
{
  return m_inputValues;
}