#ifndef STGCSFEBTOROUTER_H_
#define STGCSFEBTOROUTER_H_

//
// Derived class for testing Q1 sFEB to Router connection
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCsFEBToRouterIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCsFEBToRouter: public CalibAlg {

  public:
    sTGCsFEBToRouter(std::string calibType);
    ~sTGCsFEBToRouter() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    ptree patterns();
    int configure_tds(const nsw::FEBConfig & feb, bool enable);
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
