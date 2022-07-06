#ifndef PDOCALIB_H_
#define PDOCALIB_H_

#include <cstdint>
#include <string>
#include <vector>

#include "NSWConfiguration/hw/FEB.h"

#include "NSWCalibration/CalibAlg.h"

ERS_DECLARE_ISSUE(nsw, PDOCalibIssue, message, ((std::string)message))

ERS_DECLARE_ISSUE(nsw, PDOParameterIssue, message, ((std::string)message))

namespace nsw {

  /*!
   * \brief A class dedicated to the PDO and TDO calibration exclusively from the TDAQ partition.
   *
   * All the configurable parameters are obtained by getCalibParamsIS function.
   */
  class PDOCalib : public CalibAlg
  {
    public:

    struct RunParameters
    {
      std::chrono::milliseconds trecord{};
      std::vector<std::size_t> values{};
      std::size_t channels{};
    };

    PDOCalib(std::string calibType, const hw::DeviceManager& deviceManager);

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
     *  \param feb front-end board device
     *  \param first_chan corresponding to the first channel in a channel group to unmask/enable
     *  \param i_par pulser DAC or pulse delay value
     *  \param toggle flag to either mask (false) or enable (true) channels
     */
    void toggle_channels(const nsw::hw::FEB& feb,
                         std::size_t first_chan,
                         std::size_t i_par,
                         bool toggle);

    /*!
     * \copydoc CalibAlg::setCalibParamsFromIS
     *
     *  This function reads the IS entry that should contain the
     *  following information (comma separated):
     *   - channel group to be pulsed
     *   - calibration parameters (DACs/delays)
     *   - pulsing/recording time in milliseconds (surrounded by *)
     *
     *  The input should look like this :
     *   - ``is_write -p <part-name> -n NswParams.calibParams -t String -v 8,100,200,300,*6000* -i 0``
     *  indicating that it is going to execute a PDO calibration with
     *  groups of 8 channels to be pulsed at the same time with DAC
     *  values of 100,200,300 and recording time of 6000 milliseconds
     */
    void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary, const std::string& is_db_name) override;
    void setCalibKeyToIS(const ISInfoDictionary&) override;

    /*!
     * \brief Parse the IS string containing the calibration parameters
     *
     * \param calibParams is the string retrieved from IS containing the calibration parameters
     *
     * \throws nsw::PDOParameterIssue when the extraction of calib parameters fails
     */
    static RunParameters parseCalibParams(const std::string& calibParams);

    private:
    std::reference_wrapper<const std::vector<hw::FEB>> m_febs;  //!< vector of front-end hw interfaces

    std::chrono::milliseconds m_trecord;  //!< Waiting time to record data in [ms]

    bool m_pdo_flag{};  //!< Flag stating the type of calibration PDO/TDO

    std::vector<std::size_t> m_calibRegs{};      //!< Vector of calibration register values
    std::vector<std::size_t> m_loopCalibRegs{};  //!< Vector of calibration register values inside the loop
    std::vector<std::size_t> m_loopCalibChs{};  //!< Vector of calibration channels inside the loop
    std::size_t m_currentChannel{};              //!< Current channel in the calibration loop
    std::size_t m_currentCalibReg{};             //!< Current calibration register in the calibration loop
    std::size_t m_numChPerGroup{};               //!< Number of channels to pulse for each iteration
    std::size_t m_numChGroups{};                //!< Number of first-channels to iterate(also -1 is the the index)

    // Timekeeping
    std::chrono::time_point<std::chrono::system_clock> m_chanIterStart{};  //!< Start time for current channel iteration
    std::chrono::time_point<std::chrono::system_clock> m_calibStart{};  //!< Start time for calibration
    std::chrono::time_point<std::chrono::system_clock> m_calibStop{};   //!< Stop time for calibration

    std::vector<std::thread> m_conf_threads{};  //!< Per front-end board configuration thread

    public:
    static constexpr auto DEFAULT_TRECORD{std::chrono::milliseconds(8000)};
    static constexpr std::size_t DEFAULT_NUM_CH_PER_GROUP{8};
  };

}  // namespace nsw

#endif
