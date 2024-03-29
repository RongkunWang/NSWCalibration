#ifndef NSWCALIBRATION_SCACALIBRATION_H
#define NSWCALIBRATION_SCACALIBRATION_H

#include <functional>
#include <string>
#include <cstdint>

#include <fmt/core.h>

#include "NSWCalibration/CalibTypes.h"

#include "NSWConfiguration/hw/FEB.h"

#include <ers/Issue.h>

ERS_DECLARE_ISSUE(nsw, ScaFebCalibrationIssue,
                  fmt::format("{}: {}", feName, message),
                  ((std::string)feName)
                  ((std::string)message))

ERS_DECLARE_ISSUE(nsw, ScaVmmCalibrationIssue,
                  fmt::format("{} VMM{}: {}", feName, vmmId, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::string)message))

ERS_DECLARE_ISSUE(nsw, ScaCalibrationIssue,
                  fmt::format("{} VMM{}: {}", feName, vmmId, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::string)message))


ERS_DECLARE_ISSUE(nsw, ScaMaxRetries,
                  fmt::format("{} VMM{}: Maximum retries ({}) reached: {}", feName, vmmId, attempts, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::size_t)attempts)
                  ((std::string)message))

ERS_DECLARE_ISSUE(nsw, ScaVmmSamplingIssue,
                  fmt::format("{} VMM{}: {} Retrying after slight delay.", feName, vmmId, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::string)message))


namespace nsw {

  /*!
   * \brief Class controlling the SCA based VMM calibration for a single FEB
   */
  class ScaCalibration
  {
  public:
    ScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                   std::string outpath,
                   std::size_t nSamples,
                   std::size_t rmsFactor,
                   std::size_t sector,
                   int wheel,
                   bool debug);

    virtual ~ScaCalibration() = default;

    // Pure virtual, must be implemented in the specific calibration defining all steps
    virtual void runCalibration() = 0;

  private:
    // private data used internally

  protected:
    // private data used internally and by inherited classes
    std::reference_wrapper<const hw::FEB> m_feb;
    std::string m_feName;
    std::string m_boardName;

    bool m_isStgc;
    bool m_debug;

    std::size_t m_nVmms;
    std::size_t m_firstVmm;
    std::size_t m_quarterOfFebChannels;

    // clang-format off
    std::string m_outPath;    //!< output directory for calibration data

    std::size_t m_nSamples;   //!< Number of samples per channel can be modified from IS (default is 10)
    std::size_t m_rmsFactor;  //!< RMS factor for threshold calibration can be modified from IS (default is 9)

    int m_wheel;      //!< side (A or C)
    std::size_t m_sector;     //!< sector (1 to 16)
    // clang-format on

  protected:
    /*!
     * \brief Read the SCA ADC outputs for a given VMM chip.
     *
     * This routine will try for a maximum nsw::MAX_ATTEMPTS to read
     * readVmmPdoConsecutiveSamples.
     *
     * If the read succeeds, the size of the results vector is compared
     * against expectation (m_nSamples * samplingFactor)
     *
     * It is expected that prior to calling this function, the VMM has
     * been configured to output the desired information
     *
     * \param config Modified config to be used
     * \param vmmId Index of the VMM on this front-end being sampled
     * \param factor Bump factor of number of samples per point to
     *        acquire (default is 1).
     *
     * \returns nsw::calib::VMMSampleVector containing the resutls of the sampling.
     *          This container may be empty.
     */
    nsw::calib::VMMSampleVector getVmmPdoSamples(const VMMConfig& config, std::size_t vmmId, std::size_t samplingFactor = 1);

    /*!
     * \brief Configures the SCA and VMM to read the VMM channel monitor DAC
     *
     * Sets the monitor output to nsw::vmm::ChannelMonitor
     * Sets the channel monitor mode to nsw::vmm::ChannelAnalogOutput
     *
     * \param vmmId Index of the VMM on this front-end being sampled
     * \param channelId Index of the channel on this VMM being sampled
     * \param factor Bump factor of number of samples per point to
     *        acquire (default is 1).
     */
    nsw::calib::VMMSampleVector sampleVmmChMonDac(std::size_t vmmId,
                                                  std::size_t channelId,
                                                  std::size_t factor = 1);

