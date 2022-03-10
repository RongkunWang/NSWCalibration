#ifndef PDOCALIB_H_
#define PDOCALIB_H_

#include <cstdint>
#include <string>
#include <vector>

#include <is/infodictionary.h>

#include "NSWConfiguration/FEBConfig.h"

#include "NSWCalibration/CalibAlg.h"

ERS_DECLARE_ISSUE(nsw, PDOCalibIssue, message, ((std::string)message))

namespace nsw {

  /*!
   * \brief A class dedicated to the PDO and TDO calibration exclusively from the TDAQ partition.
   *
   * All the configurable parameters are obtained by getCalibParamsIS function.
   */
  class PDOCalib : public CalibAlg
  {
    public:
    PDOCalib(std::string calibType, const hw::DeviceManager& deviceManager, std::string calibIsName, const ISInfoDictionary& calibIsDict);

    /*!
     * \brief Obtains front-end configuration
     *
     *  Obtains the path to the configuration JSON file using \ref read_pulsing_configs
     *  and sets the total iteration counter to 1 in the \ref CalibAlg class,
     *  also the \tt is dictionary is obtained from the ipc partition
     *
     *  \param db location of the configuration JSON file
     */
    void setup(const std::string& db) override;

    /*!
     * \brief Handles the PDO/TDO calibration cycle
     *
     *  Contains the main loop of the calibration procedure.
     *  In general following procedure is performed:
     *    - the pattern generator is stopped
     *    - calibration type (PDO/TDO) is obtained along with the calibration parameters from IS
     *    - groups of channels to be pulsed are identified and number of
     *      corresponding iterations is calculated
     *    - channel group loop starts for all VMMs on all front-ends defined
     *      in the configuration JSON file:
     *      > front-ends are configured to have ALL channels on ALL VMMs masked
     *      > parameter loop starts
     *        * front-ends are configured with ith group of channels to be unmasked and pulsed
     *          with appropriate pulser DAC or pulse delay
     *        * reset signals - Soft Reset, BCR, ECR are sent
     *        * pattern generator starts
     *        * sleep for a user defined time in [ms]
     *        * pattern generator stops
     *        * repeated for next parameter
     *      > repeat for next channel group
     *    - channel loop ends
     *    - user is notified that calibration ended
     */
    void configure() override;

    /*!
     * \brief Currently simply sleeps for the required acquisition window
     */
    void acquire() override;

    /*!
     * \brief Currently unused and is there to comply with CalibAlg function structure
     */
    void unconfigure() override;

    /*!
     * \brief Define ALTI sequences for PDO/TDO calibrations
     */
    [[nodiscard]]
    nsw::commands::Commands getAltiSequences() const override;

  public:

    /*!
     * \brief Resets m_loopCalibRegs to m_calibRegs and increments the current channel counter
     *
     *  The loop over calibration register values modifies
     *  m_loopCalibRegs by calling pop_back(), so when the vector is
     *  empty, we have processed all iteration points for this channel
     *  and are ready to move on to the next.
     */
    void resetCalibLoop();

    /*!
     * \brief Reads and sorts the front-end configuration
     *
     *  Reads the configuration JSON file from ``dbcon`` and assembles
     *  the vector of \ref FEBConfig class objects
     *
     *  \param dbcon path to configuration JSON file
     */
    std::vector<nsw::FEBConfig> read_pulsing_config(const std::string& dbcon);

    /*!
     * \brief Handler of starting the configuration sending threads
     *
     *  Creates \ref toggle_channel function threads (thread-per-front-end)
     *
     *  \param i_par pulser DAC or pulse delay value
     *  \param first_chan first channel defining a channel group to pulse (unmask)
     *  \param toggle flag to either mask all channels or pulse specific ones
     */
    void send_pulsing_configs(std::size_t i_par,
                              std::size_t first_chan,
                              bool toggle);

