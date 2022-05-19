#include "NSWCalibration/PDOCalib.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

#include <ers/ers.h>

#include <is/infodynany.h>
#include <is/infodictionary.h>

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/Issues.h"

#include "NSWConfiguration/Constants.h"

using namespace std::chrono_literals;

nsw::PDOCalib::PDOCalib(std::string calibType, const hw::DeviceManager& deviceManager) :
  CalibAlg(std::move(calibType), deviceManager),
  m_febs{getDeviceManager().getFebs()},
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
      throw issue;
    }
  }();
}

void nsw::PDOCalib::setup(const std::string& /*db*/)
{
  // needs input from IS entry - calibParams

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
      ERS_INFO(fmt::format("Pulsing groups of {} channels, number of channel iterations {}",
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
}

void nsw::PDOCalib::configure()
{
  // const auto start = std::chrono::high_resolution_clock::now();

  if (m_pdo_flag) {
    ERS_INFO(fmt::format("Calibrating PDO with pulser DAC = {}", m_currentCalibReg));
  } else {
    ERS_INFO(fmt::format("Calibrating TDO with delay = {} [ns]", static_cast<float>(m_currentCalibReg) * nsw::ref::TP_DELAY_STEP));
  }

  ERS_INFO("Unmasking channels to be pulsed, setting registers");
  send_pulsing_configs(m_currentCalibReg, m_currentChannel, true);
}

void nsw::PDOCalib::acquire()
{
  // Time to acquire data during each iteration
  ERS_INFO(fmt::format("Recording data for {}", m_trecord));
  std::this_thread::sleep_for(m_trecord);
}

void nsw::PDOCalib::unconfigure()
{
  ERS_INFO("Unconfiguring");
  ERS_INFO(fmt::format("Done with parameter {}", m_currentCalibReg));

  // waiting for all the data to be transferred & l1a to be sent
  std::this_thread::sleep_for(2000ms);

  const auto chanIterStop = std::chrono::high_resolution_clock::now();
  const auto chanElapsed{chanIterStop - m_chanIterStart};
  ERS_INFO(fmt::format("Done with channel {} in {:%M:%S} min", m_currentChannel, chanElapsed));

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
  ERS_INFO(fmt::format("Iterating over channel {}", m_currentChannel));
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

void nsw::PDOCalib::send_pulsing_configs(const std::size_t i_par,
                                         const std::size_t first_chan,
                                         const bool toggle)
{
  for (const auto& feb : m_febs.get()) {
    m_conf_threads.emplace_back([this, &feb, first_chan, i_par, toggle] () {
      nsw::PDOCalib::toggle_channels(feb, first_chan, i_par, toggle);
    });
  }

  std::this_thread::sleep_for(2000ms);

  for (auto& thrd : m_conf_threads) {
    thrd.join();
  }

  m_conf_threads.clear();
}

void nsw::PDOCalib::toggle_channels(const nsw::hw::FEB& feb,
                                    const std::size_t first_chan,
                                    const std::size_t i_par,
                                    const bool toggle)
{
  // Get the VMM device objects
  const auto& vmms = feb.getVmms();

  // Get the ROC device object
  const auto& roc = feb.getRoc();

  if (toggle) {
    if (m_pdo_flag) {
      roc.writeValues({
        // delaying vmms 0 to 3
        {"ePllVmm0.tp_phase_0", static_cast<std::uint32_t>(i_par)},
        {"ePllVmm0.tp_phase_1", static_cast<std::uint32_t>(i_par)},
        {"ePllVmm0.tp_phase_2", static_cast<std::uint32_t>(i_par)},
        {"ePllVmm0.tp_phase_3", static_cast<std::uint32_t>(i_par)},
        // delaying vmms 4 to 7
        {"ePllVmm1.tp_phase_0", static_cast<std::uint32_t>(i_par)},
        {"ePllVmm1.tp_phase_1", static_cast<std::uint32_t>(i_par)},
        {"ePllVmm1.tp_phase_2", static_cast<std::uint32_t>(i_par)},
        {"ePllVmm1.tp_phase_3", static_cast<std::uint32_t>(i_par)},
        // global delay of tp clock
      // {"reg119.tp_phase_global", static_cast<std::uint32_t>(i_par)},
      });
    }
  }

  // Ensure no garbage comes out of the ROC while resetting the VMM configuration
  roc.disableVmmCaptureInputs();

  // Do the modification of the VMM configuration
  auto enable_vmm_channels = [&](nsw::VMMConfig& vmm,
                                 const std::vector<std::uint32_t>& offsets) -> void {
    for (const auto& ofs : offsets) {
      vmm.setChannelRegisterOneChannel("channel_st", 1, static_cast<std::uint32_t>(first_chan) + ofs);
      vmm.setChannelRegisterOneChannel("channel_sm", 0, static_cast<std::uint32_t>(first_chan) + ofs);
    }
  };

  for (const auto& vmmHwi : vmms) {
    auto vmm = vmmHwi.getConfig();
    vmm.setChannelRegisterAllChannels("channel_st", 0);
    vmm.setChannelRegisterAllChannels("channel_sm", 1);

    if (toggle) {
      if (m_pdo_flag) {
        vmm.setTestPulseDAC(static_cast<std::uint32_t>(i_par));

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

    try {
      const auto reset_orig = vmm.getGlobalRegister("reset");
      vmm.setGlobalRegister("reset", 3);
      vmmHwi.writeConfiguration(vmm);

      std::this_thread::sleep_for(1000ms);

      vmm.setGlobalRegister("reset", reset_orig);
      vmmHwi.writeConfiguration(vmm);

      std::this_thread::sleep_for(1000ms);
    } catch (const std::exception& e) {
      ers::warning(
        nsw::PDOCalibIssue(
          ERS_HERE,
          fmt::format("Can not reconfigure front-end: {}",
                      e.what())));
    }
  }

  // Ensure data properly comes out of the ROC
  roc.enableVmmCaptureInputs();
}

void nsw::PDOCalib::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                         const std::string& is_db_name)
{
  const auto calib_param_is_name{fmt::format("{}.Calib.calibParams", is_db_name)};

  try {
    if (not is_dictionary.contains(calib_param_is_name)) {
      throw nsw::PDOParameterIssue(ERS_HERE, fmt::format("Unable to find {} in IS", calib_param_is_name));
    }

    ISInfoDynAny calib_params_from_is;
    is_dictionary.getValue(calib_param_is_name, calib_params_from_is);
    const auto calibParams = calib_params_from_is.getAttributeValue<std::string>(0);
    ERS_INFO(fmt::format("Calibration parameters from IS: {}", calibParams));

    const auto runParams = parseCalibParams(calibParams);

    m_trecord = runParams.trecord;
    m_numChPerGroup = runParams.channels;
    m_calibRegs = std::move(runParams.values);

  } catch (const nsw::PDOParameterIssue& is) {
    std::vector<std::size_t> reg_values{};

    const auto msg = [this, &reg_values]() -> std::string {
      if (m_pdo_flag) {
        reg_values = {100, 150, 200, 250, 300, 350, 400, 450};
        return fmt::format("PDO calibration parameters were not specified - using default "
                           "settings - looping over {} DAC values, "
                           "pulsing {} channel groups",
                           reg_values,
                           m_numChPerGroup);
      } else {
        reg_values = {0, 1, 2, 3, 4, 5, 6, 7};
        return fmt::format("TDO calibration parameters were not specified - using default "
                           "settings - looping through delays {} - pulsing {} channel groups",
                           reg_values,
                           m_numChPerGroup);
      }
    }();

    // In case calibParams does not exist, the following default settings are used
    m_trecord = DEFAULT_TRECORD;
    m_numChPerGroup = DEFAULT_NUM_CH_PER_GROUP;
    m_calibRegs = std::move(reg_values);

    nsw::PDOCalibIssue issue(ERS_HERE, msg);
    ers::warning(is);
    ers::warning(issue);
  }

}

nsw::PDOCalib::RunParameters nsw::PDOCalib::parseCalibParams(const std::string& calibParams)
{
  const auto tokens{nsw::tokenizeString(calibParams, ",")};
  // Channel groups to pulse come as first arg in the string
  const auto gr{tokens.front()};
  // The remainder of the input correspond to DAC/delay values and time
  const auto regs{std::ranges::subrange(tokens.cbegin()+1, tokens.cend())};
  ERS_DEBUG(2,fmt::format("Found calibration parameters {}: Chanel groups=[{}], registers=[{}]", calibParams, gr, regs));

  nsw::PDOCalib::RunParameters run_params{};
  try {
    run_params.channels = std::stoull(gr);
  } catch (const std::exception& ex) {
    run_params.channels = DEFAULT_NUM_CH_PER_GROUP;
    ERS_LOG(fmt::format("Unable to parse channel group parameter {}, using default {}: {}", gr, DEFAULT_NUM_CH_PER_GROUP, ex.what()));
  }

  for (const auto& sub : regs) {
    if (sub.empty()) {
      // TODO strange case, possible? print warning?
      continue;
    }

    // Time setting is denoted in IS as *####* ms
    if (sub.front() == '*' and sub.back() == '*') {
      const std::size_t NUM_MARKERS{2};
      if (sub.length() > NUM_MARKERS) {
        // Parsed '*#*'
        const auto time{sub.substr(1, sub.length()-NUM_MARKERS)};
        try {
          const auto ptime = std::stoi(time);
          if (ptime > 0) {
            run_params.trecord = std::chrono::milliseconds(ptime);
            ERS_LOG(fmt::format("Found new recording time => {}", run_params.trecord));
          } else {
            run_params.trecord = DEFAULT_TRECORD;
            ERS_LOG(fmt::format("Parsed negative time parameter {}, using default {}", time, DEFAULT_TRECORD));
          }
        } catch (const std::out_of_range& ex) {
          run_params.trecord = DEFAULT_TRECORD;
          ERS_LOG(fmt::format("Unable to parse time parameter {}, using default {}: {}", time, DEFAULT_TRECORD, ex.what()));
        } catch (const std::invalid_argument& ex) {
          run_params.trecord = DEFAULT_TRECORD;
          ERS_LOG(fmt::format("Unable to parse time parameter {}, using default {}: {}", time, DEFAULT_TRECORD, ex.what()));
        }
      } else {
        // Just parsed '**''
        run_params.trecord = DEFAULT_TRECORD;
        ERS_LOG(fmt::format("Unable to parse time parameter {}, using default {}", sub, DEFAULT_TRECORD));
      }
    } else if (sub.front() == '*' or sub.back() == '*') {
      // Just parsed '*', or '*12345' or '12345*'
      run_params.trecord = DEFAULT_TRECORD;
      ERS_LOG(fmt::format("Unable to parse time parameter {}, using default {}", sub, DEFAULT_TRECORD));
    } else {
      // If not starting/ending with *, extract the pulser DAC/delay setting
      try {
        run_params.values.push_back(std::stoull(sub));
      } catch (const std::out_of_range& ex) {
        ERS_LOG(fmt::format("{} is out of range for {}", sub, ex.what()));
      } catch (const std::invalid_argument& ex) {
        ERS_LOG(fmt::format("Unable to parse parameter {}: {}", sub, ex.what()));
      }
    }
  }
  return run_params;
}
