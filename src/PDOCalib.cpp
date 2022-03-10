#include "NSWCalibration/PDOCalib.h"

#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <iomanip>

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <is/info.h>
#include <is/infoT.h>
#include <is/infostream.h>
#include <is/infodynany.h>

#include <ers/ers.h>

#include <RunControl/RunControl.h>
#include <RunControl/Common/RunControlCommands.h>

// FIXME TODO only used for nsw::ref constant inclusion
#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/Issues.h"

#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

using namespace std::chrono_literals;

nsw::PDOCalib::PDOCalib(std::string calibType,
                        const hw::DeviceManager& deviceManager,
                        std::string calibIsName,
                        const ISInfoDictionary& calibIsDict) :
  CalibAlg(std::move(calibType), deviceManager),
  m_isDbName(std::move(calibIsName)),
  m_isInfoDict(calibIsDict),
  m_trecord(8000)
{
  m_pdo_flag = [&]() {
    if (m_calibType == "PDOCalib") {
      ERS_INFO("PDOCalib::configure::PDO Calibration run");
      return true;
    } else if (m_calibType == "TDOCalib") {
      ERS_INFO("PDOCalib::configure::TDO Calibration run");
      return false;
    } else {
      nsw::PDOCalibIssue issue(
        ERS_HERE,
        fmt::format("Invalid calibration specified, must be either PDOCalib or TDOCalib"));
      ers::fatal(issue);
      // FIXME TODO confirm we really should throw here
      throw issue;
      // throw std::runtime_error(msg.str());
    }
  }();
}

void nsw::PDOCalib::setup(const std::string& db)
{
  m_feconfigs = read_pulsing_config(db);

  ERS_INFO(fmt::format("Reading frontend configuration - {}", db));

  // needs input from IS entry - calibParams
  get_calib_params_IS(m_calibRegs, m_numChPerGroup);

  ERS_INFO("Checking parameter input from IS");
  ERS_INFO(fmt::format("Number of params to loop for = {})", m_calibRegs.size()));
  for (std::size_t i{0}; i < m_calibRegs.size(); i++) {
    ERS_INFO(fmt::format("par-{} = {}", i, m_calibRegs.at(i)));
  }

  // determining if group of even number of channels will be pulsed, otherwise -
  // pulse all

  const auto num_ch_groups = [this]() -> std::size_t {
    if (m_numChPerGroup != 1 &&
        m_numChPerGroup != 2 &&
        m_numChPerGroup != 4 &&
        m_numChPerGroup != 8) {
      // this option if ALL channels will be pulsed (first
      // entry in IS string is any odd int, not a factor of 8)
      ERS_INFO(fmt::format("Pulsing ALL channels, | m_numChPerGroup={}", m_numChPerGroup));
      return 1;
    } else {
      ERS_INFO(fmt::format("Pulsing groups of |{}| channels, number of channel iterations ({})",
                           m_numChPerGroup, nsw::vmm::NUM_CH_PER_VMM / m_numChPerGroup));
      return nsw::vmm::NUM_CH_PER_VMM / m_numChPerGroup;
    }
  }();

  // Calibration loops over each calib reg value, for each channel group.
  // Within each iteration, an ALTI interaction is made
  setTotal(num_ch_groups * m_calibRegs.size());

  // loop starts at the first channel (0), but resetCalibLoop increments the current channel
  m_currentChannel = static_cast<std::size_t>(-1);
  if (!m_calibRegs.empty()) {
    resetCalibLoop();
  }

  // FIXME TODO argument of this function is an std::size_t
  // (unsigned), what is `-1` being passed for?
  // updating triggerCalibrationKey and after n sec sleep
  // reading it back from swrod
  push_to_swrod(static_cast<std::size_t>(-1));
}

void nsw::PDOCalib::configure()
{
  const auto start = std::chrono::high_resolution_clock::now();

  if (m_pdo_flag) {
    ERS_INFO(fmt::format("Calibrating PDO with pulser DAC = {}", m_currentCalibReg));
  } else {
    ERS_INFO(fmt::format("Calibrating TDO with delay = {} [ns]", m_currentCalibReg * nsw::ref::TP_DELAY_STEP));
  }
  push_to_swrod(m_currentCalibReg);  // updating triggerCalibrationKey and after n sec
  // sleep reading it back from swrod

  ERS_INFO("Unmasking channels to be pulsed, setting registers");
  send_pulsing_configs(m_currentCalibReg, m_currentChannel, true);
}

