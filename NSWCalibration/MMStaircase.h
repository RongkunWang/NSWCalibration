#ifndef MMSTAIRCASE_H_
#define MMSTAIRCASE_H_

//
// Derived class for testing Router to TP connections
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/ADDCConfig.h"
/**
ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCRouterToTPIssue,
                  message,
                  ((std::string)message)
                  )
**/

namespace nsw {
 
  class MMStaircase: public CalibAlg {

  public:
    MMStaircase(std::string calibType);
    ~MMStaircase() {};
    void setup(std::string db);
    void configure();
    void unconfigure();
  
  private:
    bool m_dry_run;
    std::string m_calibType = "";
    std::vector<nsw::ADDCConfig> m_addcs = {};
  };

}

#endif
