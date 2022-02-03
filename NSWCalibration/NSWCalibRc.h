#ifndef NSWCALIBRATION_NSWCALIBRC_H_
#define NSWCALIBRATION_NSWCALIBRC_H_

#include <string>
#include <vector>
#include <memory>
#include <future>

#include <ers/Issue.h>

#include <ipc/partition.h>
#include <is/infodictionary.h>
#include <is/inforeceiver.h>

#include <RunControl/RunControl.h>
#include <RunControl/Common/RunControlCommands.h>

#include "NSWConfiguration/NSWConfig.h"
#include "NSWCalibration/CalibAlg.h"

#include "NSWCalibrationDal/NSWCalibApplication.h"

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
    explicit NSWCalibRc(bool simulation=false);

    //! Connects to configuration database/ or reads file based config database
    //! Reads the names of front ends that should be configured and constructs
    //! FEBConfig objects in the map m_frontends
    void configure(const daq::rc::TransitionCmd& cmd) override;

    void connect(const daq::rc::TransitionCmd& cmd) override;

    void prepareForRun(const daq::rc::TransitionCmd& cmd) override;

    void stopRecording(const daq::rc::TransitionCmd& cmd) override;

    void disconnect(const daq::rc::TransitionCmd& cmd) override;

    void unconfigure(const daq::rc::TransitionCmd& cmd) override;

    void user(const daq::rc::UserCmd& usrCmd) override;

    // void onExit(daq::rc::FSM_STATE) noexcept override;

    //! Used to syncronize ROC/VMM configuration
    void subTransition(const daq::rc::SubTransitionCmd&) override;

    void publish() override;

    std::string calibTypeFromIS();
    bool simulationFromIS();
    std::pair<std::uint32_t, std::time_t> runParamsFromIS();
    void handler();
    void loop_content();
    void alti_setup();
    void alti_count();
    void publish4swrod();
    void wait4swrod();
    void alti_callback(ISCallbackInfo* isc);
    uint64_t alti_pg_duration(bool refresh = false);
    uint64_t alti_pg_multiplicity(std::string line);
    std::string alti_pg_file();
    std::string alti_monitoring(bool refresh = false);
    enum alti_pg_enum {pg_orb, pg_creq, pg_ttyp, pg_bgo, pg_l1a_ttr, pg_mult, pg_size};

 private:

    std::unique_ptr<CalibAlg> calib;
    std::string m_calibType             = "";
    std::string m_calibCounter          = "Monitoring.NSWCalibration.triggerCalibrationKey";
    std::string m_calibCounter_readback = "Monitoring.NSWCalibration.swrodCalibrationKey";
    std::string m_appname = "";
    uint64_t m_alti_pg_duration = 0;
    std::string m_alti_monitoring = "";

    const nsw::dal::NSWCalibApplication* m_nswApp;

    // Run the program in simulation mode, don't send any configuration
    bool                        m_simulation;
    bool                        m_simulation_lock = false;
    std::unique_ptr<NSWConfig>  m_NSWConfig;
    std::string                 m_dbcon;
    std::string                 m_orchestrator_name;
    bool                        m_resetVMM;
    bool                        m_resetTDS;
    std::string                 m_is_db_name;
    IPCPartition                m_ipcpartition;
    std::unique_ptr<ISInfoDictionary> is_dictionary;
    std::unique_ptr<ISInfoReceiver>   m_rec;

};
}  // namespace nsw
#endif  // NSWCALIBRATION_NSWCALIBRC_H_