void nsw::PDOCalib::acquire()
{
  // Time to acquire data during each iteration
  ERS_INFO(fmt::format("Recording data for {} [ms]", m_trecord));
  // FIXME TODO timing? Can this be passed to the StartPG arguments?
  std::this_thread::sleep_for(m_trecord);
}

void nsw::PDOCalib::unconfigure()
{
  ERS_INFO("Unconfiguring");
  ERS_INFO(fmt::format("Done with parameter {}", m_currentCalibReg));

  // FIXME TODO timing?
  // waiting for all the data to be transferred & l1a to be sent
  std::this_thread::sleep_for(2000ms);

  // FIXME TODO only for debug remove?
  // check_roc();
  // std::this_thread::sleep_for(1000ms);

  const auto chanIterStop = std::chrono::high_resolution_clock::now();
  const auto chanElapsed{chanIterStop - m_chanIterStart};
  ERS_INFO(fmt::format("Done with channel [[{}" "]], took t={:%M:%S} [min]", m_currentChannel, chanElapsed));

  // If we're at the end of the register loop, reset and increment the channel
  if (m_loopCalibRegs.empty()) {
    resetCalibLoop();
  }

  // FIXME TODO if we have reached the end of the calibration...?
  // if (m_currentChannel == num_ch_groups) {
  //   const auto calibFinish = std::chrono::high_resolution_clock::now();
  //   const auto totalElapsed{calibStop - m_calibStart};
  //   ERS_INFO(fmt::format("Calibration complete, total run time was t={:%M:%S} [min]", totalElapsed));
  // }
}

nsw::commands::Commands nsw::PDOCalib::getAltiSequences() const
{
  return {
    {},  // before configure
    {nsw::commands::actionSR,
     nsw::commands::actionBCR,
     nsw::commands::actionECR,
     nsw::commands::actionStartPG},  // during (before acquire)
    {nsw::commands::actionStopPG}    // after (before unconfigure)
  };
}
void nsw::PDOCalib::resetCalibLoop()
{
  m_chanIterStart = std::chrono::high_resolution_clock::now();
  ERS_INFO(fmt::format("Iterating over channel << {}>>", m_currentChannel));
  // Move to next channel
  m_currentChannel += 1;
  // make a copy, because in the loop we remove the entries as we go along.
  m_loopCalibRegs = m_calibRegs;
  // Perform the calibration loop in reverse
  m_currentCalibReg = m_loopCalibRegs.back();
  // Remove the element we start with
  m_loopCalibRegs.pop_back();
  // masking all channels here to be sure we pulse only what we need
  send_pulsing_configs(0, m_currentChannel, false);
}

std::vector<nsw::FEBConfig> nsw::PDOCalib::read_pulsing_config(const std::string& config_filename)
{
  ERS_INFO(fmt::format("Reading configuration file {}", config_filename));
  nsw::ConfigReader reader(config_filename);

  try {
    auto config = reader.readConfig();
  } catch (const std::exception& e) {
    nsw::PDOCalibIssue issue(ERS_HERE, fmt::format("Configuration file has a problem: {}", e.what()));
    ers::fatal(issue);
  }

  for (const auto& name : reader.getAllElementNames()) {
    try {
      m_feconfigs.emplace_back(reader.readConfig(name));
    } catch (const std::exception& e) {
      nsw::PDOCalibIssue issue(ERS_HERE, fmt::format("Configuration construction failed due to: {}", e.what()));
      ers::fatal(issue);
    }
  }
  ERS_INFO(fmt::format("-> [ {} ] will be pulsed", m_feconfigs.size()));
  return m_feconfigs;
}

void nsw::PDOCalib::send_pulsing_configs(const std::size_t i_par,
                                         const std::size_t first_chan,
                                         const bool toggle)
{
  for (auto& feb : m_feconfigs) {
    m_conf_threads.push_back(std::thread(
      &nsw::PDOCalib::toggle_channels, this, feb, first_chan, i_par, toggle));
  }

  std::this_thread::sleep_for(2000ms);

  for (auto& thrd : m_conf_threads) {
    thrd.join();
  }

  m_conf_threads.clear();
}

