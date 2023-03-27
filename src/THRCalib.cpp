#include "NSWCalibration/THRCalib.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
// #include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <ers/ers.h>

#include <is/infodynany.h>
#include <is/infodictionary.h>

#include <RunControl/Common/OnlineServices.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/Constants.h"

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/CalibTypes.h"
#include "NSWCalibration/Issues.h"
#include "NSWCalibration/Utility.h"

#include "NSWCalibration/VmmTrimmerScaCalibration.h"
#include "NSWCalibration/VmmThresholdScaCalibration.h"
#include "NSWCalibration/VmmBaselineScaCalibration.h"
#include "NSWCalibration/VmmBaselineThresholdScaCalibration.h"

namespace pt = boost::property_tree;
namespace fs = std::filesystem;

using namespace std::chrono_literals;

nsw::THRCalib::THRCalib(std::string calibType, const hw::DeviceManager& deviceManager) :
  CalibAlg(std::move(calibType), deviceManager),
  m_febs{getDeviceManager().getFebs()}
{}

void nsw::THRCalib::setup(const std::string& db)
{
  m_configFile = db;

  auto& runCtlOnlServ = daq::rc::OnlineServices::instance();

  m_app_name = runCtlOnlServ.applicationName();

  const auto tokens = [](std::string str, const char token = '-') {
    std::replace(str.begin(), str.end(), token, ' ');
    std::vector<std::string> toks;
    std::stringstream ss(str);
    std::string tok;
    while (ss >> tok) {
      toks.push_back(tok);
    }
    return toks;
  }(m_app_name);

  const auto [wheel, sector] = [this, &tokens]() -> std::pair<std::string, std::string> {
    if (tokens.size() == 3) {
      // "VS-C14-Config"
      // "BB5-A14-Config"
      return {tokens.at(1).substr(0, 1), tokens.at(1).substr(1, 2)};
    } else if (tokens.size() == 4) {
      // "191A-A05-MM-Config"
      // "191C-C13-MM-Config"
      // "191A-C14-sTGC-Config"
      return {tokens.at(1).substr(0, 1), tokens.at(1).substr(1, 2)};
    } else if (tokens.size() == 5) {
      // "NSW-MMG-EA-S01-CalibApplication"
      // "NSW-STG-EC-S14-CalibApplication"
      return {tokens.at(2).substr(1, 1), tokens.at(3).substr(1, 2)};
    } else {
      // "VS-Config"
      ers::warning(nsw::THRCalibIssue(
        ERS_HERE,
        fmt::format("Could not parse application name {}. Did not match known P1, VS, BB5, or 191 "
                    "naming definition! "
                    "Default side and sector information will be used, and may not be correct.",
                    m_app_name)));
      return {std::string("A"), std::string("01")};
    }
  }();

  if (wheel == "A") {
    m_wheel = 1;
  } else {
    m_wheel = -1;
  }
  m_sector = std::stoull(sector);

  ERS_DEBUG(2,fmt::format("Calibration Application name is {} |Wheel={}|sector={}|", m_app_name, m_wheel, m_sector));

  ERS_INFO(fmt::format("Threshold calibration type - {}", m_run_type));
}

void nsw::THRCalib::configure()
{
  // FIXME TODO move this to a generic location that occurs after
  // setCurrentRunParameters, since output locations should *only* be
  // updated when the run number is obtained.
  // At this point (`configure`) it works, but only as long as this
  // calibration does not change to have multiple iterations.
  m_output_path = getOutputDir().string();
  ERS_INFO(fmt::format("Calibration data will be written to: {}", m_output_path));
  fs::create_directories(fs::path(m_output_path));

  if (m_run_type == "baselines") {
    launch_feb_calibration<nsw::VmmBaselineScaCalibration>();
    std::this_thread::sleep_for(2000ms);
  } else if (m_run_type == "baselines_thresholds") {
    launch_feb_calibration<nsw::VmmBaselineThresholdScaCalibration>();
    std::this_thread::sleep_for(2000ms);
  } else if (m_run_type == "read_thresholds") {
    launch_feb_calibration<nsw::VmmThresholdScaCalibration>();
    std::this_thread::sleep_for(2000ms);
  } else if (m_run_type == "thresholds") {
    launch_feb_calibration<nsw::VmmTrimmerScaCalibration>();
    std::this_thread::sleep_for(2000ms);
    merge_json();
  } else {
    nsw::THRParameterIssue issue(ERS_HERE, fmt::format("Run type {} was not recognised. No THR calibration will be run.", m_run_type));
    ers::error(issue);
  }
  ERS_INFO(fmt::format("{} done!", m_run_type));
}

