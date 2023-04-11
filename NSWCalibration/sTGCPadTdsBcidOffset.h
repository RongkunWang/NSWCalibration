#ifndef NSWCALIBRATION_STGCPADTDSBCIDOFFSET_H
#define NSWCALIBRATION_STGCPADTDSBCIDOFFSET_H

/**
 * \brief Scan the STGC pad TDS BCID offset, and check alignment at the pad trigger
 *
 * The purpose of this class is to record SCA data with the pad trigger,
 *   for various pad TDS BCID offsets in the PFEBs.
 *
 * for each TDS BCID offset
 *   set the TDS BCID offset
 *   read the pad trigger alignment register
 */

#include <memory>

#include <ers/Issue.h>

#define R__HAS_STD_SPAN
#include <TFile.h>
#include <TTree.h>

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/Constants.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadTdsBcidOffsetIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadTdsBcidOffset: public CalibAlg {

  public:
    /**
     * \brief Simple constructor
     */
    sTGCPadTdsBcidOffset(std::string calibType, const hw::DeviceManager& deviceManager);

    /**
     * \brief Setup the calib alg, including checking DB and opening ROOT output
     */
    void setup(const std::string& /* db */) override;

    /**
     * \brief Configure the BCID offset in all TDSs
     */
    void configure() override;

    /**
     * \brief Acquire the pad trigger status register for BCID alignment
     */
    void acquire() override;

    /**
     * \brief Close the ROOT output on the last iteration
     */
    void unconfigure() override;

  private:
    /**
     * \brief Check the number of pad triggers and PFEBs in the configuration database
     */
    void checkObjects() const;

    /**
     * \brief Launch threads for setting BCID offsets in the TDSs
     */
    void setTdsBcidOffsets() const;

    /**
     * \brief Set BCID offset of a TDS
     */
    void setTdsBcidOffset(const nsw::hw::FEB& dev) const;

    /**
     * \brief Open a ROOT TTree and set the branches
     */
    void openTree();

    /**
     * \brief Fill the TTree with acquired pad trigger and TDS info
     */
    void fillTree(const std::uint64_t bcid_error);

    /**
     * \brief Close the TTree and write to disk
     */
    void closeTree();

    static constexpr std::string_view m_tdsReg{"BCID_Offset"};
    static constexpr std::uint64_t m_numBcPerOrbit{3564};

    // ROOT output
    std::string m_rname{""};
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};
    std::uint64_t m_pad_trigger_bcid_offset{0};
    std::uint64_t m_tds_bcid_offset{0};
    std::uint64_t m_pfeb_bcid_error{0};
    std::uint64_t m_runnumber{0};
    std::string m_pad_trigger{""};
  };

}

#endif
