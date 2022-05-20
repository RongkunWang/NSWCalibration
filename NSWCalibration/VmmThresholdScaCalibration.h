#ifndef NSWCALIBRATION_VMMTHRESHOLDSCACALIBRATION_H
#define NSWCALIBRATION_VMMTHRESHOLDSCACALIBRATION_H

#include "NSWCalibration/ScaCalibration.h"

#include <ers/Issue.h>

ERS_DECLARE_ISSUE(nsw, VmmThresholdScaCalibrationIssue, message, ((std::string)message))

namespace nsw {
  /*!
   * \brief Class controlling the VMM threshold calibration for a single FEB
   *
   * Inherits from the ScaCalibration (or template instantiates an instance of?)
   */
  class VmmThresholdScaCalibration : public ScaCalibration
  {
    public:
    // public functions exposed to callers
    // Construct a calibration for all VMMs on this FEB
    VmmThresholdScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                              std::string outpath,
                              std::size_t nSamples,
                              std::size_t rmsFactor,
                              std::size_t sector,
                              int wheel,
                              bool debug);

    void runCalibration() final;
    private:
    /*!
     * \brief FIXME TODO Separate threshold reading function for debug or diagnostic purposes
     *
     * Performs a separate, complete threshold check for a set of front-ends
     * it writes a separate output .txt file with all sample values and calculated sample RMS
     */
    void readThresholdFull();

    public:
    // public data exposed to callers containing results
    private:
    // private data used internally by the VmmThreshold calibration
  };

}  // namespace nsw

#endif
