#ifndef NSWCALIBRATION_NSWCALIBRC_H_
#define NSWCALIBRATION_NSWCALIBRC_H_

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <future>

#include "ers/ers.h"

#include "RunControl/RunControl.h"
#include "RunControl/Common/RunControlCommands.h"

#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/ConfigReader.h"

using boost::property_tree::ptree;

ERS_DECLARE_ISSUE(nsw,
                  NSWCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {
	class FEBConfig;
}

namespace nsw {
class NSWCalibRc: public daq::rc::Controllable {
 public:
    // override only the needed methods
    explicit NSWCalibRc(bool simulation = false);
    virtual ~NSWCalibRc() noexcept {}

    //! Connects to configuration database/ or reads file based config database
    //! Reads the names of front ends that should be configured and constructs
    //! FEBConfig objects in the map m_frontends
    void configure(const daq::rc::TransitionCmd& cmd) override;

    void prepareForRun(const daq::rc::TransitionCmd& cmd) override;

    void stopRecording(const daq::rc::TransitionCmd& cmd) override;

    void unconfigure(const daq::rc::TransitionCmd& cmd) override;

    void user(const daq::rc::UserCmd& cmd) override;

    // void onExit(daq::rc::FSM_STATE) noexcept override;

 private:

    //! Count how many threads are running
    size_t active_threads();
    bool too_many_threads();

    //! Calibration functions
    // void calibrateARTPhase(); // or something

    std::unique_ptr<nsw::ConfigReader> m_reader;
    std::unique_ptr<nsw::ConfigSender> m_sender;

    // Run the program in simulation mode, don't send any configuration
    bool m_simulation;


    // thread management
    size_t m_max_threads;
    std::unique_ptr< std::vector< std::future<void> > > m_threads;

};
}  // namespace nsw
#endif  // NSWCALIBRATION_NSWCALIBRC_H_
