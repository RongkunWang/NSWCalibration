#include "NSWCalibration/NSWCalibRc.h"

#include <thread>

#include <RunControl/Common/OnlineServices.h>

#include <is/infoT.h>
#include <is/infostream.h>
#include <is/infodynany.h>

#include <ers/ers.h>

#include <logit_logger.h>

#include <fmt/core.h>

#include "NSWCalibration/Commands.h"
#include "NSWCalibration/Issues.h"
#include "NSWCalibration/Utility.h"
#include "NSWCalibrationDal/NSWCalibApplication.h"
#include "NSWCalibration/MMTriggerCalib.h"
#include "NSWCalibration/MMTPInputPhase.h"
#include "NSWCalibration/sTGCTriggerCalib.h"
#include "NSWCalibration/sTGCPadVMMTDSChannels.h"
#include "NSWCalibration/sTGCStripsTriggerCalib.h"
#include "NSWCalibration/sTGCSFEBToRouter.h"
#include "NSWCalibration/sTGCPadTriggerToSFEB.h"
#include "NSWCalibration/sTGCPadTriggerInputDelays.h"
#include "NSWCalibration/sTGCPadsControlPhase.h"
#include "NSWCalibration/sTGCPadsL1DDCFibers.h"
#include "NSWCalibration/sTGCPadsRocTds40Mhz.h"
#include "NSWCalibration/sTGCPadsHitRateL1a.h"
#include "NSWCalibration/sTGCPadsHitRateSca.h"
#include "NSWCalibration/sTGCPadTdsBcidOffset.h"
#include "NSWCalibration/THRCalib.h"
#include "NSWCalibration/PDOCalib.h"
#include "NSWCalibration/RocPhaseCalibrationBase.h"
#include "NSWCalibration/RocPhase40MhzCore.h"
#include "NSWCalibration/RocPhase160MhzCore.h"
#include "NSWCalibration/RocPhase160MhzVmm.h"
#include "NSWConfiguration/NSWConfig.h"

#include "NSWConfiguration/NSWConfig.h"

using boost::property_tree::ptree;

using namespace std::chrono_literals;

nsw::NSWCalibRc::NSWCalibRc(bool simulation):m_simulation {simulation} {
    ERS_LOG("Constructing NSWCalibRc instance");
    if (m_simulation) {
        ERS_INFO("Running in simulation mode, no configuration will be sent");
        m_simulation_lock = true;
    }
    Log::initializeLogging();
    initializeOpen62541LogIt(Log::ERR);

}

void nsw::NSWCalibRc::configure(const daq::rc::TransitionCmd&) {
    ERS_INFO("Start");

    daq::rc::OnlineServices& rcSvc = daq::rc::OnlineServices::instance();
    const daq::core::RunControlApplicationBase& rcBase = rcSvc.getApplication();
    m_nswApp = rcBase.cast<nsw::dal::NSWCalibApplication>();
    m_appname  = rcSvc.applicationName();
    m_is_db_name = m_nswApp->get_dbISName();

    // Retrieving the configuration db
    m_dbcon    = m_nswApp->get_dbConnection();
    m_resetVMM = m_nswApp->get_resetVMM();
    m_resetTDS = m_nswApp->get_resetTDS();

    ERS_INFO("App name: "  << m_appname);
    ERS_INFO("DB Config: " << m_dbcon);
    ERS_INFO("reset VMM: " << m_resetVMM);
    ERS_INFO("reset TDS: " << m_resetTDS);

    // Retrieve the ipc partition
    m_ipcpartition = rcSvc.getIPCPartition();

    // Get the IS dictionary for the current partition
    is_dictionary = std::make_unique<ISInfoDictionary>(m_ipcpartition);
    std::string g_calibration_type="";
    std::string g_info_server_name="";
    const std::string stateInfoName = g_info_server_name + ".CurrentCalibState";
    const std::string calibInfoName = g_info_server_name + "." + g_calibration_type + "CalibInfo";

    // Announce the current calibType, and
    // retrieve simulation status, and
    // publish metadata to IS (in progress)
    m_calibType = calibTypeFromIS();
    if (!m_simulation_lock)
      m_simulation = simulationFromIS();
    publish4swrod();

    m_NSWConfig = std::make_unique<NSWConfig>(m_simulation);
    m_NSWConfig->readConf(m_nswApp);
    m_NSWConfig->readConfigurationResource();

    ERS_LOG("End");
}

