#ifndef NSWCALIBRATION_STGCPADSROCTDS40MHZ_H_
#define NSWCALIBRATION_STGCPADSROCTDS40MHZ_H_

/**
 * \brief STGC ROC/TDS/PadTrigger 40MHz clock calibration
 *
 * The purpose of this class is to calibrate the ROC/TDS 40Mhz ePLL,
 *   which controls data processing in the TDS and therefore affects
 *   data received by the pad trigger. The motivation is to correct for
 *   fiber length differences between L-side and R-side STGC L1DDCs.
 */

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/Constants.h"

#include <ers/Issue.h>

#define R__HAS_STD_SPAN
#include <TFile.h>
#include <TTree.h>

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsRocTds40MhzIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsRocTds40Mhz: public CalibAlg {

  public:
    sTGCPadsRocTds40Mhz(std::string calibType, const hw::DeviceManager& deviceManager);
    void setup(const std::string& /* db */) override {};
    void configure() override;
    void unconfigure() override;

  private:
    void checkObjects() const;
    void setupTree();
    void closeTree();
    void fillTree();
    void setPadTriggerDelays() const;
    void setROCPhases() const;
    void setROCPhase(const nsw::hw::FEB& feb) const;
    std::vector<std::uint32_t> getPadTriggerBcids() const;
    std::vector<std::uint32_t> getPadTriggerBcidErrors() const;
    const std::string m_reg{"ePllTdc.ePllPhase40MHz_0"};
    static constexpr std::uint32_t m_phaseStep{2};
    static constexpr std::uint32_t m_numReads{10};

    /// output ROOT file
    std::uint32_t m_step{0};
    std::uint32_t m_reads{0};
    std::uint32_t m_phase{0};
    std::uint32_t m_pt_delay{0};
    std::uint32_t m_runnumber{0};
    std::string m_app_name{""};
    std::string m_now{""};
    std::string m_rname{""};
    std::string m_pt_name{""};
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};
    std::vector<std::uint32_t> m_pfeb{};
    std::vector<std::uint32_t> m_bcid{};
    std::vector<std::uint32_t> m_error{};
  };

}

#endif
