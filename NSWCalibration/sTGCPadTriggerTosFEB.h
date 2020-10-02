#ifndef STGCPADTRIGGERTOSFEB_H_
#define STGCPADTRIGGERTOSFEB_H_

//
// Derived class for testing Q1 sFEB to Router connection
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadTriggerTosFEBIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadTriggerTosFEB: public CalibAlg {

  public:
    sTGCPadTriggerTosFEB(std::string calibType);
    ~sTGCPadTriggerTosFEB() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    int sfeb_watchdog();
    std::vector<uint32_t> sfeb_register15(const nsw::FEBConfig & feb);
    std::string strf_time();

  private:
    bool m_dry_run;
    std::string m_calibType = "";
    std::vector<nsw::FEBConfig> m_sfebs = {};
    std::future<int> m_watchdog;

  };

}

#endif