void nsw::NSWCalibRc::connect(const daq::rc::TransitionCmd&) {
    ERS_INFO("Start");

    // Announce the current calibType (again)
    m_calibType = calibTypeFromIS();

    // Sending the configuration to the HW
    m_NSWConfig->configureRc();

    handler();

    // End
    ERS_LOG("End");
}

void nsw::NSWCalibRc::prepareForRun(const daq::rc::TransitionCmd&) {
    ERS_LOG("Start");
    m_NSWConfig->startRc();
    ERS_LOG("End");
}

void nsw::NSWCalibRc::disconnect(const daq::rc::TransitionCmd&) {
    ERS_INFO("Start");
    m_NSWConfig->unconfigureRc();
    ERS_INFO("End");
}

void nsw::NSWCalibRc::unconfigure(const daq::rc::TransitionCmd&) {
    ERS_INFO("Start");
    ERS_INFO("End");
}

void nsw::NSWCalibRc::stopRecording(const daq::rc::TransitionCmd&) {
    ERS_LOG("Start");
    m_NSWConfig->stopRc();
    ERS_LOG("End");
}

void nsw::NSWCalibRc::user(const daq::rc::UserCmd& usrCmd) {
  ERS_LOG("User command received: " << usrCmd.commandName());
  if (usrCmd.commandName() == "enableVmmCaptureInputs") {
    m_NSWConfig->enableVmmCaptureInputs();
  } else if (usrCmd.commandName() == "configure") {
    publish4swrod();
    calib->progressbar();
    calib->setCalibKeyToIS(*is_dictionary);
    calib->configure();
  } else if (usrCmd.commandName() == "acquire") {
    calib->acquire();
  } else if (usrCmd.commandName() == "unconfigure") {
    calib->unconfigure();
    calib->next();
  } else if (usrCmd.commandName() == "reset") {
    calib->setCounter(0);
    calib->setCurrentRunParameters(runParamsFromIS());
  } else {
    nsw::NSWCalibIssue issue(ERS_HERE, fmt::format("Unrecognized UserCmd specified {}", usrCmd.commandName()));
    ers::warning(issue);
  }
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

void nsw::NSWCalibRc::publish() {
  ERS_LOG("Publishing information to IS");
  for (const auto& [key, commands] :
       calib->getAltiSequences().getCommands()) {
    for (const auto& command : commands) {
      if (std::find(std::cbegin(nsw::commands::availableCommands),
                    std::cend(nsw::commands::availableCommands),
                    command) == std::cend(nsw::commands::availableCommands)) {
        // FIXME: Proper error handling
        throw std::runtime_error(
          fmt::format("Received invalid command {}",
                      nsw::calib::utils::commandToString(command)));
      }
    }
    is_dictionary->checkin(
      key, ISInfoString(nsw::calib::utils::serializeCommands(commands)));
  }
  const std::string numIterations = "NswParams.Calib.numIterations";
  is_dictionary->checkin(numIterations, ISInfoUnsignedLong(calib->total()));
}

void nsw::NSWCalibRc::handler() {

  nsw::snooze();

  const auto& deviceManager = m_NSWConfig->getDeviceManager();

  // create calib object
  calib.reset();
  m_calibType = calibTypeFromIS();
  if (m_calibType=="MMARTConnectivityTest" ||
      m_calibType=="MMARTConnectivityTestAllChannels" ||
      m_calibType=="MMCableNoise" ||
      m_calibType=="MMARTPhase" ||
      m_calibType=="MML1ALatency" ||
      m_calibType=="MMStaircase") {
    calib = std::make_unique<MMTriggerCalib>(m_calibType, deviceManager);
  } else if (m_calibType=="MMTrackPulserTest") {
    calib = std::make_unique<MMTriggerCalib>(m_calibType, deviceManager);
  } else if (m_calibType=="MMTPInputPhase") {
    calib = std::make_unique<MMTPInputPhase>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadConnectivity" ||
             m_calibType=="sTGCPadLatency") {
    calib = std::make_unique<sTGCTriggerCalib>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadVMMTDSChannels") {
    calib = std::make_unique<sTGCPadVMMTDSChannels>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCSFEBToRouter"   ||
             m_calibType=="sTGCSFEBToRouterQ1" ||
             m_calibType=="sTGCSFEBToRouterQ2" ||
             m_calibType=="sTGCSFEBToRouterQ3") {
    calib = std::make_unique<sTGCSFEBToRouter>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadTriggerToSFEB") {
    calib = std::make_unique<sTGCPadTriggerToSFEB>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadTriggerInputDelays") {
    calib = std::make_unique<sTGCPadTriggerInputDelays>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCStripConnectivity") {
    calib = std::make_unique<sTGCStripsTriggerCalib>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadsControlPhase") {
    calib = std::make_unique<sTGCPadsControlPhase>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadsL1DDCFibers") {
    calib = std::make_unique<sTGCPadsL1DDCFibers>(m_calibType, deviceManager);
  } else if (m_calibType=="sTGCPadsRocTds40Mhz") {
    calib = std::make_unique<sTGCPadsRocTds40Mhz>(m_calibType, deviceManager);
  } else if (m_calibType == "sTGCPadsHitRateL1a") {
    calib = std::make_unique<sTGCPadsHitRateL1a>(m_calibType, deviceManager);
  } else if (m_calibType == "sTGCPadsHitRateSca") {
    calib = std::make_unique<sTGCPadsHitRateSca>(m_calibType, deviceManager);
  } else if (m_calibType == "sTGCPadTdsBcidOffset") {
    calib = std::make_unique<sTGCPadTdsBcidOffset>(m_calibType, deviceManager);
  } else if (m_calibType=="THRCalib"){
    calib = std::make_unique<THRCalib>(m_calibType, deviceManager);
  } else if (m_calibType=="PDOCalib" ||
             m_calibType=="TDOCalib"){
    calib = std::make_unique<PDOCalib>(m_calibType, deviceManager);
  } else if (m_calibType=="RocPhase40MHzCore") {
    calib = std::make_unique<RocPhaseCalibrationBase<RocPhase40MhzCore>>(m_calibType, deviceManager);
  } else if (m_calibType == "RocPhase160MHzCore") {
    calib = std::make_unique<RocPhaseCalibrationBase<RocPhase160MhzCore>>(m_calibType, deviceManager);
  } else if (m_calibType == "RocPhase160MHzVmm") {
    calib = std::make_unique<RocPhaseCalibrationBase<RocPhase160MhzVmm>>(m_calibType, deviceManager);
  } else {
    std::string msg = "Unknown calibration request: " + m_calibType;
    nsw::NSWCalibIssue issue(ERS_HERE, msg);
    ers::error(issue);
    throw std::runtime_error(msg);
  }

  // setup
  // FIXME: Alex will remove it in another MR
  calib->setApplicationName(m_appname);
  calib->setSimulation(m_simulation);
  calib->setCalibParamsFromIS(*is_dictionary, m_is_db_name);
  calib->setup(m_dbcon);
  ERS_INFO("calib counter:    " << calib->counter());
  ERS_INFO("calib total:      " << calib->total());
  ERS_INFO("calib wait4swrod: " << calib->wait4swrod());
  ERS_INFO("calib simulation: " << calib->simulation());
  ERS_INFO("calib run number: " << calib->runNumber());
}

void nsw::NSWCalibRc::publish4swrod() {
  //
  // Commented out for now.
  // This is powerful and risky.
  //
  // if (calib) {
  //   is_dictionary->checkin(m_calibCounter, ISInfoInt(calib->counter()));
  //   wait4swrod();
  // } else {
  //   is_dictionary->checkin(m_calibCounter, ISInfoInt(-1));
  // }
}

void nsw::NSWCalibRc::wait4swrod() {
  if (calib->wait4swrod()) {
    ISInfoInt counter(-1);
    ERS_INFO("calib waiting for swROD...");
    std::size_t           attempt_i    = 0;
    constexpr std::size_t attempts_max = 5;
    while (counter.getValue() != calib->counter()) {
      try {
        is_dictionary->getValue(m_calibCounter_readback, counter);
      } catch (daq::is::Exception& ex) {
        ers::error(ex);
      }
      ERS_INFO("calib waiting for swROD, attempt " << attempt_i);
      nsw::snooze();
      attempt_i++;
      if (attempt_i >= attempts_max)
        throw std::runtime_error("Waiting for swROD failed");
    }
  }
}

std::string nsw::NSWCalibRc::calibTypeFromIS() {
  // Grab the calibration type string from IS
  // Currently supported options are written in the `handler` function.
  const auto calibType = [this]() -> std::string {
    const auto paramIsName = fmt::format("{}.Calib.calibType", m_is_db_name);
    if (is_dictionary->contains(paramIsName)) {
      ISInfoDynAny calibTypeFromIS;
      is_dictionary->getValue(paramIsName, calibTypeFromIS);
      const auto calibType = calibTypeFromIS.getAttributeValue<std::string>(0);
      ERS_INFO("Calibration type from IS: " << calibType);
      if (m_calibType != "" && calibType != m_calibType) {
        nsw::NSWCalibIssue issue(
          ERS_HERE,
          fmt::format("Found a new calibType. Was {}, is now {}",
                      m_calibType,
                      calibType));
        ers::warning(issue);
      }
      return calibType;
    } else {
      const std::string calibType = "MMARTConnectivityTest";
      const auto is_cmd = fmt::format("is_write -p ${{TDAQ_PARTITION}} -n {} -t String -v <CalibrationType> -i 0", paramIsName);
      nsw::calib::IsParameterNotFoundUseDefault issue(ERS_HERE, "calibType", is_cmd, calibType);
      ers::warning(issue);
      return calibType;
    }
  }();

  ISInfoDynAny runParams;
  is_dictionary->getValue("RunParams.RunParams", runParams);
  runParams.setAttributeValue<std::string>(4, "Calibration");
  runParams.setAttributeValue<std::string>(8, calibType);
  is_dictionary->update("RunParams.RunParams", runParams);
  return calibType;
}

bool nsw::NSWCalibRc::simulationFromIS() {
  // Grab the simulation bool from IS
  // The OKS parameter dbISName determines the prefix, m_is_db_name
  const auto paramIsSim = fmt::format("{}.simulation", m_is_db_name);
  if (is_dictionary->contains(paramIsSim)) {
    ISInfoDynAny any;
    is_dictionary->getValue(paramIsSim, any);
    auto val = any.getAttributeValue<bool>(0);
    ERS_INFO("Simulation from IS: " << val);
    return val;
  }

  const auto is_cmd =
    fmt::format("is_write -p ${{TDAQ_PARTITION}} -n {} -t Boolean -v 1 -i 0", paramIsSim);
  ers::log(nsw::calib::IsParameterNotFound(ERS_HERE, "simulation", is_cmd));

  return false;
}

std::pair<std::uint32_t, std::time_t> nsw::NSWCalibRc::runParamsFromIS() {
  ISInfoDynAny runParams;
  is_dictionary->getValue("RunParams.RunParams", runParams);
  std::uint32_t runNumber{0};
  std::time_t runStartTime{};
  try {
    runNumber = runParams.getAttributeValue<std::uint32_t>("run_number");
  } catch(daq::is::Exception& ex) {
    ers::error(ex);
  }
  try {
    runStartTime = (runParams.getAttributeValue<OWLTime>("timeSOR")).c_time();
  } catch(daq::is::Exception& ex) {
    ers::error(ex);
  }

  return {runNumber, runStartTime};
}
