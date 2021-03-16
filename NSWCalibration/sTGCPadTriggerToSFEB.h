#ifndef STGCPADTRIGGERTOSFEB_H_
#define STGCPADTRIGGERTOSFEB_H_

//
// Derived class for testing Pad Trigger to SFEB connection
//

#include <stdint.h>
#include <ostream>
#include <string>
#include <vector>
#include <future>

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadTriggerToSFEBIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadTriggerToSFEB: public CalibAlg {

  public:
    sTGCPadTriggerToSFEB(const std::string& calibType);
    virtual ~sTGCPadTriggerToSFEB() = default;
    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

  public:
    int sfeb_watchdog() const;
    std::vector<uint32_t> sfeb_register15(const nsw::FEBConfig & feb) const;
    std::string strf_time() const;

  private:
    bool m_dry_run = false;
    std::string m_calibType = "";
    std::vector<nsw::FEBConfig> m_sfebs = {};
    std::future<int> m_watchdog;

  };

}

#endif
