#ifndef NSWCALIBRATION_NSWCALIBRC_H_                                                                                                                                                                                                                                                                                                                                                                                                     
#define NSWCALIBRATION_NSWCALIBRC_H_
 
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <future>
 
#include "ipc/partition.h"
#include "ipc/core.h"
#include "is/info.h"
#include "is/infoT.h"
#include "is/infostream.h"
#include "is/infodynany.h"
#include "is/infodictionary.h"
#include "is/inforeceiver.h"

#include "ers/ers.h"
 
#include "RunControl/RunControl.h"
#include "RunControl/Common/RunControlCommands.h"

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/NSWConfig.h"
//#include "NSWConfiguration/NSWConfig.h"
 
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
    
    void connect(const daq::rc::TransitionCmd& cmd) override;
    
    void disconnect(const daq::rc::TransitionCmd& cmd) override;
 
    void prepareForRun(const daq::rc::TransitionCmd& cmd) override;
 
    void stopRecording(const daq::rc::TransitionCmd& cmd) override;
 
    void unconfigure(const daq::rc::TransitionCmd& cmd) override;
 
    void user(const daq::rc::UserCmd& cmd) override;
 
    void subTransition(const daq::rc::SubTransitionCmd& cmd) override;                        

    // void onExit(daq::rc::FSM_STATE) noexcept override;
    
    std::atomic<bool> end_of_run;
    std::future<void> handler_thread;
		std::string calibTypeFromIS(); 
    void handler();
		void publish4swrod();
		void wait4swrod();
    void alti_start_pat();
    void alti_stop_pat();
    void alti_send_reset(std::vector<std::string> hex_data);

 private:

//    std::string m_calibType = "PDOCalib";
    std::string m_calibType = "";
		std::string m_calibCounter_readback = "Monitoring.NSWCalibration.swrodCalibrationKey";
		std::unique_ptr<CalibAlg> calib;
		std::string dbcon;

   // Run the program in simulation mode, don't send any configuration
    bool m_simulation;
    bool reset_vmm;
    bool reset_tds;
    std::unique_ptr<NSWConfig> m_NSWConfig;
    IPCPartition m_ipcpartition;
    ISInfoDictionary* is_dictionary;
		ISInfoReceiver* m_rec;
           
   };
}    // namespace nsw
#endif  // NSWCALIBRATION_NSWCALIBRC_H_
  
