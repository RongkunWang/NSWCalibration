#ifndef ROCCALIB_H_
#define ROCCALIB_H_

//
// Derived class for all things ROC calib
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"

using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWROCCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class ROCCalib: public CalibAlg {

  public:
    ROCCalib(std::string calibType);
    ~ROCCalib() {};
    void setup(std::string db);
    void configure();
    void unconfigure();
 
  private:
    [[nodiscard]] std::map<std::string, nsw::FEBConfig> splitConfigs() const;
    std::string m_db;
    bool m_dry_run; 
    std::string m_calibType = "";
    std::vector<nsw::FEBConfig> m_pfebs = {};
    std::map<std::string, std::unique_ptr<nsw::ConfigSender> > m_senders = {};

  };

}

#endif
