#ifndef NSWCALIBRATION_STGCPADVMMTDSCHANNELS_H_
#define NSWCALIBRATION_STGCPADVMMTDSCHANNELS_H_

#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadVMMTDSChannelsIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  /**
   * \brief A class to check the mapping of PFEB VMM channel to TDS
   * channel via Pad Trigger readout.
   *
   * The purpose of this class is to check the mapping of VMM channels
   * in the pad trigger readout. VMM channels are routed to the TDS ASIC
   * on the PFEB, and the TDS sends trigger data to the Pad Trigger.
   * The pad trigger then applies its own mapping to the TDS channels
   * before packaging data for readout.
   *
   * The "calibration" works by looping through each available VMM channel,
   * generating test pulses for that channel, and reading L1A data from
   * the pad trigger.
   */
  class sTGCPadVMMTDSChannels: public CalibAlg {

  public:
    /**
     * \brief Simple constructor.
     */
    sTGCPadVMMTDSChannels();
    ~sTGCPadVMMTDSChannels() = default;

    /**
     * \brief Set up the calibration algorithm.
     *
     * This function creates NSWConfig objects from the given configuration database,
     * sets the total number of iterations based on the number of VMM channels,
     * and enables toggling of the ALTI pattern.
     *
     * \param db provides the connection to the configuration db
     */
    void setup(const std::string& db) override;

    /**
     * \brief Launch threads for each PFEB configuration.
     */
    void configure() override;

  private:
    /**
     * \brief Configure the PFEB VMMs for test pulsing.
     *
     * Choose the PFEB VMM channel to test pulse,
     * disable channel masking and enable test pulsing for that channel,
     * and send this configuration to the hardware.
     */
    void configure_pfeb(nsw::FEBConfig feb);

    /**
     * \brief Create NSWConfig objects from the given configuration database.
     */
    void setup_objects(const std::string& db);


  private:
    std::vector<nsw::FEBConfig> m_pfebs;
    std::vector<nsw::PadTriggerSCAConfig> m_pts;

  };

}

#endif