    /*!
     * \brief Configures the SCA and VMM to read the VMM channel
     *        threshold DAC at a specified global threshold and channel
     *        trim DAC seating.
     *
     * Sets the channel monitor output to nsw::vmm::ChannelMonitor
     * Sets the channel monitor mode to nsw::vmm::ChannelTrimmedThreshold
     * Sets the channel trimmer to trimDacVal
     * Sets the global threshold to thrDacVal
     *
     * \param vmmId Index of the VMM on this front-end being sampled
     * \param channelId Index of the channel on this VMM being sampled
     * \param thrDacVal Value to write to the global threshold DAC
     * \param trimDacVal Value to write to the channel trim DAC
     * \param factor Bump factor of number of samples per point to
     *        acquire (default is 1).
     */
    nsw::calib::VMMSampleVector sampleVmmChTrimDac(std::size_t vmmId,
                                                   std::size_t channelId,
                                                   std::size_t thrDacVal,
                                                   std::size_t trimDacVal,
                                                   std::size_t factor = 1);


    /*!
     * \brief Configures the SCA and the VMM to read the channel
     *        threshold DAC for the configured settings.
     *
     * Sets the channel monitor output to nsw::vmm::ChannelMonitor
     * Sets the channel monitor mode to nsw::vmm::ChannelTrimmedThreshold
     *
     * \param vmmId FEB VMM index
     * \param channelId VMM channel index
     * \param factor Bump factor of number of samples per point to
     *        acquire (default is 1).
     */
    nsw::calib::VMMSampleVector sampleVmmChThreshold(std::size_t vmmId,
                                                     std::size_t channelId,
                                                     std::size_t factor = 1);
    /*!
     * \brief Configures the SCA and VMM to read the VMM global threshold DAC
     *
     * Sets the monitor output of nsw::vmm::ThresholdDAC to nsw::vmm::CommonMonitor
     * Sets the global threshold to dacVal
     *
     * \param vmmId Index of the VMM on this front-end being sampled
     * \param dacValue Value to write to the global threshold DAC
     * \param factor Bump factor of number of samples per point to
     *        acquire (default is 1).
     */
    nsw::calib::VMMSampleVector sampleVmmThDac(std::size_t vmmId,
                                               std::size_t dacValue,
                                               std::size_t factor = 1);

    /*!
     * \brief Configures the SCA and VMM to read the VMM TestPulse DAC
     *
     * Sets the monitor output of nsw::vmm::TestPulseDAC to nsw::vmm::CommonMonitor
     * Sets the test pulse DAC to dacVal
     *
     * \param vmmId Index of the VMM on this front-end being sampled
     * \param dacValue Value to write to the TestPulse DAC
     * \param factor Bump factor of number of samples per point to
     *        acquire (default is 1).
     */
    nsw::calib::VMMSampleVector sampleVmmTpDac(std::size_t vmmId,
                                               std::size_t dacValue,
                                               std::size_t factor = 1);

  public:
    /*!
     * \brief Checks if the selected channel is connected to the micromegas
     * readout strip.
     *
     * \ingroup thresholds
     *
     *  The mapping of the unconnected channels can be found here:
     *   https://twiki.cern.ch/twiki/bin/viewauth/Atlas/NSWParameterBook
     *
     * N.B This function currently only works for MMFE8 FEBs
     *
     * \param feName front-end board name (e.g. MMFE8_L1P1_IPL)
     * \param vmmId FEB VMM index
     * \param channelId VMM channel index
     */
    bool checkIfUnconnected(const std::string& feName,
                            std::size_t vmmId,
                            std::size_t channelId) const;

    /*!
     * \brief Get the correct number of VMMs, starting VMM index, and
     *        quarter number of VMM channels depending on the board type
     *
     * \returns a \ref FEBVMMConstants containing the info for the FEB
     */
    nsw::calib::FEBVMMConstants getBoardVmmConstants();

    /*!
     * \brief Template to write out a tab-delimited line
     *
     * \tparm T first argument in the parameter pack
     * \tparm Args... all remaining arguments
     */
    template<typename T, typename... Args>
    static void writeTabDelimitedLine(std::ostream& stream, const T& first, const Args&... args)
    {
      stream << first;
      (..., (stream << '\t' << args)) << '\n';
    }

  };
}  // namespace nsw

#endif
