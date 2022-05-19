#ifndef NSWCALIBRATION_VMMTRIMMERSCACALIBRATION_H
#define NSWCALIBRATION_VMMTRIMMERSCACALIBRATION_H

#include <unordered_map>
#include <fstream>

#include <boost/property_tree/ptree.hpp>

#include <fmt/core.h>

#include "NSWCalibration/ScaCalibration.h"

#include "NSWCalibration/CalibTypes.h"

#include <ers/Issue.h>

ERS_DECLARE_ISSUE(nsw, VmmTrimmerScaCalibrationIssue, message, ((std::string)message))
ERS_DECLARE_ISSUE(nsw,
                  VmmTrimmerFmtIssue,
                  fmt::format("{} VMM{}: {}", feName, vmmId, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::string)message))

ERS_DECLARE_ISSUE(nsw,
                  VmmTrimmerBadDacSlope,
                  fmt::format("{} VMM{}: Bad slope: {}", feName, vmmId, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::string)message))

ERS_DECLARE_ISSUE(nsw,
                  VmmTrimmerBadDacValue,
                  fmt::format("{} VMM{}: Bad DAC value: {}", feName, vmmId, message),
                  ((std::string)feName)
                  ((std::size_t)vmmId)
                  ((std::string)message))

ERS_DECLARE_ISSUE(nsw,
                  VmmTrimmerBadGlobalThreshold,
                  message,
                  ((std::string)message))

namespace nsw {
  /*!
   * \brief Object storing calibration data unique to a VMM
   *
   * If there were one instance of the calibration class per FEB+VMM,
   * this information would be member data
   */
  struct VmmTrimmerData {
    // Per-VMM channel objects
    nsw::calib::FebChannelMap<std::size_t> baselineMed;     //!< Per-channel baseline median
    nsw::calib::FebChannelMap<float> baselineRms;     //!< Per-channel baseline rms
    nsw::calib::FebChannelMap<int> maxEffThresh;    //<! Per-channel effective threshold ADC counts at trimmer maximum position
    nsw::calib::FebChannelMap<int> minEffThresh;    //<! Per-channel effective threshold ADC counts at trimmer minimum position
    nsw::calib::FebChannelMap<int> midEffThresh;    //<! Per-channel effective threshold ADC counts at trimmer middle position
    nsw::calib::FebChannelMap<float> effThreshSlope;  //<! Per-channel trimmer DAC slope at the linear region
    nsw::calib::FebChannelMap<std::size_t> trimmerMax;//<! Per-channel maximum trimmer DAC operational register values

    nsw::calib::FebChannelMap<float> eff_thr_w_best_trim;
    nsw::calib::FebChannelMap<std::size_t> best_channel_trim;
    nsw::calib::FebChannelMap<float> channel_trimmed_thr;
    nsw::calib::FebChannelMap<std::size_t> dac_to_add;
  };

  /*!
   * \brief Object storing calibration data unique to a FEB
   *
   * Arguably, everything within this should be members of the
   * calibration class, since there is one instance per FEB
   */
  struct FebTrimmerData {
    // Per-VMM objects
    nsw::calib::FebVmmMap<std::size_t> baselineMed;   //<! Per VMM baseline median
    nsw::calib::FebVmmMap<float> baselineRms;   //<! Per VMM baseline RMS
    nsw::calib::FebVmmMap<std::size_t> thDacValues;   //<! Per VMM final threshold DAC value (initial value part of thDacConstants)
    nsw::calib::FebVmmMap<int> midTrimMed;    //<! Per VMM global threshold ADC counts median at trimmer middle position
    nsw::calib::FebVmmMap<int> midEffThresh;  //<! Per VMM global effective thresholds ADC counts at trimmer middle position
    nsw::calib::FebVmmMap<std::size_t> baselinesOverThresh;  //<! Per VMM the number of channels with baselines above threshold

    nsw::calib::FebVmmMap<nsw::calib::GlobalThrConstants> thDacConstants;          //<!
    nsw::calib::FebVmmMap<nsw::calib::VMMChannelSummary> channelInfo;              //<!
    nsw::calib::FebVmmMap<nsw::calib::VmmChannelArray<std::size_t>> channelMasks;  //<!
    nsw::calib::FebVmmMap<std::size_t> goodChannels;                               //<!
    nsw::calib::FebVmmMap<std::size_t> totalChannels;                              //<!
  };

