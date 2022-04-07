#ifndef STGCPADTRIGGERINPUTDELAYS_H_
#define STGCPADTRIGGERINPUTDELAYS_H_

/**
 * \brief STGC Pad Trigger input delay calibration
 *
 * The purpose of this class is to calibrate Pad Trigger (PT) input delays.
 * The PT has 24 adjustable delays, one for each PFEB/TDS input.
 * The calibration works by looping through each available delay value,
 *   and reading the BCID of all PFEBs as recorded by the PT for that delay.
 * The units of the delay are clock cycles of a 240 MHz clock, i.e. 4.16ns.
 * There are 16 possible delays, i.e. the data of each PFEB can be delayed by
 *   as much as 66.6ns at the PT input.
 * The 4 LSB of the PFEB BCIDs are published as status registers.
 * The results of the calibration are saved as a ROOT TTree within a TFile.
 */

#include <string>
#include <vector>
#include "NSWCalibration/CalibAlg.h"
#include "ers/Issue.h"
#include "TFile.h"
#include "TTree.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadTriggerInputDelaysIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadTriggerInputDelays: public CalibAlg {

  public:
    sTGCPadTriggerInputDelays(std::string calibType, const hw::DeviceManager& deviceManager);
    sTGCPadTriggerInputDelays(const sTGCPadTriggerInputDelays&) = delete;
    sTGCPadTriggerInputDelays& operator=(const sTGCPadTriggerInputDelays&) = delete;
    void setup(const std::string& /* db */) override {};
    void configure() override;
    void unconfigure() override;

  private:
    void checkObjects();
    void setupTree();
    void closeTree();
    void setDelays(const nsw::hw::PadTrigger& pt) const;
    void fillTree();

    /// output ROOT file
    std::uint32_t m_delay = 0;
    std::string m_now = "";
    std::string m_rname = "";
    std::string m_opcserverip = "";
    std::string m_address = "";
    std::vector<std::uint32_t> m_bcid{};
    std::vector<std::uint32_t> m_pfeb{};
    std::unique_ptr<TFile> m_rfile;
    std::shared_ptr<TTree> m_rtree;
  };

}

#endif
