#ifndef NSWCALIBRATION_ROCPHASE160MHZCORE_H
#define NSWCALIBRATION_ROCPHASE160MHZCORE_H

#include <map>
#include <string>
#include <vector>

#include "NSWConfiguration/hw/ROC.h"

/**
 * \brief Calibration of the ROC 160 MHz core clock
 *
 * The 160 MHz core clock of the ROC is responsible for decoding incoming data from the VMMs
 * and internal functionality of the sROCs. The clock phase can be shifted using the
 * ePllCore.ePllPhase160MHz setting in 32 steps. The clock is triplicated for SEU protection.
 * All three clocks must have the identical phase.
 *
 * Since the 40 and 160 MHz core clocks must be changed synchronously, the 160 MHz core calibration
 * loops over all phase values of the 40 MHz core clock again, in order to bypass online analysis.
 * After the configuration a pattern of continous L1As and test pulses is sent. The ROC status
 * registers are read while the pattern is running and saved into a file for each iteration.
 *
 * A good phase region for the 160 MHz clock should be free of errors in all status registers :
 * sROCs=0 since now should receive packages from the VMMs, capture status=1 for sampling the data
 * correctly, and parity counters=0 for no parity errors in the data. Note, there will be an
 * additional soft reset region from the misinterpretation of the test pulse.
 *
 * After the 40 and 60 MHz core calibrations are complete, offline analysis is used to combine the
 * results for the optimum phase for each. The good region on the 40 MHz core must overlap with
 * a good region on the 160 MHz clock. The middle of this overlap is then needed to subsequently
 * perform the 160 MHz VMM calibration.
 */
class RocPhase160MhzCore
{
  using ValueMap = std::map<std::string, std::vector<std::uint8_t>>;

  public:
  /**
   * \brief Construct a new ROC 160 Mhz core clock calibration object
   *
   * \param input Range of phases (if empty: all phase values)
   */
  explicit RocPhase160MhzCore(const std::vector<std::uint8_t>& input);

  /**
   * \brief Calibration specific ROC configuration
   *
   * Not used for 160 MHz core calibration
   *
   * \param roc ROC to be configured
   */
  static void configure(const nsw::hw::ROC& roc);

  /**
   * \brief Get the value of the phase for a given iteration
   *
   * Will be identical to the iteration if all values are scanned
   *
   * \param iteration Number of iteration
   * \return int Value of the 40 MHz clock phase
   */
  [[nodiscard]] int getValueOfIteration(std::size_t iteration) const;

  /**
   * \brief Get the input phase values
   *
   * \return ValueMap Phase values for each iteration for configuration of the clocks
   */
  [[nodiscard]] ValueMap getInputVals() const;

  private:
  /**
   * \brief Create the input phase values (helper function)
   *
   * \param input Range of phases (if empty: scan all phases)
   * \return ValueMap Phase values for each iteration for configuration of the clocks
   */
  [[nodiscard]] static ValueMap createInputVals(const std::vector<std::uint8_t>& input);

  ValueMap m_inputValues{};
};

#endif
