#ifndef STGCSFEBTOROUTER_H_
#define STGCSFEBTOROUTER_H_

//
// Derived class for testing Q1 SFEB to Router connection
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCSFEBToRouterIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCSFEBToRouter: public CalibAlg {

  public:
    sTGCSFEBToRouter(std::string calibType);
    ~sTGCSFEBToRouter() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    ptree patterns();
    int configure_tds(const nsw::FEBConfig & feb, bool enable);
    int configure_routers();
    int configure_router(const nsw::RouterConfig & router);
    void gather_sfebs();
    int pattern_number(std::string name);
    int router_watchdog();
    bool router_ClkReady(const nsw::RouterConfig & router);
    std::string strf_time();

  private:
    bool m_dry_run;
    ptree m_patterns;
    std::string m_calibType = "";
    std::vector<std::string> m_sfebs_ordered = {};
    std::vector<nsw::FEBConfig> m_sfebs = {};
    std::vector<nsw::RouterConfig> m_routers = {};
    std::future<int> m_watchdog;

  };

}

#endif