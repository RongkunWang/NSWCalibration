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
#include "NSWConfiguration/hw/PadTrigger.h"
#include "NSWConfiguration/FEBConfig.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCPadsHitRateScaIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCPadsHitRateSca: public CalibAlg {

  public:
    sTGCPadsHitRateSca(std::string calibType, const hw::DeviceManager& deviceManager);
    void setup(const std::string& db) override {};
    void configure() override;
    void acquire() override;

  private:
    std::size_t getAcquireTime() const;
    void checkObjects() const;
    std::size_t getCurrentMetadata() const;
    std::size_t getCurrentVmmThreshold() const;
    // std::size_t getCurrentPadTriggerPfeb() const;
    std::size_t getCurrentTdsChannel() const;
    bool updateVmmThresholds() const;
    // bool updateCurrentPadTriggerPfeb() const;
    void setCurrentMetadata() const;
    // void setCurrentPadTriggerPfeb() const;
    void setCurrentTdsChannels() const;
    void setCurrentTdsChannel(const nsw::hw::FEB& dev) const;
    void setVmmThresholds() const;
    void setVmmThreshold(const nsw::hw::FEB& dev) const;
    void checkThresholdAdjustment(std::uint32_t thr, int adj) const;
    std::vector<int> m_thresholdAdjustments
      = {-20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80};
    /* std::vector<int> m_thresholdAdjustments */
    /*   = {0, 10}; */
    std::size_t m_acquire_time{1};

    const std::size_t NTDSCHAN{104};
    // const std::size_t NPFEB{24};
    // const std::size_t NTDSCHAN{5};
    const std::size_t NPFEB{3};
    static constexpr std::uint8_t m_regAddressChannelMask{0x2};

    // ROOT output
    std::unique_ptr<TFile> m_rfile{nullptr};
    std::shared_ptr<TTree> m_rtree{nullptr};

  };

}

#endif