    /*!
     * \brief Creates and sends the pulsing configuration
     *
     *  This function two purposes, depending on the toggle value
     *    - if ``toggle = false``: the function masks all VMM channels in the front-end config
     *    - if ``toggle = true``: the function unmasks certain channels and sets the appropriate
     *      pulser DAC or pulse delay register values depending on pdo/tdo flags
     *  At the end, the function sends the resulting configuration to the front-end boards
     *
     *  \param feb front-end configuration
     *  \param first_chan corresponding to the first channel in a channel group to unmask/enable
     *  \param i_par pulser DAC or pulse delay value
     *  \param toggle flag to either mask (false) or enable (true) channels
     */
    void toggle_channels(nsw::FEBConfig feb,
                         std::size_t first_chan,
                         std::size_t i_par,
                         bool toggle);

    /*!
     * \brief passes and reads back data to/from swROD
     *
     * Uploads the concurrent calibration parameter into IS for the swROD
     * to record data in short future
     *
     * \param i_par calibration parameter value (DAC/delay)
     */
    void push_to_swrod(std::size_t i_par);

    /*!
     * \brief retrieves calibration parameters from IS
     *
     *  This function reads the IS entry ``setup.NSW.calibParams``
     *  that should contain the following information (comma
     *  separated):
     *   - channel group to be pulsed
     *   - calibration parameters (DACs/delays)
     *   - pulsing/recording time in milliseconds (separated by *)
     *
     *  The input should look like this :
     *   - ``is_write -p <part-name> -n Setup.NSW.calibType -t String -v PDOCalib -i 0``
     *   - ``is_write -p <part-name> -n Setup.NSW.calibParams -t String -v 8,100,200,300,*6000* -i
     * 0`` that would say that it is going to execute a PDO calibration run with groups of 8 channel
     * to be pulsed at at the same time with DAC values of 100,200,300 and recording time of 6000
     *  milliseconds
     *
     *  \param[in,out] reg_values vector of the register values to record data for
     *  \param[in,out?] group channel group
     */
    void get_calib_params_IS(std::vector<int>& reg_values, int& group);

    /*!
     * \brief Reads ROC digital register error status
     *
     *  The main purpose is the debug of the ROC state and readout quality
     */
    void check_roc();

    private:
    std::string m_isDbName;  //!< Name of the IS info DB
    const ISInfoDictionary& m_isInfoDict;  //!< Instance of the IS dictionary

    std::chrono::milliseconds m_trecord;  //!< Waiting time to record data in [ms]

    bool m_pdo_flag;  //!< Flag stating the type of calibration PDO/TDO

    std::vector<nsw::FEBConfig> m_feconfigs = {};  //!< vector of front-end configurations

    std::vector<int> m_calibRegs;      //!< Vector of calibration register values
    std::vector<int> m_loopCalibRegs;  //!< Vector of calibration register values inside the loop
    std::size_t m_currentChannel;      //!< Current channel in the calibration loop
    int m_currentCalibReg;             //!< Current calibration register in the calibration loop
    int m_numChPerGroup;               //!< Number of channels to pulse for each iteration

    // Timekeeping
    std::chrono::time_point<std::chrono::system_clock> m_chanIterStart;  //!< Start time for current channel iteration
    std::chrono::time_point<std::chrono::system_clock> m_calibStart;  //!< Start time for calibration
    std::chrono::time_point<std::chrono::system_clock> m_calibStop;   //!< Stop time for calibration

    std::vector<std::thread> m_conf_threads = {};  //!< Per front-end board configuration thread
    std::string m_calibCounterTck =
      "Monitoring.NSWCalibration.triggerCalibrationKey";  //!< monitoring IS entry for the
                                                          //!< triggerCalibrationKey that is
                                                          //!< essentially pulser DAC or delay
                                                          //!< register values
    std::string m_calibCounterTck_readback =
      "Monitoring.NSWCalibration.swrodCalibrationKey";  //!< Is entry sent to swROD, same as above
  };

}  // namespace nsw

#endif
