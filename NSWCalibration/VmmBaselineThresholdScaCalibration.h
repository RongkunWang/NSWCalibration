#ifndef NSWCALIBRATION_VMMBASELINETHRESHOLDSCACALIBRATION_H
#define NSWCALIBRATION_VMMBASELINETHRESHOLDSCACALIBRATION_H

#include "NSWCalibration/ScaCalibration.h"

#include <ers/Issue.h>

namespace nsw {
  /*!
   * \brief Class controlling the baseline and threshold calibrations for the VMMs on one FEB
   *
   * Inherits from the ScaCalibration
   */
  class VmmBaselineThresholdScaCalibration : public ScaCalibration
  {
    public:
    VmmBaselineThresholdScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                              std::string outpath,
                              std::size_t nSamplesBaseline,
                              std::size_t nSamplesThreshold,
                              std::size_t sector,
                              int wheel,
                              bool debug);

    std::size_t m_nSamplesBaseline;
    std::size_t m_nSamplesThreshold;

    /*!
     * \brief Runs calibration loop for all channels associated with this FEB
     *        * Baselines
     *        * Thresholds
     *        Outputs are written to text files
     */
    void runCalibration() final;
    private:

    /*!
     * \brief Read channel threshold for vmmId,channelId and write a line to output file
     *
     * \param outputFile
     * \param vmmId
     * \param channelId
     */
    void readChannelThreshold(std::ofstream& outputFile, std::size_t vmmId, std::size_t channelId);

    /*!
     * \brief Read baseline for vmmId,channelId and write a line to output file
     *
     * \param outputFile
     * \param vmmId
     * \param channelId
     */
    void readChannelBaseline(std::ofstream& outputFile, std::size_t vmmId, std::size_t channelId);

  };

}  // namespace nsw

#endif
