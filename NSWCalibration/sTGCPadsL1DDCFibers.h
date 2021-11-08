#ifndef STGCPADSL1DDCFIBERS_H_
#define STGCPADSL1DDCFIBERS_H_

/**
 * \brief STGC ROC/TDS/PadTrigger ePLL phase delay calibration.
 *
 * The purpose of this class is to calibrate the ROC/TDS 40Mhz ePLL,
 *   which controls data processing in the TDS and therefore affects
 *   data received by the pad trigger. The motivation is to correct for
 *   fiber length differences between L-side and R-side STGC L1DDCs.
 */

#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/PadTriggerSCAConfig.h"
#include "NSWConfiguration/hw/PadTrigger.h"
#include "NSWConfiguration/FEBConfig.h"

#include <ers/Issue.h>

#include <TFile.h>
#include <TTree.h>

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsL1DDCFibersIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsL1DDCFibers: public CalibAlg {

  public:
    explicit sTGCPadsL1DDCFibers(const std::string& calibType) : CalibAlg(std::move(calibType)) {};
    void setup(const std::string& db) override;
    void configure() override;
    void unconfigure() override;

    std::vector<std::uint32_t> getPadTriggerBCIDs() const;
    void setROCPhase() const;
    bool isLeft(const std::string& name) const;
    bool isLeftStripped(const std::string& name) const;
    void fill();

  private:
    void setupObjects(const std::string& db);
    void setupTree();
    void closeTree();
    bool existsInDB(const std::string& name) const;
    const std::string the_reg{"reg096ePllTdc"};
    const std::string the_subreg{"ePllPhase40MHz_0"};
    static constexpr std::uint32_t phase_step{4};
    static constexpr std::uint32_t num_reads{nsw::padtrigger::NUM_PFEB_BCID_READS/10};

    /// output ROOT file
    std::uint32_t m_step{0};
    std::uint32_t m_reads{0};
    std::uint32_t m_phase_L{0};
    std::uint32_t m_phase_R{0};
    std::uint32_t m_runnumber{0};
    std::string m_app_name{""};
    std::string m_now{""};
    std::string m_rname{""};
    std::string m_pt_name{""};
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};
    std::unique_ptr< std::vector<std::uint32_t> > m_pfeb{nullptr};
    std::unique_ptr< std::vector<std::uint32_t> > m_bcid{nullptr};
    std::unique_ptr< std::vector<bool> >          m_mask{nullptr};
    std::unique_ptr< std::vector<bool> >          m_left{nullptr};

    std::unique_ptr<nsw::hw::PadTrigger> m_pt{nullptr};
    std::vector<nsw::FEBConfig> m_pfebs{};
  };

}

#endif
