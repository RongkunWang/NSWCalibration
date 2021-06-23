#ifndef STGCROUTERTOTP_H_
#define STGCROUTERTOTP_H_

//
// Derived class for testing Router to TP connections
//

#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"

#include "NSWConfiguration/RouterConfig.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCRouterToTPIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCRouterToTP: public CalibAlg {

  public:
    sTGCRouterToTP(const std::string& calibType);
    virtual ~sTGCRouterToTP() = default;
    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

  public:
    int configure_router(const nsw::RouterConfig & router,
                         std::chrono::seconds hold_reset) const;

  private:
    bool m_dry_run = false;
    std::string m_calibType = "";
    std::vector<nsw::RouterConfig> m_routers = {};
  };

}

#endif
