#ifndef NSWCALIBRATION_VMMCONFIGURATIONCHECK_H
#define NSWCALIBRATION_VMMCONFIGURATIONCHECK_H

#include <string>
#include <vector>
#include <thread>

#include "NSWCalibration/CalibAlg.h"

namespace nsw {

  /**
   * \brief Check VMM configuration
   *
   * Configure the VMMs and read the configuration back. Count how often they do not match per VMM.
   */
  class VmmConfigurationCheck : public nsw::CalibAlg
  {
    public:
    /**
     * \brief Constructor
     *
     * \param calibType Calibration type
     * \param deviceManager Device Manager
     */
    explicit VmmConfigurationCheck(const std::string& calibType,
                                   const nsw::hw::DeviceManager& deviceManager);

    /**
     * \brief Configure VMM
     */
    void configure() override;

    /**
     * \brief Check configuration of VMM
     */
    void acquire() override;

    /**
     * \brief Configure ROCs
     */
    void setup(const std::string& /*db*/) override;

    /**
     * \brief Implements a method to extract calibration parameters from IS
     *
     * Read the number of iterations from IS
     *
     * \param is_dictionary Reference to the ISInfoDictionary from the RC application
     * \param is_db_name Name of the IS server storing the parameters
     */
    void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                              const std::string& is_db_name) override;

    private:
    /**
     * \brief Dump the result to the log file
     */
    void dumpResult() const;

    /**
     * \brief Parallelizes calibration functions
     *
     * User can pass a function that takes a FEB as an argument.
     * This function will be called on all FEBs in parallel.
     *
     * \param func Input function to be performed and parallelized
     */
    void executeFunc(const std::regular_invocable<const nsw::hw::FEB&> auto& func) const
    {
      std::vector<std::future<void>> threads{};
      const auto& deviceManager = getDeviceManager();
      const auto& febs = deviceManager.getFebs();
      threads.reserve(std::size(febs));
      for (const auto& feb : febs) {
        threads.push_back(std::async(std::launch::async, func, std::ref(feb)));
      }
      for (auto& thread : threads) {
        thread.get();
      }
    }

    constexpr static std::size_t NUM_ITERATIONS_DEFAULT{10};
    std::map<std::string, int> m_stats{};
    std::size_t m_iterations{NUM_ITERATIONS_DEFAULT};
    mutable std::mutex m_mutex;
  };
}  // namespace nsw

#endif
