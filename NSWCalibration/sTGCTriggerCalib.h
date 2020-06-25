#ifndef STGCTRIGGERCALIB_H_
#define STGCTRIGGERCALIB_H_

//
// Derived class for all things sTGC trigger calib
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
using boost::property_tree::ptree;

namespace nsw {

  class sTGCTriggerCalib: public CalibAlg {

  public:
    sTGCTriggerCalib(std::string calibType);
    ~sTGCTriggerCalib() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    int configure_vmms(nsw::FEBConfig feb, bool unmask);
    int configure_pad_trigger();

  private:
    bool m_dry_run;
    int m_nbc_for_latency = 60;
    std::string m_calibType = "";
    std::vector<nsw::FEBConfig> m_pfebs = {};
    std::vector<nsw::PadTriggerSCAConfig> m_pts = {};
    std::map<std::string, std::unique_ptr<nsw::ConfigSender> > m_senders = {};

  };

}

#endif
