#ifndef STGCPADSHITRATEL1A_H_
#define STGCPADSHITRATEL1A_H_

/**
 * \brief STGC pads hit rate, as recorded by the pad trigger
 *
 * The purpose of this classes is to record L1A data with the pad trigger,
 *   for various VMM thresholds in the PFEBs. Given a nominal configuration,
 *   this calibration will record data with variations on that configuration.
 */

#include <array>
#include <chrono>
#include <string>
#include <ers/Issue.h>
#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/Constants.h"


ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsHitRateL1aIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsHitRateL1a: public CalibAlg {

  public:
    sTGCPadsHitRateL1a(std::string calibType, const hw::DeviceManager& deviceManager);
    void setup(const std::string& /* db */) override {};
    void configure() override;
    void acquire() override;
    void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary, const std::string& is_db_name) override;

  private:
    void checkObjects() const;
    void setPadTriggerSector() const;
    void setFebThresholds() const;
    void setFebThreshold(const nsw::hw::FEB& dev) const;
    void checkThresholdAdjustment(std::uint32_t thr, int adj) const;
    static constexpr std::array m_thresholdAdjustments{-20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80};
    std::chrono::seconds m_acquire_time{1};
  };

}

#endif
