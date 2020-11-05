#include <utility>  // make_pair
#include <string>
#include <memory>

// Header to the RC online services
#include "RunControl/Common/OnlineServices.h"

#include "NSWCalibration/NSWCalibRc.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"
#include "NSWCalibration/MMTriggerCalib.h"
#include "NSWCalibration/sTGCTriggerCalib.h"
#include "NSWCalibration/sTGCStripsTriggerCalib.h"
#include "NSWCalibration/SCAIDCalib.h"
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
    m_scaIdTable = nswApp->get_scaIdTable();

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
    m_calibType = calibTypeFromIS();
    publish4swrod();

    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf(nswApp);

    ERS_LOG("End");
}

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd& cmd) {
    ERS_INFO("Start");

    // Announce the current calibType (again)
    m_calibType = calibTypeFromIS();

    // Retrieving the ptree configuration to be modified
    ptree conf = m_NSWConfig->getConf();

    // Sending the new configuration to be used for this run
    m_NSWConfig->substituteConf(conf);

    // Sending the configuration to the HW
    m_NSWConfig->configureRc();

    // End
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
      m_calibType=="MMARTConnectivityTestAllChannels" ||
      m_calibType=="MMTrackPulserTest" ||
      m_calibType=="MMCableNoise" ||
      m_calibType=="MMARTPhase" ||
      m_calibType=="MML1ALatency") {
    calib = std::make_unique<MMTriggerCalib>(m_calibType);
  } else if (m_calibType=="sTGCPadConnectivity" ||
             m_calibType=="sTGCPadLatency") {
    calib = std::make_unique<sTGCTriggerCalib>(m_calibType);
  } else if (m_calibType=="sTGCFakeStripConnectivity") {
    calib = std::make_unique<sTGCStripsTriggerCalib>(m_calibType);
  } else if (m_calibType == "SCAIDCheck") {
    calib = std::make_unique<ScaIdCalib>(m_scaIdTable);
  } else {
    std::string msg = "Unknown calibration request: " + m_calibType;
    nsw::NSWCalibIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw std::runtime_error(msg);
  }

  // setup
  alti_setup();
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
  alti_count();
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

    // sleep until the pattern should be finished
    // safety factor of 2x
    usleep(2 * alti_pg_duration());
}

void nsw::NSWCalibRc::alti_setup() {
  alti_monitoring(1);
  alti_pg_duration(1);
  if (alti_monitoring() != "") {
    m_rec = new ISInfoReceiver(m_ipcpartition, false);
    m_rec->subscribe(alti_monitoring(), &nsw::NSWCalibRc::alti_callback);
  }
}

std::string nsw::NSWCalibRc::alti_monitoring(bool refresh) {
  if (!refresh)
    return m_alti_monitoring;
  ISInfoStream ii(m_ipcpartition, "Monitoring");
  while (!ii.eof()) {
    auto name = ii.name();
    ERS_LOG("Looping through Monitoring server: " << name);
    if (name.find("AltiMonitoring") != std::string::npos) {
      ERS_INFO("ALTI monitoring server: " << name);
      m_alti_monitoring = name;
      return name;
    } else {
      ii.skip();
    }
  }
  std::string msg = "Cannot find AltiMonitoring in IS. Will fly blind.";
  nsw::NSWCalibIssue issue(ERS_HERE, msg);
  ers::warning(issue);
  m_alti_monitoring = "";
  return m_alti_monitoring;
}

std::string nsw::NSWCalibRc::alti_pg_file() {
  //
  // Look up the ALTI PG file
  //
  try {
    ISInfoDynAny any;
    if (alti_monitoring() == "")
      return "";
    is_dictionary->getValue(alti_monitoring(), any);
    auto pg_file = any.getAttributeValue<std::string>("pg_file");
    return pg_file;
  } catch(daq::is::Exception& ex) {
    ers::warning(ex);
  }
  return "";
}

