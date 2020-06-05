#include <utility>  // make_pair
#include <string>
#include <memory>

// Header to the RC online services
#include "RunControl/Common/OnlineServices.h"

#include "NSWCalibration/NSWCalibRc.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"
#include "NSWCalibration/CalibAlg.h"
#include "NSWCalibration/MMTriggerCalib.h"
#include "NSWConfiguration/NSWConfig.h"

using boost::property_tree::ptree;

nsw::NSWCalibRc::NSWCalibRc(bool simulation):m_simulation {simulation} {
    ERS_LOG("Constructing NSWCalibRc instance");
    if (m_simulation) {
        ERS_INFO("Running in simulation mode, no configuration will be sent");
    }
}

void nsw::NSWCalibRc::configure(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");

    //Retrieving the configuration db
    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    const daq::core::RunControlApplicationBase& rcBase = rcSvc.getApplication();
    const nsw::dal::NSWCalibApplication* nswApp = rcBase.cast<nsw::dal::NSWCalibApplication>();
    m_dbcon = nswApp->get_dbConnection();
    m_resetVMM = nswApp->get_resetVMM();
    m_resetTDS = nswApp->get_resetTDS();
    ERS_INFO("DB Configuration: " << m_dbcon);
    ERS_INFO("reset VMM: " << m_resetVMM);
    ERS_INFO("reset TDS: " << m_resetTDS);
    //Retrieve the ipc partition
    m_ipcpartition = rcSvc.getIPCPartition();

    // Get the IS dictionary for the current partition
    is_dictionary = new ISInfoDictionary (m_ipcpartition);
    std::string g_calibration_type="";
    std::string g_info_server_name="";
    const std::string stateInfoName = g_info_server_name + ".CurrentCalibState";
    const std::string calibInfoName = g_info_server_name + "." + g_calibration_type + "CalibInfo";

    // Currently supported options are:
    //    MMARTConnectivityTest
    //    MMTrackPulserTest
    //    MMARTPhase

    // Going to attempt to grab the calibration type string from IS
    // Can manually write to this variable from the command line:
    // > is_write -p part-BB5-Calib -n Setup.NSW.calibType -t String  -v MMARTPhase -i 0
    // > is_ls -p part-BB5-Calib -R ".*NSW.cali.*" -v

    ISInfoDynAny calibTypeFromIS;
    if(is_dictionary->contains("Setup.NSW.calibType") ){
      is_dictionary->getValue("Setup.NSW.calibType", calibTypeFromIS);
      m_calibType = calibTypeFromIS.getAttributeValue<std::string>(0);
      ERS_INFO("Calibration type from IS: " << m_calibType);
    } else {
      m_calibType = "MMARTConnectivityTest";
      nsw::NSWConfigIssue issue(ERS_HERE, "Calibration type not found in IS. Defaulting to: " + m_calibType);
      ers::warning(issue);
    }

    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf(nswApp);

    ERS_LOG("End");
}

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");

    //Retrieving the ptree configuration to be modified
    ptree conf = m_NSWConfig->getConf();
    write_xml(std::cout, conf);

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

  // create calib object
  std::unique_ptr<CalibAlg> calib = 0;
  ERS_INFO("Calibration Type: " << m_calibType);
  if (m_calibType=="MMARTConnectivityTest" ||
      m_calibType=="MMTrackPulserTest" ||
      m_calibType=="MMCableNoise" ||
      m_calibType=="MMARTPhase"){
    calib = std::make_unique<MMTriggerCalib>(m_calibType);
  } else {
    throw std::runtime_error("Unknown calibration request");
  }

  // setup
  calib->setup(m_dbcon);
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
  ERS_INFO("NSWCalibRc::handler::End of handler");
}

void nsw::NSWCalibRc::alti_toggle_pattern() {
    //
    // NB: the StartPatternGenerator logic is:
    //   if pg enabled
    //     stop it
    //   start it
    //
    ERS_INFO("alti_toggle_pattern()");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "StartPatternGenerator";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
}

