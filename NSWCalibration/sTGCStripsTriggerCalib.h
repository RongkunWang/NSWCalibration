#ifndef STGCSTRIPSTRIGGERCALIB_H_
#define STGCSTRIPSTRIGGERCALIB_H_

//
// Derived class for all things sTGC strips trigger calib
//
#include <vector>
#include <utility>

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCStripsTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCStripsTriggerCalib: public CalibAlg {

  public:
    sTGCStripsTriggerCalib(const std::string& calibType);
    virtual ~sTGCStripsTriggerCalib() = default;
    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

  public:
    int configure_tds(const std::string& name, const std::vector<std::string>& tdss, bool prbs_e, bool pause);
    int configure_tds(const nsw::FEBConfig& feb, const std::string& tds, bool prbs_e);
    void gather_sfebs();
    std::vector<std::pair <std::string, std::string > > router_recovery_tds();
    bool dont_touch(const std::string& name, const std::string& tds);
    std::string simplified(const std::string& name) const;

  private:
    std::string m_calibType = "";
    std::vector<std::string> m_sfebs_ordered = {};
    std::vector<nsw::FEBConfig> m_sfebs = {};
    std::vector<std::pair <std::string, std::string > > m_router_recovery_tds = {};
    std::vector<std::vector<std::string> > m_tdss = {};

  };

}

#endif
