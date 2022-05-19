#ifndef NSWCALIBRATION_VMMBASELINESCACALIBRATION_H
#define NSWCALIBRATION_VMMBASELINESCACALIBRATION_H

#include "NSWCalibration/ScaCalibration.h"

#include <ers/Issue.h>

ERS_DECLARE_ISSUE(nsw, VmmBaselineScaCalibrationIssue, message, ((std::string)message))

namespace nsw {
  /*!
   * \brief Class controlling the VMM baseline calibration for a single FEB
   *
   * Inherits from the ScaCalibration (or template instantiates an instance of?)
   */
  class VmmBaselineScaCalibration : public ScaCalibration
  {
    public:
    // public functions exposed to callers
    // Construct a calibration for all VMMs on this FEB
    VmmBaselineScaCalibration(std::reference_wrapper<const hw::FEB> feb,
                              std::string outpath,
                              std::size_t nSamples,
                              std::size_t rmsFactor,
                              std::size_t sector,
                              int wheel,
                              bool debug);

    void runCalibration() final;
    private:
    /*!
     * \brief FIXME TODO Separate baseline reading function for debug or diagnostic purposes
     *
     * Performs a separate, complete baseline check for a set of front-ends
     * it writes a separate output .txt file with all sample values and calculated sample RMS
     */
    void readBaselineFull();

    /*!
     * FIXME TODO move to PDOCalib
     * \brief Calibrates VMM internal pulser DAC analog part
     *
     * Calibrates the VMM internal pulser DAC necessary for the PDO calibration.
     * Writes two .txt files containing pure sample data per DAC value and separate file with
     * calculated DAC slope/offset data
     */
    void calibPulserDac();

    public:
    // public data exposed to callers containing results
    private:
    // private data used internally by the VmmBaseline calibration
  };

}  // namespace nsw

#endif
