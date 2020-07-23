#include <utility>  // make_pair
#include <string>
#include <memory>

// Header to the RC online services
#include "RunControl/Common/OnlineServices.h"

#include "NSWCalibration/NSWCalibRc.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"
#include "NSWCalibration/MMTriggerCalib.h"
#include "NSWCalibration/sTGCTriggerCalib.h"
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

    // Retrieving the configuration db
    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    const daq::core::RunControlApplicationBase& rcBase = rcSvc.getApplication();
    const nsw::dal::NSWCalibApplication* nswApp = rcBase.cast<nsw::dal::NSWCalibApplication>();
    m_dbcon = nswApp->get_dbConnection();
    m_resetVMM = nswApp->get_resetVMM();
    m_resetTDS = nswApp->get_resetTDS();
    ERS_INFO("DB Configuration: " << m_dbcon);
    ERS_INFO("reset VMM: " << m_resetVMM);
    ERS_INFO("reset TDS: " << m_resetTDS);
    // Retrieve the ipc partition
    m_ipcpartition = rcSvc.getIPCPartition();

    // Get the IS dictionary for the current partition
    is_dictionary = new ISInfoDictionary (m_ipcpartition);
    std::string g_calibration_type="";
    std::string g_info_server_name="";
    const std::string stateInfoName = g_info_server_name + ".CurrentCalibState";
    const std::string calibInfoName = g_info_server_name + "." + g_calibration_type + "CalibInfo";

    // Announce the current calibType, and
    // publish metadata to IS (in progress)
    auto tmp = calibTypeFromIS();
    publish4swrod();

    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf(nswApp);

    ERS_LOG("End");
}

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");

    // Announce the current calibType (again)
    auto tmp = calibTypeFromIS();

    // Retrieving the ptree configuration to be modified
    ptree conf = m_NSWConfig->getConf();

    // Sending the new configuration to be used for this run
    m_NSWConfig->substituteConf(conf);

    // Sending the configuration to the HW
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
  calib = 0;
  m_calibType = calibTypeFromIS();
  if (m_calibType=="MMARTConnectivityTest" ||
      m_calibType=="MMTrackPulserTest" ||
      m_calibType=="MMCableNoise" ||
      m_calibType=="MMARTPhase" ||
      m_calibType=="MML1ALatency") {
    calib = std::make_unique<MMTriggerCalib>(m_calibType);
  } else if (m_calibType=="sTGCPadConnectivity" ||
             m_calibType=="sTGCPadLatency") {
    calib = std::make_unique<sTGCTriggerCalib>(m_calibType);
  } else {
    std::string msg = "Unknown calibration request: " + m_calibType;
    nsw::NSWCalibIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw std::runtime_error(msg);
  }

  // setup
  calib->setup(m_dbcon);
  ERS_INFO("calib counter:    " << calib->counter());
  ERS_INFO("calib total:      " << calib->total());
  ERS_INFO("calib toggle:     " << calib->toggle());
  ERS_INFO("calib wait4swrod: " << calib->wait4swrod());

  // calib loop
  while (calib->next()) {
    if (end_of_run)
      break;
    publish4swrod();
    calib->progressbar();
    calib->configure();
    if (calib->toggle())
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
    ERS_LOG("alti_toggle_pattern()");
    std::string app_name = "Alti_RCD";
    std::string cmd_name = "StartPatternGenerator";
    daq::rc::UserCmd cmd(cmd_name, std::vector<std::string>());
    daq::rc::CommandSender sendr(m_ipcpartition.name(), "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
    usleep(100e3);
}

void nsw::NSWCalibRc::publish4swrod() {
  //
  // Commented out for now.
  // This is powerful and risky.
  //
  // if (calib) {
  //   is_dictionary->checkin(m_calibCounter, ISInfoInt(calib->counter()));
  // } else {
  //   is_dictionary->checkin(m_calibCounter, ISInfoInt(-1));
  // }
  // wait4swrod();
}

void nsw::NSWCalibRc::wait4swrod() {
  if (!calib)
    return;
  if (!calib->wait4swrod())
    return;
  ISInfoInt counter(-1);
  ERS_INFO("calib waiting for swROD...");
  int attempt_i = 0;
  int attempts_max = 5;
  while (counter.getValue() != calib->counter()) {
    try {
      is_dictionary->getValue(m_calibCounter_readback, counter);
    } catch(daq::is::Exception& ex) {
      ers::error(ex);
    }
    // usleep(100e3);
    ERS_INFO("calib waiting for swROD, attempt " << attempt_i);
    usleep(100e3);
    // usleep(1e6);
    attempt_i++;
    if (attempt_i >= attempts_max)
      throw std::runtime_error("Waiting for swROD failed");
  }
}

std::string nsw::NSWCalibRc::calibTypeFromIS() {
  // Grab the calibration type string from IS
  // Can manually write to this variable from the command line:
  // > is_write -p part-BB5-Calib -n Setup.NSW.calibType -t String  -v MMARTPhase -i 0
  // > is_ls -p part-BB5-Calib -R ".*NSW.cali.*" -v
  // Currently supported options are written in the `handler` function.
  std::string calibType;
  if(is_dictionary->contains("Setup.NSW.calibType") ){
    ISInfoDynAny calibTypeFromIS;
    is_dictionary->getValue("Setup.NSW.calibType", calibTypeFromIS);
    calibType = calibTypeFromIS.getAttributeValue<std::string>(0);
    ERS_INFO("Calibration type from IS: " << calibType);
  } else {
    calibType = "MMARTConnectivityTest";
    nsw::NSWCalibIssue issue(ERS_HERE, "Calibration type not found in IS. Defaulting to: " + calibType);
    ers::warning(issue);
  }
  return calibType;
}