  /*!
   * \brief Class controlling the VMM trim calibration for a single FEB
   *
   * Inherits from the ScaCalibration (or template instantiates an instance of?)
   *
   * The trimmer calibration consists of several steps. In the first
   * step, the function VmmTrimmerScaCalibration::readThresholds is
   * called to measure the per-channel thresholds in the default
   * setting. No member variables are updated, and the result is an
   * output text file.
   *
   * The second step is the trimming. In this phase, the first step is
   * to call VmmTrimmerScaCalibration::getUnconnectedChannels to
   * determine which channels are not connected/reachable.
   *
   * In the case that all channels are disconnected, or the sampling
   * returned an empty vector, the output for that VMM would consist
   * of default values.
   *
   *
   */
  class VmmTrimmerScaCalibration : public ScaCalibration
  {
  public:
    // public functions exposed to callers
    // Construct a calibration for all VMMs on this FEB
    VmmTrimmerScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                             std::string outpath,
                             std::size_t nSamples,
                             std::size_t rmsFactor,
                             std::size_t sector,
                             int wheel,
                             bool debug);

    void runCalibration() final;

  private:
    /*!
     * \brief Standalone threshold reading function for debug/diagnostics
     *
     * Records threshold data to disk for later calibration performance assesment
     * - Performs a loop over all the VMMs
     *   - For each VMM loops over the channels
     *     - Calls \c chThreshold, and if this fails, the channel is skipped
     *     - If the read is successful, calculates the sum, mean, and largest deviation
     *       from the mean
     *     - Issues warnings if the sample mean is below \c nsw::ref::THR_DEAD_CUTOFF
     *     - Sorts the results to calculate the
     *       -  max, min, and deviation between max and min
     *       - If the deviation between max and min is larger than
     *         \c nsw::ref::THR_SAMPLE_CUTOFF, and the largest deviation from the mean
     *         is greater than \c m_n_samples * \c nsw::ref::BASELINE_SAMP_FACTOR / 4,
     *         issues a warning and increases a counter of strongly deviating thresholds
     *     - writes to a file the following information:
     *       wheel sector name vmmID channelID mean max_deviation min_deviation
     *   - At the end of the routine, if the total number of channels with strongly
     *     deviating thresholds is larger than one quarter of all FEB channels, a warning
     *     is issued
     */
    void readThresholds();

    /*!
     * \brief Performs a full cycle of the threshold calibration
     *
     *  The main logic in general is the following:
     *    for each VMM on this front-end
     *    - read baseline of each channel
     *    - read baseline samples of all channels combined
     *    - from RMS of the combined sample vector calculate defired global threshold with desired
     *      effective threshold over VMM
     *    - check if the substantial nr of channels at the trimmer middle position have
     *      baseline below the global threshold
     *    - check linearity and usable range of the trimmer DACs
     *    - calculate the best suitable trimmer DAC to match desired effective threshold over VMM
     *    - write data and partial config (containing global and trimmer DACs and
     *      masked channels) to the disk
     */
    void scaCalib();

    // Functions that operate on a single VMM only, could be put into
    // a nested class for further encapsulation

    /*!
     * \brief Executes the VMM calibration routine
     *
     * First determines the number of disconnected channels.
     * - If the number of disconnected channels is equal to \c
     *   nsw::vmm::NUM_CH_PER_VMM, writes default trim and mask values
     *   via \c set_ch_trim_and_mask.
     *
     * - Otherwise, calculates the median of the baseline samples, the
     *   mean and  median of the  bad baseline samples, and  masks the
     *   hot  and dead  channels, via  \c mask_hot_dead_channels,  and
     *   prints warnings depending on the number of bad channels.
     *
     * - Prunes the samples to keep only those within the median +/- 30mV
     * - From these pruned results, calculates the sum, mean, stdev, and median
     * - Uses this to compute a guess to the global threshold DAC value
     *
     * \param vmmId FEB index of the VMM on this front-end to calibrate.
     */
    void calibrateVmm(std::size_t vmmId);

