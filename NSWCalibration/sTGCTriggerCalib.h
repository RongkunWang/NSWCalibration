#ifndef STGCTRIGGERCALIB_H_
#define STGCTRIGGERCALIB_H_

//
// Derived class for all things sTGC trigger calib
//

#include <string>
#include <vector>

#include <ers/Issue.h>

#include <NSWConfiguration/hw/PadTrigger.h>
#include <NSWConfiguration/hw/FEB.h>

#include "NSWCalibration/CalibAlg.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWsTGCTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class sTGCTriggerCalib: public CalibAlg {

  public:
    /**
     * \brief Constructor with basic setup
     */
    sTGCTriggerCalib(std::string calibType, const nsw::hw::DeviceManager& deviceManager);

    /**
     * \brief Empty setup
     */
    void setup(const std::string& /* db */) override {};

    /**
     * \brief Configure the VMMs and pad trigger for test pulsing
     */
    void configure() override;

    /**
     * \brief Unconfigure the VMMs from test pulsing
     */
    void unconfigure() override;

  public:
    void checkCalibType() const;
    void checkObjects() const;
    void configureVMMs(const nsw::hw::FEB& feb, bool unmask) const;
    void configurePadTrigger() const;
    std::string_view getCurrentFebName() const;
    const nsw::hw::FEB& getCurrentFeb() const;
    std::uint32_t getLatencyScanOffset() const;
    std::uint32_t getLatencyScanNBC() const;
    std::uint32_t getLatencyScanCurrent() const {return getLatencyScanOffset() + counter();}
    void writeToFile(const std::vector<std::uint32_t>& rates) const;

  private:
    const bool m_latencyScan;

    static constexpr bool m_unmask{true};
    static constexpr bool m_mask{false};
    static constexpr std::uint32_t m_pulse{700};
    static constexpr std::uint32_t m_threshold{400};

  };

}

#endif
