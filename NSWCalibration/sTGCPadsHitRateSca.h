#ifndef STGCPADSHITRATESCA_H_
#define STGCPADSHITRATESCA_H_

/**
 * \brief STGC pads hit rate, as recorded by the pad trigger
 *
 * The purpose of this classes is to record SCA DATA with the pad trigger,
 *   for various VMM thresholds in the PFEBs. Given a nominal configuration,
 *   this calibration will record data with variations on that configuration.
 *
 * for each VMM threshold
 *   set the VMM threshold
 *   for each TDS channel
 *     set the TDS channel
 *   for each PFEB
 *     set the PFEB
 *     read the PFEB rate
 */

#include <string>
#include <vector>
#include <ers/Issue.h>
#include <TFile.h>
#include <TTree.h>
#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/Constants.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsHitRateScaIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsHitRateSca: public CalibAlg {

  public:
    sTGCPadsHitRateSca(std::string calibType, const hw::DeviceManager& deviceManager);
    void setup(const std::string& /* db */) override {};
    void configure() override;
    void acquire() override;

  private:
    void checkObjects() const;
    void setupTree();
    void fillTree(const std::uint32_t pfeb,
                  const std::uint32_t rate);
    void closeTree();
    std::size_t getCurrentVmmThreshold() const;
    std::size_t getCurrentTdsChannel() const;
    __uint128_t getCurrentTdsChannelMask() const;
    bool updateVmmThresholds() const;
    void setCurrentTdsChannels() const;
    void setCurrentTdsChannel(const nsw::hw::FEB& dev) const;
    void setVmmThresholds() const;
    void setVmmThreshold(const nsw::hw::FEB& dev) const;
    void checkThresholdAdjustment(std::uint32_t thr, int adj) const;
    static constexpr std::array m_thresholdAdjustments{-30, -20, -10, 0, 10, 20, 30, 40, 50, 60, 70};
    static constexpr std::uint8_t m_regAddressChannelMask{0x2};

    // ROOT output
    std::string m_rname{""};
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};
    std::uint32_t m_tds_chan{0};
    std::uint32_t m_pfeb{0};
    std::uint32_t m_rate{0};
    std::uint32_t m_threshold{0};
    std::uint32_t m_runnumber{0};
    std::string m_app_name{""};
    std::string m_sector{""};
    std::string m_now{""};
    std::string m_pt_name{""};

  };

}

#endif
