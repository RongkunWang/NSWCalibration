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

#include <is/infodictionary.h>
#include <is/infodynany.h>

#include <RunControl/RunControl.h>
#include <RunControl/Common/OnlineServices.h>
#include <RunControl/Common/RunControlCommands.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "NSWCalibrationDal/NSWCalibApplication.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/Constants.h"

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/CalibTypes.h"
#include "NSWCalibration/Issues.h"
#include "NSWCalibration/Utility.h"

#include "NSWCalibration/VmmTrimmerScaCalibration.h"
#include "NSWCalibration/VmmBaselineScaCalibration.h"

namespace pt = boost::property_tree;
namespace fs = std::filesystem;

using namespace std::chrono_literals;

nsw::THRCalib::THRCalib(std::string calibType,
                        const hw::DeviceManager& deviceManager,
                        std::string calibIsName,
                        const ISInfoDictionary& calibIsDict) :
  CalibAlg(std::move(calibType), deviceManager),
  m_isDbName(std::move(calibIsName)),
  m_isInfoDict(calibIsDict)
{}

void nsw::THRCalib::setup(const std::string& db)
{
  m_configFile = db;
  read_config(m_configFile);

  auto& runCtlOnlServ = daq::rc::OnlineServices::instance();
  const auto& rcBase = runCtlOnlServ.getApplication();
  const auto* calibApp = rcBase.cast<nsw::dal::NSWCalibApplication>();

  const auto calibOutPath = calibApp->get_CalibOutput();
  // const auto ipcpartition = runCtlOnlServ.getIPCPartition();

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

  ERS_INFO(fmt::format("Calibration Application name is {} |Wheel={}|sector={}|", m_app_name, m_wheel, m_sector));

  try {
    ISInfoDynAny runPar;
    m_isInfoDict.getValue("RunParams.RumParams", runPar);
    m_run_nr = runPar.getAttributeValue<std::uint32_t>("run_number");
    ERS_INFO(fmt::format( "Run number is: {}", m_run_nr));
  } catch (const daq::is::Exception& e) {
    ers::warning(
      nsw::THRCalibIssue(ERS_HERE, fmt::format("Could not find RUN number because: {}", e.what())));
    m_run_nr = 0;
  }

  if (m_run_nr != 0) {
    m_run_string = std::to_string(m_run_nr);
  } else {
    m_run_string = nsw::calib::utils::strf_time();
    ERS_LOG(fmt::format("Using run number timestamp={}", m_run_string));
  }

  m_output_path = fmt::format("{}/{}", calibOutPath, m_run_string);
  ERS_INFO(fmt::format("Calibration data will be written to: {}/{}", m_output_path, m_run_string));
  fs::create_directories(fs::path(m_output_path));

  std::this_thread::sleep_for(2000ms);
  get_setup_from_IS();

  ERS_INFO(fmt::format("Threshold calibration type - {}", m_run_type));
}

void nsw::THRCalib::configure()
{
  if (m_run_type == "baselines") {
    launch_feb_calibration<nsw::VmmBaselineScaCalibration>();
    std::this_thread::sleep_for(2000ms);
  } else if (m_run_type == "thresholds") {
    launch_feb_calibration<nsw::VmmTrimmerScaCalibration>();
    std::this_thread::sleep_for(2000ms);
    merge_json();
  } else {
    ERS_INFO("Run type was not recognised - nothing to do");
  }
  ERS_INFO(fmt::format("{} done!", m_run_type));
}

void nsw::THRCalib::read_config(const std::string& config_db)
{
  nsw::ConfigReader reader(config_db);
  try {
    m_config = reader.readConfig();
  } catch (const std::exception& e) {
    nsw::THRCalibIssue issue(ERS_HERE, fmt::format("Can't read config file due to : {}", e.what()));
    ers::fatal(issue);
    throw issue;
  }

  ERS_DEBUG(2, fmt::format("Reading configuration for full set of FEBs specified by {}", config_db));

  for (const auto& name : reader.getAllElementNames()) {
    try {
      m_feconfigs.emplace_back(reader.readConfig(name));
      m_fenames.emplace(m_feconfigs.back().getAddress());
    } catch (const std::exception& e) {
      nsw::THRCalibIssue issue(
        ERS_HERE,
        fmt::format(
          "Skipping FE {}! - Problem constructing configuration due to : [{}]", name, e.what()));
      ers::error(issue);
      // FIXME throw?
      // throw issue;
    }
  }
}

