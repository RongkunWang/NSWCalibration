#ifndef STGCPADSCONTROLPHASE_H_
#define STGCPADSCONTROLPHASE_H_

/**
 * \brief STGC ROC/TDS/PadTrigger TTC control phase
 *
 * The purpose of this class is to calibrate the ROC TTC control phase,
 *   which controls TTC signals sent to the TDS and therefore affects
 *   data received by the pad trigger.
 */

#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/hw/PadTrigger.h"
#include "NSWConfiguration/FEBConfig.h"

#include <ers/Issue.h>

#include <TFile.h>
#include <TTree.h>

using FebMask = std::bitset<nsw::padtrigger::NUM_PFEBS>;

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsControlPhaseIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsControlPhase: public CalibAlg {

  public:
    explicit sTGCPadsControlPhase(std::string calibType) : CalibAlg(std::move(calibType)) {};

    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

    std::string getCurrentPFEBName() const;
    std::uint32_t getPadTriggerRate() const;
    bool isIP(const std::string& pfeb_name) const;
    std::uint32_t getQuadRadius(const std::string& pfeb_name) const;
    void maskPFEBs() const;
    void maskPFEB(nsw::FEBConfig feb) const;
    bool setROCPhase() const;
    std::pair<FebMask, FebMask> getPadTriggerMask() const;
    void setPadTriggerMask() const;
    void fill();

  private:
    void setup_objects(const std::string& db);
    void setup_tree();
    void close_tree();
    const std::string the_reg{"reg107ePllTdc"};
    const std::string the_subreg{"ctrl_phase_0"};
    FebMask m_mask_to_0;
    FebMask m_mask_to_1;

    /// output ROOT file
    uint32_t m_delay{0};
    uint32_t m_pfeb_addr{0};
    uint32_t m_triggers{0};
    uint32_t m_runnumber{0};
    std::string m_app_name{""};
    std::string m_now{""};
    std::string m_pfeb_name{""};
    std::string m_rname{""};
    std::string m_pt_name{""};
    std::string m_pt_opcserverip{""};
    std::string m_pt_address{""};
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};

  private:
    std::vector<nsw::hw::PadTrigger> m_pts{};
    std::vector<nsw::FEBConfig> m_pfebs{};
  };

}

#endif
