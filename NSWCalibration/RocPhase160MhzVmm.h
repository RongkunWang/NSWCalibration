#ifndef NSWCALIBRATION_ROCPHASE160MHZVMM_H
#define NSWCALIBRATION_ROCPHASE160MHZVMM_H

#include <map>
#include <string>
#include <vector>

#include "NSWConfiguration/hw/ROC.h"

#include <is/infodynany.h>
#include <is/infodictionary.h>
#include <optional>

/**
 * \brief Calibration of the ROC 160 MHz VMM clock
 *
 * The 160 MHz VMM clock is responsible for sending data from the VMMs to the ROC.
 * The clock phase can be shifted using the ePllVmm0.ePllPhase160MHz and ePllVmm1.ePllPhase160MHz
 * settings in 32 steps, with four VMMs per each block.
 *
 * For the 160 MHz VMM calibration, the data stream is sampled using a fixed 160 MHz core clock.
 * Therefore, the calibrations of the two core clocks must be preformed first, using the results
 * for the VMM clock calibration.
 *
 * The calibration loops over all phase values (32 iterations). After the configuration a pattern
 * of continous L1As and test pulses is sent. The ROC status registers are read while the pattern
 * is running and saved into a file for each iteration.
 *
 * A good phase region for the 160 MHz VMM clock should be free of errors in the capture status
 * registers for each VMM. Offline analysis is then performed to find the optimum phase in the
 * middle of these regions, per VMM.
 */
class RocPhase160MhzVmm
{
  using ValueMap = std::map<std::string, std::vector<std::uint8_t>>;

  public:
  /**
   * \brief Construct a new ROC 160 Mhz VMM clock calibration object
   *
   * \param input Range of phases (if empty: all phase values)
   */
  explicit RocPhase160MhzVmm(const std::vector<std::uint8_t>& input);

  /**
   * \brief Calibration specific ROC configuration
   *
   * Not used for 160 MHz VMM calibration
   *
   * \param roc ROC to be configured
   */
  void configure(const nsw::hw::ROC& roc) const;

  /**
   * \brief Get the value of the phase for a given iteration
   *
   * Will be identical to the iteration if all values are scanned
   *
   * \param iteration Number of iteration
   * \return int Value of the 160 MHz VMM phase
   */
  [[nodiscard]] int getValueOfIteration(std::size_t iteration) const;

  /**
   * \brief Get the input phase values
   *
   * \return ValueMap Phase values for each iteration for configuration of the clocks
   */
  [[nodiscard]] ValueMap getInputVals() const;

  void getJson(const ISInfoDictionary& is_dictionary, const std::string& is_db_name);

  private:
  /**
   * \brief Create the input phase values (helper function)
   *
   * \param input Range of phases (if empty: scan all phases)
   * \return ValueMap Phase values for each iteration for configuration of the clocks
   */
  [[nodiscard]] static ValueMap createInputVals(const std::vector<std::uint8_t>& input);

  ValueMap m_inputValues{};
  std::optional<std::string> m_calibJson{};
};

#endif
