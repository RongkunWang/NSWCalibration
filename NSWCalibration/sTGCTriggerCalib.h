#ifndef STGCTRIGGERCALIB_H_
#define STGCTRIGGERCALIB_H_

//
// Derived class for all things sTGC trigger calib
//

#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"

#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/hw/PadTrigger.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {
  class ConfigSender;

  class sTGCTriggerCalib: public CalibAlg {

  public:
    sTGCTriggerCalib(std::string calibType, const hw::DeviceManager& deviceManager) :
      CalibAlg(std::move(calibType), deviceManager),
      m_pts{getDeviceManager().getPadTriggers()} {};

    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

  public:
    int configureVMMs(nsw::FEBConfig feb, bool unmask);
    int configurePadTrigger() const;
    std::string nextPFEB(bool pop);
    bool orderPFEBs()        const {return m_order_pfebs;}
    void gatherPFEBs();
    int latencyScanOffset()  const {return m_offset_for_latency;}
    int latencyScanNBC()     const {return m_nbc_for_latency;}
    int latencyScanCurrent() const {return latencyScanOffset() + counter();}
    void setLatencyScanOffset(int val) {m_offset_for_latency = val;}
    void setLatencyScanNBC(int val)    {m_nbc_for_latency    = val;}

  private:
    std::reference_wrapper<const std::vector<hw::PadTrigger>> m_pts;
    bool m_order_pfebs = true;
    int m_offset_for_latency = 0;
    int m_nbc_for_latency = 0;
    std::vector<nsw::FEBConfig> m_pfebs = {};
    std::vector<std::string> m_pfebs_ordered = {};

  };

}

#endif