    /*!
     * \brief Determine the number of unconnected channels, and the number of
     *        sampling outliers when reading the baselines.
     *
     * \param vmmId FEB VMM index
     *
     * \returns nsw::calib::VMMDisconnectedChannelInfo an object containing:
     *          - a vector containing the median baseline for each channel
     *          - a vector containing the RMS of the baseline samples for each channel
     *          - a vector containing the baseline samples for all channels
     *          - the total number of disconnected channels for this VMM
     *          - a vector containing the number of baseline samples that were far
     *            outliers for each channel.
     */
    nsw::calib::VMMDisconnectedChannelInfo getUnconnectedChannels(std::size_t vmmId);

    /*!
     * \brief Reads the baseline of the VMM
     *
     *  Reads the baseline of one VMM channel a defined number of times (n_samples x 10)
     *  and fills the results in a per-channel map of floats for later calculations
     *  and records the number of bad samples for each channel.
     *
     * This function is called by \c getUnconnectedChannels for all
     * connected channels
     *
     * This function sets for the specified channel:
     *   - \c m_vmmTrimmerData.baselineMed
     *   - \c m_vmmTrimmerData.baselineRms
     *
     * \param vmmId FEB VMM index
     * \param channelId VMM channel index
     *
     * \returns an std::pair containing the samples for this channel
     *          and the number of far outliers detected for this channel
     */
    std::pair<std::vector<short unsigned int>, std::size_t> readBaseline(std::size_t vmmId, std::size_t channelId);

    /*!
     * \brief Writes default mask (1) and trim (0) values for JSON
     *        consumption or writes the values calculated by the
     *        calibration.
     *
     * \param vmmId FEB index of the VMM on this front-end being sampled
     * \param reset Indicates that the values to be set will be the defaults
     */
    void setChTrimAndMask(std::size_t vmmId, bool reset=false);

    /*!
     * \brief Compute and apply the masks for hot and dead channels
     *
     * \param vmmId FEB index of the VMM on this front-end to calibrate.
     * \param median The median baseline value from all channels on this VMM
     *
     * \returns a \c VMMChannelSummary containing a summary of the
     *          noisy, hot, and dead channels, the sum RMS noise, and
     *          whether the baselines are considered bad.
     */
    nsw::calib::VMMChannelSummary maskChannels(std::size_t vmmId, std::size_t median);

    /*!
     * \brief Calculate the global threshold DAC value
     *
     * \param vmmId FEB index of the VMM on this front-end being sampled
     *
     * \returns the global threshold DAC value
     */
    nsw::calib::GlobalThrConstants calculateGlobalThreshold(std::size_t vmmId);

    /*!
     * \brief Calculates the threshold DAC value
     *
     * Function samples thresholds of all VMM channels at defined DAC values.
     * Then performs linear constant calculation and determines the
     * corresponding DAC value that matches the "guess" value. later, compares resulting
     * threshold in mV to the "guess" threshold in mV and notifies if difference is significant
     *
     * \param vmmId FEB VMM index
     * \param thDacTargetValue_mV desired value of the resulting threshold in mV
     *
     * \returns the threshold DAC parameters for a given FE board
     */
    nsw::calib::GlobalThrConstants calculateThrDacValue(std::size_t vmmId,
                                                        float thDacTargetValue_mV);

    /*!
     * \brief Compute the median channel trim DAC and effective global
     *        thereshold DAC for a VMM.
     *
     * Reads global threshold DAC with all trimmer DACs set to the middle
     * position for the purpose of determining if any channel
     * baseline is above the calculated global threshold to be used
     * later
     *
     * This function samples the VMM channel trim DAQ for all unmasked
     * channels and sets the VMM global values of:
     *   - \c m_febTrimmerData.midTrimMed
     *   - \c m_febTrimmerData.midEffThresh
     *
     * \param vmmId FEB index of the VMM on this front-end to calibrate.
     */
    void calculateVmmGlobalThreshold(std::size_t vmmId);

    /*!
     * \brief Perform a scan over the trim values
     *
     * Updates the calibration data object with the number of good and
     * total channels
     *
     * \param vmmId FEB index of the VMM on this front-end to calibrate.
     */
    void scanTrimmers(std::size_t vmmId);