uint64_t nsw::NSWCalibRc::alti_pg_duration(bool refresh) {
  //
  // Read the pg file, and sum the number of BCs
  // i.e. the duration of one iteration
  // final units: microseconds
  //
  if (!refresh)
    return m_alti_pg_duration;
  auto fname = alti_pg_file();
  ERS_INFO("ALTI PG file: " << (fname=="" ? "N/A" : fname));
  if (fname == "")
    return 0;
  uint64_t sum = 0;
  std::ifstream inf(fname.c_str(), std::ifstream::in);
  std::string line;
  while (std::getline(inf, line)) {
    auto mult = alti_pg_multiplicity(line);
    sum = sum + mult;
  }
  // convert to microseconds
  m_alti_pg_duration = sum * 25 / 1000;
  ERS_INFO("ALTI PG duration [BC]: " << sum);
  ERS_INFO("ALTI PG duration [ms]: " << m_alti_pg_duration/1000);
  return m_alti_pg_duration;
}

uint64_t nsw::NSWCalibRc::alti_pg_multiplicity(std::string line) {
  if (line.empty())
    return 0;
  if (line[0] == '#' || line[0] == '-')
    return 0;
  std::vector<std::string> pg_words = {};
  std::istringstream ss(line);
  while (!ss.eof()) {
    std::string buf;
    std::getline(ss, buf, ' ');
    if (buf != "")
      pg_words.push_back(buf);
  }
  if (pg_words.size() < pg_size) {
    std::string msg = "Cant understand the ALTI pg_file: line = " + line;
    nsw::NSWCalibIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw std::runtime_error(msg);
  }
  auto mult = pg_words[pg_mult];
  return std::stoull(mult);
}

void nsw::NSWCalibRc::alti_callback(ISCallbackInfo* isc) {
  ISInfoDynAny any;
  try {
    isc->value(any);
    std::vector<ISInfoDynAny> & counters
      = any.getAttributeValue< std::vector<ISInfoDynAny> >("counters");
    for (auto & counter : counters)
      if (counter.getAttributeValue<std::string>("name") == "L1A")
        ERS_LOG("L1A value = " << counter.getAttributeValue<uint32_t>("value")
                << ", reason = " << isc->reason());
  } catch (daq::is::Exception& ex) {
    ers::error(ex);
  }
}

void nsw::NSWCalibRc::alti_count() {
  //
  // Count all L1A
  //
  try {
    usleep(1e6);
    ERS_INFO("Pausing to collect L1A values from IS...");
    usleep(5e6);
    uint64_t l1as = 0;
    struct Wrapper : public ISInfoDynAny {};
    std::vector<Wrapper> anys;
    is_dictionary->getValues(alti_monitoring(), anys);
    for (auto & any: anys) {
      std::vector<ISInfoDynAny> & counters
        = any.getAttributeValue< std::vector<ISInfoDynAny> >("counters");
      for (auto & counter : counters) {
        if (counter.getAttributeValue<std::string>("name") == "L1A") {
          // ERS_INFO("L1A value = " << counter.getAttributeValue<uint32_t>("value"));
          l1as = l1as + counter.getAttributeValue<uint32_t>("value");
        }
      }
    }
    ERS_INFO("L1A sum, according to IS: " << l1as);
  } catch (daq::is::Exception& ex) {
    ers::error(ex);
  }
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
    if (m_calibType != "" && calibType != m_calibType) {
      std::string msg = "Found a new calibType. Was " + m_calibType + ", is now " + calibType;
      nsw::NSWCalibIssue issue(ERS_HERE, msg);
      ers::warning(issue);
    }
  } else {
    calibType = "MMARTConnectivityTest";
    nsw::NSWCalibIssue issue(ERS_HERE, "Calibration type not found in IS. Defaulting to: " + calibType);
    ers::warning(issue);
  }
  ISInfoDynAny runParams;
  is_dictionary->getValue("RunParams.RunParams", runParams);
  runParams.setAttributeValue<std::string>(4,"Calibration");
  runParams.setAttributeValue<std::string>(8,calibType);
  is_dictionary->update("RunParams.RunParams", runParams);
  return calibType;
}