void nsw::THRCalib::merge_json()
{
  ERS_INFO("Merging generated common configuration trees");

  ERS_LOG(fmt::format("Run number string = {}", m_run_string));

  const auto in_files = [this]() {
    std::vector<std::string> tmp_files{};
    const fs::path dir{m_output_path};
    if (fs::exists(dir)) {
      for (auto const& ent : fs::directory_iterator{dir}) {
        const std::string file_n = ent.path().filename();
        if (file_n.find("partial_config") != std::string::npos) {
          tmp_files.push_back(file_n);
          ERS_LOG(fmt::format("Found partial config [ {} ]", file_n));
        }
      }
    }

    std::sort(std::begin(tmp_files), std::end(tmp_files));

    return tmp_files;
  }();

  ERS_LOG(fmt::format("JSON directory has [{}] files, found: {}", in_files.size(), m_configFile));

  ERS_DEBUG(2, "Reading initial config file into ptree");
  pt::ptree prev_conf = [this]() {
    try {
      nsw::ConfigReader reader(m_configFile);
      return reader.readConfig();
    } catch (const std::exception& e) {
      nsw::THRCalibIssue issue(ERS_HERE, fmt::format("Can't read initial config file due to: {}", e.what()));
      ers::fatal(issue);
      throw issue;
    }}();
  ERS_DEBUG(2, "Successfully read initial config file into ptree");

  for (const auto& in_file : in_files) {
    const auto in_file_path = fmt::format("{}/{}", m_output_path, in_file);

    // FIXME TODO REMOVE or is this necessary?
    std::ifstream in_file_check;
    in_file_check.open(in_file_path, std::ios::in);

    if (in_file_check.peek() == std::ifstream::traits_type::eof()) {
      ERS_DEBUG(2, fmt::format("File: {} is empty", in_file));
      in_file_check.close();
      continue;
    } else {
      ERS_DEBUG(2, fmt::format("File: {} has data", in_file));
      in_file_check.close();
    }

    pt::ptree trimmer_conf{};
    pt::read_json(in_file_path, trimmer_conf);

    updatePtreeWithFeb(prev_conf, trimmer_conf);
  }

  const auto editedconfig =
    fmt::format("run_config_wsdsm_RMSx{}.json", m_rms_factor);
  ERS_LOG(fmt::format("New config file name: {}", editedconfig));

  pt::write_json(fmt::format("{}/{}", m_output_path, editedconfig), prev_conf);
}

// pt::ptree updatePtreeWithFeb(pt::ptree input, pt::ptree update)
void nsw::THRCalib::updatePtreeWithFeb(pt::ptree& input, pt::ptree update)
{
  const auto fename = update.get<std::string>("OpcNodeId");

  // Find the key in the original JSON corresponding to this SCA address
  const auto initial_key = [&input, &fename]() {
    for (const auto& node : input) {
      const auto key = node.first;
      const auto address = node.second.get_optional<std::string>("OpcNodeId");
      if (address != boost::none) {
        if (address == fename) {
          return key;
        }
      }
    }
    throw nsw::THRCalibIssue(ERS_HERE,
                             fmt::format("Unable to find a node with an SCA address {} in "
                                         "initial config: this should not be possible!",
                                         fename));
  }();

  // Select the FEB we are updating
  auto& original_feb = input.get_child(initial_key);

  for (std::size_t nth_vmm{0}; nth_vmm < nsw::NUM_VMM_PER_SFEB; ++nth_vmm) {
    const auto vmm = fmt::format("vmm{}", nth_vmm);
    const auto child_name = fmt::format("{}.{}", fename, vmm);

    ERS_DEBUG(2, fmt::format("Looking for node: {}", vmm));
    if (update.count(vmm) == 0) {
      continue;
    }

    if (original_feb.count(vmm) == 0) {
      original_feb.add_child(vmm, update.get_child(vmm));
    } else {
      auto& original_vmm = original_feb.get_child(vmm);
      const auto& updated_vmm = update.get_child(vmm);
      // iterate over keys to update
      for (const auto& node : updated_vmm) {
        // if update has a matching key, update
        if (node.first.find("channel_") == std::string::npos) {
          original_vmm.put(node.first, node.second.data());
        } else {
          // channel settings may be arrays
          original_vmm.put_child(node.first, node.second);
        }
      }
    }
    ERS_DEBUG(2, fmt::format("Added {} VMM{} node", fename, nth_vmm));
  }

  // // TODO or this is taken by non-const reference and modified in place
  // return input;
}