    /*!
     * \brief Determines the linearity of trimmer DACs
     *
     *  - Samples the threshold DAC at 3 extreme trimmer DAC values (0,14,31)
     *  - Checks if at the middle position of trimmer DAC the effective threshold
     *    (V_threshold - V_baseline > 0) is positive
     *  - Performs a linear fit of the slopes of highest-to-middle point
     *    and lowest-to-middle points
     *
     * If the comparison of slopes declares that lower point slope is
     * not close or equal to the highest point slope, then function is
     * called again at the lowest trimmer DAC value increased by 2
     * DAC.
     *
     * The process repeats until the slopes of the lowest point
     * matches that of the middle of the trimmer DAC (14).
     *
     * \param vmmId FEB VMM index
     * \param channelId VMM channel index
     * \param thDac threshold DAC values with respect to which trimmer DAC is being sampled
     * \param trimPoints is an std::tuple containing the high, midlle, and low trimmer DAC points for this iteration
     *
     * \returns An `std::pair` holding the average slope and the high
     *          trim value used to perform the calculation
     */
    std::pair<float, std::size_t> findLinearRegionSlope(std::size_t vmmId,
                                                        std::size_t channelId,
                                                        std::size_t thDac,
                                                        const nsw::calib::TrimPoints& trimPoints);

    /*!
     * \brief Selects best trimmer DAC settings
     *
     * Find the best fitting trimmer DAC value for all VMM channels
     * creating a more-or-less uniform effective threshold over
     * all channels of the chip.

     * The function loops over all channels calling \c analyseChannelTrimmers
     * on the unmasked channels.
     *
     * If \c recalc is false, a check is done to determine if any
     * unmasked channels are above threshold.
     *
     * If there are no unmasked channels above threshold, a check is
     * performed on the number of channls with a bad trim value, and a
     * warning is issued if this is greater than a quarter of all
     * channels on this VMM
     *
     * Otherwise the function is recursively called once more, setting
     * \c recalc to true.
     * The largest updated threshold DAC offset from all chnnels,
     * provided it is below the trim circuit midpoint, is then used to
     * compute the new threshold DAC value for the second pass through
     * the analyzer.
     *
     * \param vmmId FEB VMM index
     * \param channelId VMM channel index
     * \param thDac threshold DAC value
     * \param thDacSlope slope of the fitted threshold DAC for the given VMM
     * \param recalc flag signifying that certain channels at best trimmer setting (default is false)
     */
    void analyseTrimmers(std::size_t vmmId,
                         std::size_t thDac,
                         float thDacSlope,
                         bool recalc = false);

    /*!
     * \brief Selects best trimmer DAC settings
     *
     * Find the best fitting trimmer DAC value for a VMM channel
     * that would create a more or less uniform effective threshold over
     * all channels of the chip.
     *
     * This function is called from \c analyseTrimmers on all unmasked channels
     *
     * This function checks
     *   - if the slope of the trimmer on this channel is usable
     *   - if channel is masked
     *   - if channel is noisy
     * later using the information about the trimmer DAC accessible range and the
     * slope/offset the best trimmer value is calculated
     *
     * This function sets
     *   - \c m_vmmTrimmerData.eff_thr_w_best_trim to the effective threshold (0 if the SCA sampling fails)
     *   - \c m_vmmTrimmerData.channel_trimmed_thr to the median (0 if the SCA sampling fails)
     *
     * \param vmmId FEB VMM index
     * \param channelId VMM channel index
     * \param thdac threshold DAC value
     * \param recalc flag signifying that certain channels at best trimmer setting
     */
    void analyseChannelTrimmers(std::size_t vmmId,
                                std::size_t channelId,
                                std::size_t thdac,
                                bool recalc);

    /*!
     * \brief Writes out the SCA calibration data for a VMM
     */
    void writeOutScaVmmCalib();

  public:
    // public data exposed to callers containing results

  private:
    // private data used internally by the VmmTrimmer calibration
    VmmTrimmerData m_vmmTrimmerData;  //<! Per-VMM channel calibration data
    FebTrimmerData m_febTrimmerData;  //<! Per-FEB VMM calibration data
    std::ofstream m_chCalibData;      //<! Per-VMM channel calibration output data
    boost::property_tree::ptree m_febOutJson;  //<!
  };
}  // namespace nsw

#endif
