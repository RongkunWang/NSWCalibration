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
    sTGCPadsL1DDCFibers(std::string calibType, const hw::DeviceManager& deviceManager);
    void setup(const std::string& /* db */) override {};
    void configure() override;
    void unconfigure() override;

  private:
    void checkObjects() const;
    void setupTree();
    void closeTree();
    void fillTree();
    void setPhases();
    void setROCPhases() const;
    void setROCPhase(const nsw::hw::FEB& feb) const;
    std::vector<std::uint32_t> getPadTriggerBCIDs() const;
    bool isLeft(const std::string& name) const;
    bool isLeftStripped(const std::string& name) const;
    bool existsInDB(const std::string& name) const;
    const std::string m_reg{"ePllTdc.ePllPhase40MHz_0"};
    static constexpr std::uint32_t m_phaseStep{4};
    static constexpr std::uint32_t m_numReads{nsw::padtrigger::NUM_PFEB_BCID_READS/10};

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
    std::vector<std::uint32_t> m_pfeb{};
    std::vector<std::uint32_t> m_bcid{};
    std::vector<bool>          m_mask{};
    std::vector<bool>          m_left{};
  };

}

#endif