void nsw::THRCalib::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                         const std::string& is_db_name)
{
  const auto calib_param_is_name = fmt::format("{}.Calib.calibParams", is_db_name);

  try {
    if (not is_dictionary.contains(calib_param_is_name)) {
      throw nsw::THRParameterIssue(ERS_HERE, fmt::format("Unable to find {} in IS", calib_param_is_name));
    }

    ISInfoDynAny calib_params_from_is;
    is_dictionary.getValue(calib_param_is_name, calib_params_from_is);
    const auto calibParams = calib_params_from_is.getAttributeValue<std::string>(0);
    ERS_INFO(fmt::format("Calibration Parameters from IS: {}", calibParams));

    const auto run_params{nsw::THRCalib::parseCalibParams(calibParams)};

    m_n_samples = run_params.samples;
    m_rms_factor = run_params.factor;
    m_run_type = run_params.type;
    m_debug = run_params.debug;
  } catch (const nsw::THRParameterIssue& is) {
    const auto is_cmd = fmt::format("is_write -p ${{TDAQ_PARTITION}} -n {} -t String -v Type,samples,xRMS,debug -i 0", calib_param_is_name);
    nsw::calib::IsParameterNotFoundUseDefault issue(ERS_HERE, "calibParams", is_cmd, "Running a threshold scan using default n_sample(100/chan)/m_rms_factor(x6)/ settings");
    ers::warning(is);
    ers::warning(issue);
    m_run_type = "thresholds";
  }
  ERS_INFO(fmt::format("Run type - {}", m_run_type));
  std::this_thread::sleep_for(500ms);
}

nsw::THRCalib::RunParameters nsw::THRCalib::parseCalibParams(const std::string& calibParams)
{
  const auto tokens{nsw::tokenizeString(calibParams, ",")};
  constexpr std::size_t NUM_THR_PARAMS{4};
  if (tokens.size() != NUM_THR_PARAMS) {
    throw nsw::THRParameterIssue(ERS_HERE, fmt::format("Expected to parse {} parameters from {}", NUM_THR_PARAMS, calibParams));
  }

  nsw::THRCalib::RunParameters run_params{};
  try {
    const auto n_samples{std::stoull(tokens.at(1))};
    const auto rms_factor{std::stoull(tokens.at(2))};
    const auto dflag{std::stoi(tokens.back())};

    ERS_DEBUG(2, fmt::format("Check: {}--{}--{}", n_samples, rms_factor, dflag));

    run_params.samples = static_cast<std::size_t>(n_samples);
    run_params.factor  = static_cast<std::size_t>(rms_factor);
    run_params.debug = static_cast<bool>(dflag);
  } catch (const std::exception& ex) {
    throw nsw::THRParameterIssue(ERS_HERE, fmt::format("Error parsing parameters: {}", ex.what()));
  }

  const auto type = tokens.front();
  if (type == "BLN") {
    run_params.type = "baselines";
  } else if (type == "RTH") {
    run_params.type = "read_thresholds";
  } else if (type == "baselines_thresholds") {
    run_params.type = "baselines_thresholds";
  } else if (type == "THR") {
    run_params.type = "thresholds";
  } else {
    throw nsw::THRParameterIssue(ERS_HERE,
                                 fmt::format("Invalid calibration type specified: {} (expected BLN, RTH, or THR, or even baselines_thresholds)", type));
  }
  return run_params;
}
