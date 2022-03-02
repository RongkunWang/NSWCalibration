#ifndef NSWCALIBRATION_ROCPHASECALIBRATIONBASE_H
#define NSWCALIBRATION_ROCPHASECALIBRATIONBASE_H

#include <string>
#include <vector>
#include <array>
#include <thread>

#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/hw/ROC.h"

#include "NSWCalibration/CalibAlg.h"

using ValueMap = std::map<std::string, std::vector<std::uint8_t>>;

struct StatusRegisters {
  std::array<uint8_t, nsw::MAX_NUMBER_OF_VMM> m_vmmStatus{};
  std::array<uint8_t, nsw::MAX_NUMBER_OF_VMM> m_vmmParity{};
  std::array<uint8_t, nsw::roc::NUM_SROCS> m_srocStatus{};
};

/**
 * \brief Calibration of three ROC clocks.
 *
 * The 40 MHz core clock, the 160 MHz core clock, and the 160 MHz VMM ROC clocks are calilbrated
 * for each NSW FEB : MMFE8, SFEB and PFEBs.
 *
 * Each calibration scans over the phases of the clock and records the ROC status registers :
 * VMM Capture Status, VMM Parity Counter, and sROC Status
 *
 * In each iteration send SR, OCR, ECR, and start pattern generator after configuration and
 * stop pattern generator after acquire.
 *
 * The two core clocks are scanned first and then results are analyzed offline. The resulting
 * optimum phase for both core clocks is then used in the final scan of the VMM clocks. Offline
 * analysis is then performed for the optimum phase of each VMM clock. The phase of the 40 MHz VMM
 * clock is configured to be the same as the 40 MHz core clock.
 *
 * \tparam Specialized Particular clock calibration class
 */
template<typename Specialized>
class RocPhaseCalibrationBase : public nsw::CalibAlg
{
  public:
  /**
   * \brief Constructor for a ROC base calibration object
   *
   * \param outputFilenameBase Prefix for output file
   * \param values Phases passed to the calibration (if empty: calibration will run over all
   * possible phases of a clock)
   */
  explicit RocPhaseCalibrationBase(std::string outputFilenameBase,
                                   const std::vector<std::uint8_t>& values = {});

  /**
   * \brief Configure ROC phase value for current iteration of phase loop
   */
  void configure() override;

  /**
   * \brief Check status register and save result for current iteration
   */
  void acquire() override;

  /**
   * \brief Initial setup of all ROCs
   *
   * 1. Read ROC configuration for each FEB from json or db
   * 2. Write full configuration for each ROC
   * 3. Write calibration specific ROC configuration (may not be used)
   * 4. Write file header of output file
   *
   * \param db Configuration resource (json or db)
   */
  void setup(const std::string& db) override;

  /**
   * \brief Get the list of commands to be executed by the AltiController
   *
   * Before: nothing
   * During: SR, OCR, ECR, start generator
   * After: stop generator
   *
   * \return nsw::commands::Commands ALTI commands
   */
  [[nodiscard]] nsw::commands::Commands getAltiSequences() const override;

  private:
  /**
   * \brief Create map of values for ROC configuration for a given iteration
   *
   * This map can be directly written to the ROC using roc.writeValues()
   *
   * \param inputValues ValueMap of phase values
   *
   * \param iteration Current iteration
   *
   * \return std::map<std::string, unsigned int> Values to be written to ROC
   */
  [[nodiscard]] static std::map<std::string, unsigned int> createValueMap(
    const ValueMap& inputValues,
    std::size_t iteration);

  /**
   * \brief Check ROC status registers after iteration of phase loop
   *
   * \param roc ROC to be calibrated
   *
   * \return StatusRegisters Arrays of values for each status register per ROC
   */
  [[nodiscard]] static StatusRegisters checkStatusRegisters(const nsw::hw::ROC& roc);

  /**
   * \brief Write values of status registers for each iteration per ROC to csv file
   *
   * \param result Values of status registers after iteration of phase loop
   *
   * \param filename Filename of output file
   */
  void saveResult(const StatusRegisters& result, const std::string& filename) const;

  /**
   * \brief Configure ROC phase value for current iteration of phase loop
   *
   * \param roc ROC to be calibrated
   */
  void setRegisters(const nsw::hw::ROC& roc) const;

  /**
   * \brief Write file header of output file
   *
   * \param filename Filename of output file
   */
  void writeFileHeader(const std::string& filename) const;

  /**
   * \brief Get the output path from the partition
   *
   * \param path directory
   */
  [[nodiscard]] std::string getOutputPath() const;

  /**
   * \brief Create output filename (<directory>/<prefix>_<time>_<FEB name>.csv)
   *
   * \param roc ROC to be calibrated
   *
   * \return std::string Filename of output file
   */
  [[nodiscard]] std::string getFileName(const nsw::hw::ROC& roc) const;

  /**
   * \brief Get total number of iterations in phase loop
   *
   * \return std::size_t Number of iterations in phase loop
   */
  [[nodiscard]] std::size_t getNumberOfIterations() const;

  /**
   * \brief Parallelizes calibration functions
   *
   * User can pass a function that takes a ROC as an argument. This function can
   * perform multiple operations, for example, checkStatusRegisters then saveResult.
   * This function will be called on all ROCs in parallel.
   *
   * \param func Input function to be performed and parallelized
   *
   * \tparam Func Invocable (ie lambda function) that takes ROC as parameter
   */
  template<typename Func>
  void executeFunc(const Func& func) const
  {
    std::vector<std::thread> threads;
    threads.reserve(m_rocs.size());
    for (const auto& roc : m_rocs) {
      threads.push_back(std::thread(func, std::ref(roc)));
    }
    for (auto& thread : threads) {
      thread.join();
    }
  }

  std::vector<nsw::hw::ROC> m_rocs{};
  std::string m_initTime{};
  std::string m_outputPath{};
  std::string m_outputFilenameBase{};
  Specialized m_specialized{};
};

#endif
