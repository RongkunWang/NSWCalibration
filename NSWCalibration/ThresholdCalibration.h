#ifndef NSWCALIBRATION_THRESHOLDCALIBRATIONBASE_H
#define NSWCALIBRATION_THRESHOLDCALIBRATIONBASE_H

#include <chrono>
#include <string>
#include <vector>
#include <array>
#include <thread>

#include <ipc/partition.h>
#include <ipc/threadpool.h>

#include "NSWConfiguration/hw/FEB.h"

#include "NSWCalibration/CalibAlg.h"


namespace nsw {

  /**
   * \brief TODO.
   */
  class ThresholdCalibration : public nsw::CalibAlg
  {
    public:
    /**
     * \brief Constructor
     *
     * \param calibName Name of the calibration
     * \param deviceManager Device Manager
     */
    ThresholdCalibration(std::string calibName, const nsw::hw::DeviceManager& deviceManager);

    ~ThresholdCalibration() override;
    ThresholdCalibration(const ThresholdCalibration&) = delete;
    ThresholdCalibration(ThresholdCalibration&&) = delete;
    ThresholdCalibration& operator=(ThresholdCalibration&&) = delete;
    ThresholdCalibration& operator=(const ThresholdCalibration&) = delete;

    /**
     * \brief Configure VMMs to use current threshold step
     */
    void configure() override;

    /**
     * \brief Wait for configured amount of time
     */
    void acquire() override;

    /**
     * \brief Save LBs for this iteration
     */
    void unconfigure() override;

    /**
     * \brief Should not be necessary
     */
    void setup(const std::string& /*db*/) override{};

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

    /**
     * \brief Tell orchestrator to increase LB between iterations
     *
     * \return Increase LB between iterations 
     */
    [[nodiscard]] bool increaseLbBetweenIterations() const override { return true; }

    /**
     * \brief Extract calibration parameters from IS
     *
     * Read steps from IS
     *
     * \param is_dictionary Reference to the ISInfoDictionary from the RC application
     * \param is_db_name Name of the IS server storing the parameters
     */
    void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                              const std::string& is_db_name) override;

    /**
     * \brief Save the lumi block map and steps into a JSON
     */
    void saveParameters() const;

    /**
     * \brief Compute the new threshold for a VMM
     *
     * \param vmm HWI of VMM
     * \param step Step to be taken
     * \return std::uint32_t New threshold
     */
    [[nodiscard]] static std::uint32_t computeTreshold(const nsw::hw::VMM& vmm, int step);

    /**
     * \brief Set thresholds of all VMMs of FEB
     *
     * \param vmm HWI of FEB
     * \param step Step to be taken
     */
    static void configureFeb(const nsw::hw::FEB& feb, int step);

    /**
     * \brief Write the threshold to a VMM
     *
     * \param vmm HWI of VMM
     * \param threhold Threshold to be written (sdt_dac)
     */
    static void setThreshold(const nsw::hw::VMM& vmm, std::uint32_t threshold);

    private:
    constexpr static std::chrono::seconds DEFAULT_ACQUISITION_TIME{60};
    constexpr static std::array
    DEFAULT_STEPS{0, 100, 10,110, 20,120, 30,130, 40,140, 50,150, 60,160, 70,170, 80,180, 90,190, 100,200};
    // DEFAULT_STEPS{-30, -10, 0, 5, 10, 15, 20, 25, 30, 40, 50, 75, 100, 150};
    constexpr static std::size_t NUM_CONCURRENT{10};
    std::vector<int> m_steps{std::cbegin(DEFAULT_STEPS), std::cend(DEFAULT_STEPS)};
    IPCThreadPool m_threadPool{NUM_CONCURRENT};
    std::chrono::seconds m_acquisitionTime{DEFAULT_ACQUISITION_TIME};
    int m_lbStart{};
  };

}  // namespace nsw

#endif
