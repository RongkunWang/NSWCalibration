#ifndef STGCPADSCONTROLPHASE_H_
#define STGCPADSCONTROLPHASE_H_

/**
 * \brief STGC ROC/TDS/PadTrigger TTC control phase
 *
 * The purpose of this class is to calibrate the ROC TTC control phase,
 *   which controls TTC signals sent to the TDS and therefore affects
 *   data received by the pad trigger.
 */

#include "NSWCalibration/CalibAlg.h"
#include <ers/Issue.h>
#include <TFile.h>
#include <TTree.h>

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsControlPhaseIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsControlPhase: public CalibAlg {

  public:
    sTGCPadsControlPhase(std::string calibType, const hw::DeviceManager& deviceManager);
    void setup(const std::string& /* db */) override {};
    void configure() override;
    void unconfigure() override;

  private:
    void checkObjects() const;
    void setupTree();
    void closeTree();
    void fillTree();
    void maskPFEBs() const;
    void maskPFEB(const nsw::hw::FEB& feb) const;
    void setROCPhases() const;
    void setROCPhase(const nsw::hw::FEB& feb) const;
    const std::string m_reg{"ePllTdc.ctrl_phase_0"};

    /// output ROOT file
    std::uint32_t m_delay{0};
    std::uint32_t m_pfeb_addr{0};
    std::uint32_t m_pfeb_rate{0};
    std::uint32_t m_runnumber{0};
    std::string m_app_name{""};
    std::string m_now{""};
    std::string m_pfeb_name{""};
    std::string m_rname{""};
    std::string m_pt_name{""};
    std::string m_pt_opcserverip{""};
    std::string m_pt_address{""};
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};

  };

}

#endif
