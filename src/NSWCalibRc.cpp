#include <utility>  // make_pair
#include <string>
#include <memory>

// Header to the RC online services
#include "RunControl/Common/OnlineServices.h"

#include "NSWCalibration/NSWCalibRc.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"
#include "NSWCalibration/NSWCalibAlg.h"
#include "NSWCalibration/NSWCalibArtInputPhase.h"

using boost::property_tree::ptree;

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
    std::string g_calibration_type="";
    std::string g_info_server_name="";
    const std::string stateInfoName = g_info_server_name + ".CurrentCalibState";
    const std::string calibInfoName = g_info_server_name + "." + g_calibration_type + "CalibInfo";

    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf();
    
    ERS_LOG("End");
}

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");

    //Retrieving the ptree configuration to be modified
    ptree conf = m_NSWConfig->getConf();
    write_xml(std::cout, conf);

    return;

    //Sending the new configuration to be used for this run
    m_NSWConfig->substituteConf(conf);

    //Sending the configuration to the HW
    m_NSWConfig->configureRc();
    ERS_LOG("End");
}

void nsw::NSWCalibRc::prepareForRun(const daq::rc::TransitionCmd& cmd) {
    ERS_LOG("Start");
    end_of_run = 0;
    handler_thread = std::async(std::launch::async, &nsw::NSWCalibRc::handler, this);
    ERS_LOG("End");
}

void nsw::NSWCalibRc::disconnect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
    m_NSWConfig->unconfigureRc();
    ERS_INFO("End");
}

void nsw::NSWCalibRc::unconfigure(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");
    ERS_INFO("End");
}

void nsw::NSWCalibRc::stopRecording(const daq::rc::TransitionCmd& cmd) {
    ERS_LOG("Start");
    end_of_run = 1;
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

void nsw::NSWCalibRc::handler() {

  sleep(1);
  //
  // Im a sad hardcode for now
  //
  std::string fname = "json:///afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files/config_json/BB5/A10/full_small_sector_a10_bb5_ADDC_TP.json";

  // create calib object
  std::unique_ptr<NSWCalibAlg> calib = 0;
  if (true)
    calib = std::make_unique<NSWCalibArtInputPhase>();
  else
    throw std::runtime_error("Unknown calibration request");

  // setup
  calib->setup(fname);
  ERS_INFO("calib counter: " << calib->counter());
  ERS_INFO("calib total:   " << calib->total());

  // calib loop
  while (calib->next()) {
    if (end_of_run)
      break;
    ERS_INFO("Iteration " << calib->counter()+1 << " / " << calib->total());
    calib->configure();
    alti_toggle_pattern();
    calib->unconfigure();
  }

  // fin
  ERS_INFO("NSWCalibRc::handler::End of handler, exiting.");
}

void nsw::NSWCalibRc::alti_toggle_pattern() {
    ERS_INFO("alti_toggle_pattern()");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "toggle";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    // sendr.sendCommand(app_name, cmd);
    usleep(100e3);
}

