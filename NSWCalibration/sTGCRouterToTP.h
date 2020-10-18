#ifndef STGCROUTERTOTP_H_
#define STGCROUTERTOTP_H_

//
// Derived class for testing Router to TP connections
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCRouterToTPIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCRouterToTP: public CalibAlg {

  public:
    sTGCRouterToTP(std::string calibType);
    ~sTGCRouterToTP() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    int configure_router(const nsw::RouterConfig & router, int hold_reset);
    std::string strf_time();

  private:
    bool m_dry_run;
    std::string m_calibType = "";
    std::vector<nsw::RouterConfig> m_routers = {};
  };

}

#endif
