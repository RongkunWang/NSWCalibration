#ifndef STGCSFEBTOROUTER_H_
#define STGCSFEBTOROUTER_H_

//
// Derived class for testing Q1 SFEB to Router connection
//

#include <string>
#include <vector>
#include <future>
#include <fstream>

#include "NSWCalibration/CalibAlg.h"

#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/RouterConfig.h"
#include "NSWConfiguration/Utility.h"

#include "boost/property_tree/ptree.hpp"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCSFEBToRouterIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCSFEBToRouter: public CalibAlg {

  public:
    sTGCSFEBToRouter(const std::string& calibType);
    virtual ~sTGCSFEBToRouter() = default;
    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

  public:
    [[deprecated]]
    boost::property_tree::ptree patterns() const;
    int configure_tds(const nsw::FEBConfig & feb, bool enable) const;
    int configure_routers() const;
    int configure_router(const nsw::RouterConfig & router) const;
    void gather_sfebs();
    int router_watchdog(bool open, bool close);
    void wait_for_routers(size_t expectation) const;
    size_t count_ready_routers() const;
    bool router_ClkReady(const nsw::RouterConfig & router) const;

  private:
    boost::property_tree::ptree m_patterns;
    std::string m_calibType = "";
    std::vector<std::string> m_sfebs_ordered = {};
    std::vector<nsw::FEBConfig> m_sfebs = {};
    std::vector<nsw::RouterConfig> m_routers = {};
    std::future<int> m_watchdog;

  private:
    std::string m_fname;
    std::ofstream m_myfile;
    size_t m_routers_at_start;

  };

}

#endif
