#ifndef STGCTRIGGERCALIB_H_
#define STGCTRIGGERCALIB_H_

//
// Derived class for all things sTGC trigger calib
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

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
    std::string next_pfeb(bool pop);
    int latencyscan_offset()  {return m_offset_for_latency;}
    int latencyscan_nbc()     {return m_nbc_for_latency;}
    int latencyscan_current() {return latencyscan_offset() + counter();}
    bool order_pfebs()        {return m_order_pfebs;}
    void gather_pfebs();
    void set_latencyscan_offset(int val) {m_offset_for_latency = val;}
    void set_latencyscan_nbc(int val)    {m_nbc_for_latency    = val;}

  private:
    bool m_dry_run;
    bool m_order_pfebs = 1;
    int m_offset_for_latency = 0;
    int m_nbc_for_latency = 0;
    std::string m_calibType = "";
    std::vector<nsw::FEBConfig> m_pfebs = {};
    std::vector<nsw::PadTriggerSCAConfig> m_pts = {};
    std::map<std::string, std::unique_ptr<nsw::ConfigSender> > m_senders = {};
    std::vector<std::string> m_pfebs_ordered = {};

  };

}

#endif