void nsw::PDOCalib::toggle_channels(nsw::FEBConfig feb,
                                    const std::size_t first_chan,
                                    const std::size_t i_par,
                                    const bool toggle)
{
  nsw::ConfigSender cs;

  auto& roc_analog = feb.getRocAnalog();
  auto& roc_digital = feb.getRocDigital();
  auto& vmms = feb.getVmms();

  const std::string febNode = feb.getAddress();
  const std::string febIp = feb.getOpcServerIp();

  const auto [n_vmms, firstVmm] = [&]() -> std::pair<std::size_t, std::size_t> {
    if (vmms.size() == nsw::ref::NUM_VMM_SFEB6) {
      return {nsw::NUM_VMM_PER_SFEB, nsw::SFEB6_FIRST_VMM};
    }
    return {vmms.size(), 0};
  }();

  for (std::size_t conn_try{1}; conn_try <= nsw::MAX_ATTEMPTS; conn_try++) {
    if (cs.readSCAOnline(feb)) {
      ERS_DEBUG(2, febNode << " is online");
      break;
    }

    nsw::PDOCalibIssue issue(ERS_HERE, fmt::format("Waiting 5 sec to reconnect with {}, (attempt {}/3)", febNode, conn_try));
    ers::warning(issue);
    std::this_thread::sleep_for(5000ms);

    if (conn_try == nsw::MAX_ATTEMPTS) {
      nsw::PDOCalibIssue issue(ERS_HERE, fmt::format("Lost connection to frontend - {}", febNode));
      ers::error(issue);
    }
  }

  for (auto& vmm : vmms) {
    vmm.setChannelRegisterAllChannels("channel_st", 0);
    vmm.setChannelRegisterAllChannels("channel_sm", 1);
  }

  if (toggle) {
    auto enable_vmm_channels = [&](nsw::VMMConfig& vmm,
                                   // const std::size_t vmm_id,
                                   const std::vector<std::size_t>& offsets) -> void {
      for (const auto& ofs : offsets) {
        vmm.setChannelRegisterOneChannel("channel_st", 1, first_chan + ofs);
        vmm.setChannelRegisterOneChannel("channel_sm", 0, first_chan + ofs);
      }
    };

    for (auto& vmm : vmms) {
      if (m_pdo_flag) {
        vmm.setTestPulseDAC(i_par);

        // FIXME TODO or this should be part of the previous block?
        // vmm.setTestPulseDAC(static_cast<size_t>(380));
        // vmm.setGlobalRegister("stc", 0);  // TAC slope 60 ns
        // vmm.setGlobalRegister("st", 3);   // peak time 25 ns
      }

      if (m_numChPerGroup == 1) {
        enable_vmm_channels(vmm, {0});
      } else if (m_numChPerGroup == 2) {
        enable_vmm_channels(vmm, {0, 32});
      } else if (m_numChPerGroup == 4) {
        enable_vmm_channels(vmm, {0, 16, 32, 48});
      } else if (m_numChPerGroup == 8) {
        enable_vmm_channels(vmm, {0, 8, 16, 24, 32, 40, 48, 56});
      } else {
        vmm.setChannelRegisterAllChannels("channel_st", 1);
        vmm.setChannelRegisterAllChannels("channel_sm", 0);
      }
    }

    // FIXME TODO this should be outside the vmm loop?
    if (m_pdo_flag) {
      // delaying vmms 0 to 3
      roc_analog.setRegisterValue("reg073ePllVmm0", "tp_phase_0", static_cast<std::uint32_t>(i_par));
      roc_analog.setRegisterValue("reg073ePllVmm0", "tp_phase_1", static_cast<std::uint32_t>(i_par));
      roc_analog.setRegisterValue("reg074ePllVmm0", "tp_phase_2", static_cast<std::uint32_t>(i_par));
      roc_analog.setRegisterValue("reg074ePllVmm0", "tp_phase_3", static_cast<std::uint32_t>(i_par));
      // delaying vmms 4 to 7
      roc_analog.setRegisterValue("reg089ePllVmm1", "tp_phase_0", static_cast<std::uint32_t>(i_par));
      roc_analog.setRegisterValue("reg089ePllVmm1", "tp_phase_1", static_cast<std::uint32_t>(i_par));
      roc_analog.setRegisterValue("reg090ePllVmm1", "tp_phase_2", static_cast<std::uint32_t>(i_par));
      roc_analog.setRegisterValue("reg090ePllVmm1", "tp_phase_3", static_cast<std::uint32_t>(i_par));

      // global delay of tp clock
      // roc_analog.setRegisterValue("reg119","tp_phase_global",i_par)
    }
  }

  for (std::size_t itry{1}; itry <= nsw::MAX_ATTEMPTS; itry++) {
    try {
      cs.sendRocConfig(febIp, febNode, roc_analog, roc_digital);
      cs.disableVmmCaptureInputs(feb);

      std::vector<unsigned> reset_orig;

      for (auto& vmm : vmms) {
        reset_orig.push_back(vmm.getGlobalRegister("reset"));
        vmm.setGlobalRegister("reset", 3);
      }

      cs.sendVmmConfig(feb);
      std::this_thread::sleep_for(1000ms);

      { // FIXME with c++20 control statement initializers: scope for i
        std::size_t i{0};
        for (auto& vmm : vmms) {
          vmm.setGlobalRegister("reset", reset_orig.at(i++));
        }
      }

      cs.sendVmmConfig(feb);
      std::this_thread::sleep_for(1000ms);

      cs.enableVmmCaptureInputs(feb);
    } catch (const std::exception& e) {
      ers::warning(
        nsw::PDOCalibIssue(
          ERS_HERE,
          fmt::format("Can not reconfigure front-end: {}\nFront-end status (SCA replies) = {}",
                      e.what(),
                      cs.readSCAOnline(feb))));

      std::this_thread::sleep_for(5000ms);

      if (itry == nsw::MAX_ATTEMPTS) {
        nsw::PDOCalibIssue issue(
          ERS_HERE,
          fmt::format("Could not configure {} {} consecutive times.\n Front-end status: {}",
                      febNode,
                      nsw::MAX_ATTEMPTS,
                      cs.readSCAOnline(feb)));
        ers::fatal(issue);
        // FIXME throw?
        // throw issue;
      }
    }
    break;
  }
}

