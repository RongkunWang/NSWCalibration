#ifndef NSWCALIBRATION_ROCPHASE40MHZCORE_H
#define NSWCALIBRATION_ROCPHASE40MHZCORE_H

#include <map>
#include <string>
#include <vector>

#include "NSWConfiguration/hw/ROC.h"

/**
 * \brief Calibration of the ROC 40 MHz core clock
 *
 * The 40 MHz clock of the ROC samples the incoming TTC stream. The clock phase can be shifted
 * using the ePllCore.ePllPhase40MHz setting in 128 steps. The clock is triplicated for SEU
 * protection. All three clocks must have the identical phase. The phase of the 160 MHz core
 * clock must be identical to the last five bits of the 40 MHz clock.
 *
 * The calibration loops over all phase values (128 iterations). After the configuration a pattern
 * of continous L1As is sent. The ROC status registers are read while the pattern is running and
 * saved into a file for each iteration.
 *
 * The VMMs are set to fake failure mode meaning that they do not send data to the sROCs. The TTC
 * FIFO will fill up if an L1A is interpreted correctly since the sROC will wait for a package from
 * the VMMs which it will not receive.
 *
 * The ALTI will also send an L0A for each L1A. The analysis has to differentiate between those two
 * signals to identify the good region. The read registers will be 255 if a signal is interpreted
 * as a soft reset.
 */
class RocPhase40MhzCore
{
  using ValueMap = std::map<std::string, std::vector<std::uint8_t>>;

  public:
  /**
   * \brief Construct a new ROC 40 Mhz core clock calibration object
   *
   * \param input Range of phases (if empty: all phase values)
   */
  explicit RocPhase40MhzCore(const std::vector<std::uint8_t>& input);

  /**
   * \brief Calibration specific ROC configuration
   *
   * Enable fake failure mode for all VMMs
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
