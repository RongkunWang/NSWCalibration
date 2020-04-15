#include <utility>  // make_pair
#include <string>
#include <memory>

// Header to the RC online services
#include "RunControl/Common/OnlineServices.h"

#include "NSWCalibration/NSWCalibRc.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"


nsw::NSWCalibRc::NSWCalibRc(bool simulation):m_simulation {simulation} {
    ERS_LOG("Constructing NSWCalibRc instance");
    if (m_simulation) {
        ERS_INFO("Running in simulation mode, no configuration will be sent");
    }

}

void nsw::NSWCalibRc::configure(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
  
    //Retrieve the ipc partition
    m_ipcpartition = daq::rc::OnlineServices::instance().getIPCPartition();
    // Get the IS dictionary for the current partition
    is_dictionary = new ISInfoDictionary (m_ipcpartition);


    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf();
    
    //Retrieving the ptree configuration to be modified
    ptree conf = m_NSWConfig->getConf();

    //Sending the new configuration to be used for this run
    m_NSWConfig->substituteConf(conf);

    //Sending the configuration to the HW
    m_NSWConfig->configureRc();
    ERS_LOG("End");
}

void nsw::NSWCalibRc::unconfigure(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
    m_NSWConfig->unconfigureRc();
    ERS_INFO("End");
}


void nsw::NSWCalibRc::prepareForRun(const daq::rc::TransitionCmd& cmd) {
    ERS_LOG("Start");
    ERS_LOG("End");
}

void nsw::NSWCalibRc::stopRecording(const daq::rc::TransitionCmd& cmd) {
    ERS_LOG("Start");
    ERS_LOG("End");
}

void nsw::NSWCalibRc::user(const daq::rc::UserCmd& usrCmd) {
  ERS_LOG("User command received: " << usrCmd.commandName());
}

void nsw::NSWCalibRc::subTransition(const daq::rc::SubTransitionCmd& cmd) {
    auto main_transition = cmd.mainTransitionCmd();
    auto sub_transition = cmd.subTransition();

    ERS_LOG("Sub transition received: " << sub_transition << " (mainTransition: " << main_transition << ")");

    // This part should be in sync with NSWTTCConfig application. Some of this steps can also be a regular
    // state transition instead of a subTransition. SubTransitions are called before the main transition
    // This is not used in current software version, it may be used if one requires to configure different
    // boards at different times, instead of configuring everything at "configure" step.
    /*if (sub_transition == "CONFIGURE_ROC") {
      // configureROCs();
    } else if (sub_transition == "CONFIGURE_VMM") {
      // configureVMMs();
    } else {
      ERS_LOG("Nothing to do for subTransition" << sub_transition);
    }*/
}