void nsw::THRCalib::merge_json()
{
  ERS_INFO("Merging generated common configuration trees");

  ERS_INFO(fmt::format("Run number string = {}", m_run_string));

  const auto in_files = [this]() {
    std::vector<std::string> tmp_files;
    const fs::path dir{m_output_path};
    if (fs::exists(dir)) {
      for (auto const& ent : fs::directory_iterator{dir}) {
        const std::string file_n = ent.path().filename();
        if (file_n.find("partial_config") != std::string::npos) {
          tmp_files.push_back(file_n);
          ERS_INFO(fmt::format("Found partial config [ {} ]", file_n));
        }
      }
    }

    std::sort(std::begin(tmp_files), std::end(tmp_files));

    return tmp_files;
  }();

  ERS_INFO(fmt::format("JSON directory has [{}] files, found: {}", in_files.size(), m_configFile));

  const auto start_configuration = m_configFile.erase(0, 7);
  ERS_INFO(fmt::format("Modified config-filename: {}", m_configFile));

  ERS_DEBUG(2, fmt::format("Start config - {}", start_configuration));

  std::ifstream json_check;

  json_check.open(start_configuration, std::ios::in);
  if (json_check.peek() == std::ifstream::traits_type::eof()) {
    ERS_INFO(fmt::format("Config file: {} is empty!", start_configuration));
  } else {
    ERS_INFO(fmt::format("Config file: {} is OK", start_configuration));
  }

  ERS_DEBUG(2, "Reading config file into ptree");
  auto& prev_conf = m_config;
  ERS_DEBUG(2, "Read config file into ptree");

  std::unordered_map<std::string, std::size_t> feb_vmms;

  for (const auto& feb : m_feconfigs) {
    feb_vmms.insert_or_assign(feb.getAddress(), feb.getVmms().size());
    ERS_DEBUG(2, fmt::format("FEB - {} has {} VMMs", feb.getAddress(), feb.getVmms().size()));
  }

  ERS_DEBUG(2, fmt::format("partial config directory has |{}| files", in_files.size()));
  pt::ptree mmfe_conf;
  for (const auto& in_file : in_files) {
    const auto in_file_path = fmt::format("{}/{}", m_output_path, in_file);
    std::ifstream in_file_check;
    in_file_check.open(in_file_path, std::ios::in);

    if (in_file_check.peek() == std::ifstream::traits_type::eof()) {
      ERS_DEBUG(2, fmt::format("FILE({}) is EMPTY", in_file));
      in_file_check.close();
      continue;
    } else {
      ERS_DEBUG(2, fmt::format("File: {} is OK", in_file));
    }

    in_file_check.close();
    ERS_DEBUG(3, fmt::format("FILE({}) has data", in_file));
    pt::read_json(in_file_path, mmfe_conf);
    const auto fename = mmfe_conf.get<std::string>("OpcNodeId");

    for (std::size_t nth_vmm = 0; nth_vmm < feb_vmms.at(fename); nth_vmm++) {
      const auto vmm = fmt::format("vmm{}", nth_vmm);
      const auto child_name = fmt::format("{}.{}", fename, vmm);
      ERS_DEBUG(2, fmt::format("Looking for node: {}", vmm));
      if (mmfe_conf.count(vmm) == 0) {
        ERS_DEBUG(2, "Failed");
        continue;
      } else {
        ERS_DEBUG(2, "Success");
      }
      prev_conf.add_child(child_name, mmfe_conf.get_child(vmm));
      ERS_DEBUG(2, fmt::format("Added {} VMM{} node", fename, nth_vmm));
    }
  }

  const auto editedconfig =
    fmt::format("run_{}_config_wsdsm_RMSx{}.json", m_run_string, m_rms_factor);
  ERS_LOG(fmt::format("New config file name: {}", editedconfig));

  const auto write_this_json = fmt::format("{}/{}", m_output_path, editedconfig);
  pt::write_json(write_this_json, prev_conf);
}

void nsw::THRCalib::get_setup_from_IS()
{
  std::string sca_read_params;
  const auto calibParamsIsName = fmt::format("{}.Calib.calibParams", m_isDbName);
  if (m_isInfoDict.contains(calibParamsIsName)) {
    ISInfoDynAny calibParamsFromIS;
    m_isInfoDict.getValue(calibParamsIsName, calibParamsFromIS);
    sca_read_params = calibParamsFromIS.getAttributeValue<std::string>(0);
    ERS_INFO(fmt::format("Calibration Parameters from IS: {}", sca_read_params));
    std::this_thread::sleep_for(2000ms);

    const auto type = sca_read_params.substr(0, 3);
    const auto run_params = sca_read_params.substr(4, sca_read_params.length());
    int dflag = 0;

    sscanf(run_params.c_str(), "%zu,%zu,%d", &m_n_samples, &m_rms_factor, &dflag);
    ERS_DEBUG(2, fmt::format("Check: {}--{}--{}", m_n_samples, m_rms_factor, dflag));

    m_debug = static_cast<bool>(dflag);

    if (type == "BLN") {
      m_run_type = "baselines";
      ERS_INFO(fmt::format("Setting up BASELINE run |{}", m_run_type));
    } else if (type == "THR") {
      m_run_type = "thresholds";
      ERS_INFO(fmt::format("Setting up THRESHOLD run |", m_run_type));
    } else {
      ers::warning(
        nsw::THRCalibIssue(ERS_HERE,
                           "Calibration parameters were not specified "
                           "(nedd char B (baseline) or T (threshold)) or not entered correctly - "
                           "defaulting to threshold type"));
      m_run_type = "thresholds";
    }
  } else {
    const auto is_cmd = fmt::format(
      "is_write -p ${{TDAQ_PARTITION}} -n {} -t String  -v '<params>' -i 0", calibParamsIsName);
    ers::warning(nsw::calib::IsParameterNotFound(ERS_HERE, "calibParams", is_cmd));
    ers::warning(nsw::THRCalibIssue(ERS_HERE,
                                    "Calibration parameters were not specified - "
                                    "using default n_sample(100/chan)/m_rms_factor(x6)/ settings"));
    m_run_type = "thresholds";
  }
  ERS_INFO(fmt::format("Run type - {}", m_run_type));
  std::this_thread::sleep_for(2000ms);
}
