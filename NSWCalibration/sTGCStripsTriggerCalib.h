#ifndef STGCSTRIPSTRIGGERCALIB_H_
#define STGCSTRIPSTRIGGERCALIB_H_

//
// Derived class for all things sTGC strips trigger calib
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCStripsTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCStripsTriggerCalib: public CalibAlg {

  public:
    sTGCStripsTriggerCalib(std::string calibType);
    ~sTGCStripsTriggerCalib() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    int configure_tds(std::string name, std::vector<std::string> tdss, bool prbs_e);
    int configure_tds(nsw::FEBConfig feb, std::string tds, bool prbs_e);
    void gather_sfebs();
    std::vector<std::pair <std::string, std::string > > router_recovery_tds();
    bool dont_touch(std::string name, std::string tds);
    std::string simplified(std::string name);

  private:
    std::string m_calibType = "";
    std::vector<std::string> m_sfebs_ordered = {};
    std::vector<nsw::FEBConfig> m_sfebs = {};
    std::vector<std::pair <std::string, std::string > > m_router_recovery_tds = {};
    std::vector< std::vector<std::string> > m_tdss = {};

  };

}

#endif
