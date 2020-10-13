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
    ptree patterns();
    int configure_tds(ptree tr, bool unmask);
    int configure_tds(nsw::FEBConfig feb, int i_tds, bool unmask);
    void gather_sfebs();
    int pattern_number(std::string name);

  private:
    bool m_dry_run;
    ptree m_patterns;
    std::string m_calibType = "";
    std::vector<std::string> m_sfebs_ordered = {};
    std::vector<nsw::FEBConfig> m_sfebs = {};

  };

}

#endif