void nsw::PDOCalib::push_to_swrod(const std::size_t i_par)
{
  m_isInfoDict.checkin(m_calibCounterTck, ISInfoInt(i_par));

  ERS_INFO("Publishing following parameter to swrod plugin >>" << i_par);

  std::this_thread::sleep_for(2000ms);
  ISInfoInt swrod_trigger_par;
  try {
    m_isInfoDict.getValue(m_calibCounterTck_readback, swrod_trigger_par);
    ERS_INFO("Reading back updated triggerCalibrationKey is >> " << swrod_trigger_par);
  } catch (daq::is::Exception& ex) {
    ers::error(ex);
  }
}

void nsw::PDOCalib::get_calib_params_IS(std::vector<int>& reg_values, int& group)
{
  const auto calibParamsIsName = fmt::format("{}.Calib.calibParams", m_isDbName);
  if (m_isInfoDict.contains(calibParamsIsName)) {
    ISInfoDynAny calibParamsFromIS;
    m_isInfoDict.getValue(calibParamsIsName, calibParamsFromIS);
    const std::string params = calibParamsFromIS.getAttributeValue<std::string>(0);

    // channel groups to pulse come as first arg in the string
    const auto gr = params.substr(0, 1);
    const auto regs = params.substr(2, params.length());
    group = std::stoi(gr);
    ERS_INFO(fmt::format("Found entries {} CHAN-GR= [{}] REGS= [{}]", params, gr, regs));
    std::stringstream instream(regs);

    // Parse the remainder of the input to extract time and DAC/delay values
    int iter = 0;
    while (instream.good()) {
      std::string sub;
      getline(instream, sub, ',');
      if (sub.at(0) == '*' && sub.at(sub.length() - 1) == '*') {
        // time setting is denoted in IS as *####* ms
        std::string time;
        for (std::size_t c = 0; c < sub.length(); c++) {
          if (sub[c] != '*') {
            time += sub[c];
          } else {
            continue;
          }
        }
        m_trecord = std::chrono::milliseconds(std::stoi(time));
        ERS_INFO(fmt::format("Found new recording time => {} [ms]", m_trecord));
      } else {
        // if sub is not starting/ending with * - it's a pulser
        // DAC/delay setting
        reg_values.push_back(std::atoi(sub.c_str()));
      }
      iter++;
    }
  } else {
    // in case calibParams does not exist, the following settings are used
    m_trecord = std::chrono::milliseconds(8000);
    group = 8;

    const auto msg = [this, &reg_values]() -> std::string {
      if (m_pdo_flag) {
        reg_values = {100, 150, 200, 250, 300, 350, 400, 450};
        return "PDO Calibration parameters were not specified - using default "
               "settings - looping from 100 to 600 Pulser DAC in 100 DAC steps, "
               "pulsing 8 channel groups";
      } else {
        reg_values = {0, 1, 2, 3, 4, 5, 6, 7};
        return "TDO Calibration parameters were not specified - using default "
               "settings - looping through delays 1,3,5,7 - pulsing 8 channel groups";
      }
    }();

    const auto is_cmd = fmt::format(
      "is_write -p ${{TDAQ_PARTITION}} -n {} -t String  -v '<params>' -i 0", calibParamsIsName);
    ers::warning(nsw::calib::IsParameterNotFound(ERS_HERE, "calibParams", is_cmd));
    nsw::PDOCalibIssue issue(ERS_HERE, msg);
    ers::warning(issue);
  }
}

