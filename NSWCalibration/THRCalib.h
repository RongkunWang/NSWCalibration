#ifndef NSWCALIBRATION_THRCALIB_H
#define NSWCALIBRATION_THRCALIB_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <type_traits>

#include <boost/property_tree/ptree.hpp>

#include "NSWConfiguration/hw/FEB.h"

#include "NSWCalibration/CalibAlg.h"
#include "NSWCalibration/CalibTypes.h"

#include "NSWCalibration/ScaCalibration.h"

ERS_DECLARE_ISSUE(nsw, THRCalibIssue, message, ((std::string)message))

ERS_DECLARE_ISSUE(nsw, THRParameterIssue, message, ((std::string)message))

namespace nsw {
  // class VmmTrimmerScaCalibration;
  // class VmmThresholdScaCalibration;
  // class VmmBaselineScaCalibration;

  /*!
   * \brief A class designed to run VMM threshold calibration from TDAQ partition
   */
  class THRCalib : public CalibAlg
  {
    public:

    struct RunParameters
    {
      std::size_t samples{};
      std::size_t factor{};
      std::string type{};
      bool debug{};
    };

    THRCalib(std::string calibType, const hw::DeviceManager& deviceManager);

    /*!
     * \brief Get main calibration parameters for the run
     *
     *   Get the main input parameters from the partition to determine the
     *   NSW side (A or C), the sector name. Also loads the user defined input parameters for
     *   the particular calibation procedure via `GetSetupFromIS` (baselines
     *   or thresholds, number of samples per channel etc...)
     *
     * \param db path to the configuration
     */
    void setup(const std::string& db) override;

    /*!
     * \brief Holds the main loop for the calibration
     *
     *  Threshold calibration, baseline reading, or pulser DAC calibration
     *  mainly started by per-front-end threads
     */
    void configure() override;

    /*!
     * \brief Merges recorded partial json configs into a single file
     */
    void merge_json();

    /*!
     * \copydoc CalibAlg::setCalibParamsFromIS
     *
     *  The input for a baselines calibration should look like this:
     *   - ``is_write -p <part-name> -n NswParams.calibParams -t String -v BLN,10,9,0 -i 0``
     *  indicating that it is going to execute a baseline calibration
     *  with 10 samples per channel, an RMS factor of 9, and debug
     *  mode false
     *
     *  The input for a trimmers calibration should look like this:
     *   - ``is_write -p <part-name> -n NswParams.calibParams -t String -v THR,10,9,0 -i 0``
     *  indicating that it is going to execute a trimmer calibration with
     *  with 10 samples per channel, an RMS factor of 9, and debug
     *  mode false
     */
    void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary, const std::string& is_db_name) override;

    /*!
     * \brief Updates an input config file with per FEB information stored in a separate JSON
     *
     *  The update JSON contains the VMM blocks and one `OpcNodeId`
     *  key, and the function searches the input ptree for a node
     *  matching the same `OpcNodeId` and inserts/updates the `vmmX`
     *  blocks
     *
     * \param[in,out] input the ptree that is to be updated 9should
     *                correspond to the usual config JSON file.
     * \param update the ptree that contains the updated FEB information

     * \throws [set exception type here] when the update ptree
     *         contains an `OpcNodeId` not matching any key in the input ptree
     */
    static void updatePtreeWithFeb(boost::property_tree::ptree& input, boost::property_tree::ptree update);

    /*!
     * \brief Parse the IS string containing the calibration parameters
     *
     * \param calibParams is the string retrieved from IS containing the calibration parameters
     *
     * \throws nsw::THRParameterIssue when the extraction of calib parameters fails
     */
    static RunParameters parseCalibParams(const std::string& calibParams);

    private:
    /*!
     * \brief Wrapper around common usage pattern to launch threads.
     *
     * \tparam Calibration must be an ScaCalibration
     */
    template<typename Calibration>
    void launch_feb_calibration() const
    {
      static_assert(std::is_base_of_v<nsw::ScaCalibration, Calibration>,
                    "Invalid calibration type, must specify a derivative of nsw::ScaCalibration!");
      std::vector<std::thread> threads;
      for (const auto& feb : m_febs.get()) {
        threads.emplace_back([this, feb] () {
          Calibration calibration(feb, m_output_path, m_n_samples, m_rms_factor, m_sector, m_wheel, m_debug);
          calibration.runCalibration();
        });
        ERS_DEBUG(2,
                  "launch_feb_calibration::Started thread, output_path="
                    << m_output_path << ", nsamples=" << m_n_samples << ", m_debug=" << m_debug);
      }

      ERS_DEBUG(2, "launch_feb_calibration::Config sending loop ended");

      for (auto& thrd : threads) {
        thrd.join();
      }
    }

    private:
    std::string m_configFile;  //!< Configuration source from the dbConnection xml attribute

    std::reference_wrapper<const std::vector<hw::FEB>> m_febs;  //!< vector of front-end hw interfaces

    bool m_debug{false};       //!< general debug flag

    std::size_t m_n_samples{10};  //!< number of samples per channel can be modified from IS
    std::size_t m_rms_factor{9};  //!< RMS factor for threshold calibration can be modified from IS

    std::string m_run_type;        //!< run type obtained from IS
    std::string m_output_path;     //!< output directory for calibration data
    std::string m_app_name;

    // TODO FIXME obtain from GeoID
    std::size_t m_sector;  //!< sector (1 to 16)
    int m_wheel;   //!< side (A or C)
    std::size_t m_run_nr;  //!< run number from partition
  };
}  // namespace nsw
#endif