void nsw::PDOCalib::check_roc()
{
  nsw::ConfigSender cs;
  const auto& feb = m_feconfigs.front();

  // FIXME TODO not currently DRY...
  const auto [n_vmms, firstVmm] = [&]() -> std::pair<std::size_t, std::size_t> {
    if (feb.getVmms().size() == nsw::ref::NUM_VMM_SFEB6) {
      return {nsw::NUM_VMM_PER_SFEB, nsw::SFEB6_FIRST_VMM};
    }
    return {feb.getVmms().size(), 0};
  }();

  const auto febname = feb.getAddress();
  const auto opcIp = feb.getOpcServerIp();

  const auto roc_lock1 = cs.readGPIO(opcIp, febname + ".gpio.rocPllLocked");
  const auto roc_lock2 = cs.readGPIO(opcIp, febname + ".gpio.rocPllRocLocked");

  ERS_INFO(fmt::format("L1P3_IPR - rocPllLocked[{}] - rocPllRocLocked[{}]",
                       static_cast<std::uint32_t>(roc_lock1),
                       static_cast<std::uint32_t>(roc_lock2)));

  for (std::size_t v{firstVmm}; v < n_vmms; ++v) {
    const auto rocDigiReg_32 =
      cs.readBackRocDigital(opcIp, febname + ".gpio.bitBanger", static_cast<uint8_t>(32 + v));
    const auto r32_bits = std::bitset<8>(rocDigiReg_32).to_string();
    ERS_INFO(fmt::format("L1P3_IPR - VMM-{}-Capture errors: {}", v, r32_bits));
  }

  const auto rocDigiReg_sroc =
    cs.readBackRocDigital(opcIp, febname + ".gpio.bitBanger", static_cast<uint8_t>(40 + 0));
  const auto sroc_bits = std::bitset<8>(rocDigiReg_sroc).to_string();
  ERS_INFO(fmt::format("L1P3_IPR sROC status: {}", sroc_bits));

  const auto rocDigiReg_vmmCaptureStat =
    cs.readBackRocDigital(opcIp, febname + ".gpio.bitBanger", static_cast<uint8_t>(63));
  const auto vcsbits = std::bitset<8>(rocDigiReg_vmmCaptureStat).to_string();
  ERS_INFO(fmt::format("L1P3_IPR - VMM timeout status(1-bit per VMM)-{}: ", vcsbits));

  const auto rocDigiReg_SEU =
    cs.readBackRocDigital(opcIp, febname + ".gpio.bitBanger", static_cast<uint8_t>(53));
  const auto seubits = std::bitset<8>(rocDigiReg_SEU).to_string();
  ERS_INFO(fmt::format("L1P3_IPR - VMM SEU Counter -: {}", seubits));

  const auto rocDigiReg_SEUcore =
    cs.readBackRocDigital(opcIp, febname + ".gpio.bitBanger", static_cast<uint8_t>(44));
  const auto seuCorebits = std::bitset<8>(rocDigiReg_SEUcore).to_string();
  ERS_INFO(fmt::format("L1P3_IPR - VMM SEU core register -: {}", seuCorebits));
}
