// Program to read MMTP status registers

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <cmath>
#include <unistd.h>
#include <signal.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/TPConfig.h"
#include "NSWConfiguration/Utility.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfigurationDal/NSWConfigApplication.h"
#include "NSWCalibration/Utility.h"
#include "NSWCalibration/MMTPFeedback.h"
#include "NSWCalibration/MMTPStatusRegisters.h"
#include <TFile.h>
#include <TTree.h>
#include <TInterpreter.h>

#include <ipc/core.h>
#include <RunControl/RunControl.h>
#include <RunControl/Common/OnlineServices.h>
#include <is/infodictionary.h>
#include <is/infoiterator.h>
#include <is/infodynany.h>
#include <is/criteria.h>

// Generated (open62541-compat) file
#include <logit_logger.h>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

int tp_watchdog(std::string config, int sleep_time, bool reset_l1a, bool sim, bool debug);
int tp_feedback(const std::string& data_file,
                const std::string& json_file,
                const std::string& outr_name,
                size_t noisy_channel, size_t nsleep,
                bool sim, bool debug);
std::string exec(const char* cmd);
std::string metadata();
std::string swrod_current_file();
std::string nswconfig_dbconn();
uint32_t wordify(const std::vector<uint8_t> & vec);
std::atomic<bool> end(false);
std::atomic<bool> interrupt(false);

int main(int argc, const char *argv[])
{
    //
    // logging
    //
    Log::initializeLogging();
    initializeOpen62541LogIt(Log::ERR);

    //
    // options for reading status registers
    //
    std::string config_filename;
    std::string board_name;
    int sleep_time;
    bool sim;
    bool reset_l1a;
    bool debug;

    //
    // options for masking noisy channels via MM TP feedback
    //
    bool   feedback                = false;
    size_t feedback_sleep          = 140;
    size_t feedback_noisy_channel  = 1e3;
    std::string feedback_data_file = "";

    //
    // command line args
    //
    po::options_description desc(std::string("TP diagnostics reader"));
    desc.add_options()
        ("help,h", "produce help message")
        ("config_file,c", po::value<std::string>(&config_filename)->
         default_value(""), "Configuration file path")
        ("sim", po::bool_switch()->
         default_value(false), "Option to disable all I/O with the hardware")
        ("reset_l1a,r", po::bool_switch()->
         default_value(false), "Option to reset L1A packet builder on each iteration")
        ("debug,d", po::bool_switch()->
         default_value(false), "Option to print info to the screen")
        ("sleep", po::value<int>(&sleep_time)->
         default_value(1), "The amount of time to sleep between each iteration")
        ("name,n", po::value<std::string>(&board_name)->
         default_value(""), "The name of frontend to configure (should start with MMTP_).")
        ("feedback_data_file", po::value<std::string>(&feedback_data_file)->
         default_value(feedback_data_file), "Name of channel rates data file")
        ("feedback_sleep", po::value<size_t>(&feedback_sleep)->
         default_value(feedback_sleep), "Time to sleep before reading channel rates")
        ("feedback_noisy_channel", po::value<size_t>(&feedback_noisy_channel)->
         default_value(feedback_noisy_channel), "Rate to consider a channel noisy")
        ("feedback", po::bool_switch()->
         default_value(feedback), "Option to mask channels based on MM TP feedback")
      ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    sim       = vm.at("sim")       .as<bool>();
    reset_l1a = vm.at("reset_l1a") .as<bool>();
    debug     = vm.at("debug")     .as<bool>();
    feedback  = vm.at("feedback")  .as<bool>();
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    //
    // option to guess configuration from active partition
    //
    if (config_filename == "") {
      std::cout << std::endl;
      std::cout << "Attempting to get json from partition..." << std::endl;
      config_filename = nswconfig_dbconn();
      std::cout << "Guessed: " << config_filename << std::endl;
      std::cout << std::endl;
    }

    //
    // TP objects
    //
    auto cfg = "json://" + config_filename;
    auto tps = nsw::ConfigReader::makeObjects<nsw::TPConfig>(cfg, "TP", board_name);
    if (tps.size() > 1) {
      std::cout << "Can only analyze 1 TP for now." << std::endl;
      return 1;
    }
    auto tp = tps.at(0);

    //
    // launch monitoring thread (class-based)
    //
    auto watchdog = std::async(std::launch::async, tp_watchdog,
                               config_filename, sleep_time, reset_l1a, sim, debug);

    //
    // launch channel rate feedback thread
    //
    if (feedback) {
      const auto data_file = (feedback_data_file == "") ? swrod_current_file() : feedback_data_file;
      const auto outr_name  = "mmtpfeedback." + metadata();
      std::cout << std::endl;
      std::cout << "Data file for MM TP channel rates:" << std::endl;
      std::cout << data_file << std::endl;
      std::cout << std::endl;
      auto feedback = std::async(std::launch::async, tp_feedback,
                                 data_file, config_filename, outr_name,
                                 feedback_noisy_channel, feedback_sleep,
                                 sim, debug);
    } else {
      std::cout << "Will not consider MM TP feedback for noisy channels" << std::endl;
    }

    //
    // wait for user to end
    //
    std::cout << "Press [Enter] to end" << std::endl;
    std::cin.get();
    end = 1;
    usleep(1e6);

    //
    // wait for threads to end
    //
    watchdog.get();

    return 0;
}

static void sig_handler(int sig) {
  std::cout << "Caught: " << sig << std::endl;
  end = 1;
  interrupt = 1;
}

//
// A thread for reading MM TP status registers periodically
//   and writing them to a ROOT file
//
int tp_watchdog(std::string config, int sleep_time, bool reset_l1a, bool sim, bool debug) {

  //
  // protect against Ctrl+C
  //
  signal(SIGINT, sig_handler);

  //
  // create and setup
  //
  auto statusregs = std::make_unique<nsw::MMTPStatusRegisters>();
  statusregs->SetDebug     (debug);
  statusregs->SetSimulation(sim);
  statusregs->SetConfig    (config);
  statusregs->SetResetL1A  (reset_l1a);
  statusregs->SetSleepTime (sleep_time);
  statusregs->SetMetadata  (metadata());

  //
  // initialize
  //
  statusregs->Initialize();

  //
  // execute
  //
  while (!end) {
    try {
      statusregs->Execute();
    } catch (std::exception & ex) {
      std::cout << "Exception: " << ex.what() << std::endl;
      break;
    }
    sleep(statusregs->SleepTime());
  }

  //
  // finalize
  //
  std::cout << "Finalizing..." << std::endl;
  statusregs->Finalize();
  if (interrupt)
    std::cout << std::endl << "Press [Enter] again please." << std::endl << std::endl;


  return 0;
}

//
// A thread for reading MM TP channel rates
//   and masking noisy channels according to the user
//
int tp_feedback(const std::string& data_file,
                const std::string& json_file,
                const std::string& outr_name,
                size_t noisy_channel, size_t nsleep,
                bool sim, bool debug) {

  try {
    
    //
    // check args
    //
    if (data_file == "") {
      std::cout << "No data file was given to tp_feedback. Exiting." << std::endl;
      return 1;
    }
    if (json_file == "") {
      std::cout << "No json file was given to tp_feedback. Exiting." << std::endl;
      return 1;
    }

    //
    // configure outputs
    // disable VMM threshold manipulation for now
    //
    const auto now = nsw::calib::utils::strf_time();
    std::string root_file = data_file + "." + now + ".root";
    std::string outj_file = json_file + "." + now + ".json";
    std::string outr_file = outr_name + "." + now + ".root";
    size_t noisy_vmm = 1e9;

    //
    // feedback class
    //
    auto feedback = std::make_unique<nsw::MMTPFeedback>();

    //
    // configuration
    //
    feedback->SetDebug(debug);
    feedback->SetSimulation(sim);
    feedback->SetChannelRateDataFile(data_file);
    feedback->SetChannelRateRootFile(root_file);
    feedback->SetNoisyChannelFactor(noisy_channel);
    feedback->SetNoisyVMMFactor(noisy_vmm);
    feedback->SetInputJsonFile(json_file);
    feedback->SetOutputJsonFile(outj_file);
    feedback->SetOutputRootFile(outr_file);
    feedback->SetLoop(0);

    //
    // wait for rates to be collected
    //
    std::cout << "" << std::endl;
    std::cout << "Waiting for " << nsleep
              << " seconds for MM TP feedback"
              << std::endl;
    std::cout << "" << std::endl;
    while (nsleep > 0) {
      if (end) {
        std::cout << std::endl;
        std::cout << "Breaking feedback" << std::endl;
        return 0;
      }
      nsleep--;
      sleep(1);
    }

    //
    // run
    //
    feedback->AnalyzeNoise();

  } catch (std::exception & ex) {

    std::cout << "Caught exception: " << ex.what() << std::endl;
    return -1;

  }

  return 0;
}

std::string exec(const char* cmd) {
  //
  // https://stackoverflow.com/questions/52164723/
  //
  constexpr size_t bufsize = 128;
  std::array<char, bufsize> buffer;
  std::string result;
  std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), bufsize, pipe.get()) != nullptr)
      result += buffer.data();
  }
  return result;
}

std::string metadata() {
  //
  // If a partition environment is active,
  //   this will get the run and lab/sector.
  // Otherwise, it'll give some dummy info.
  // NB: The only segment in the NSW partition
  //     with a hyphen is the swROD segment,
  //     which is named LAB-SECTOR.
  //     Its evil, I know.
  //
  const std::string dummy_run = "0000000000";
  const std::string dummy_lab = "LAB-SECTOR";
  const std::string dummy     = dummy_run + "." + dummy_lab;
  const std::string cmd_partition  = "ipc_ls -P | grep $TDAQ_PARTITION";
  const std::string cmd_run_number = "rc_getrunnumber";
  const std::string cmd_lab_sector = "rc_print_tree | grep '-' | tr -d '[:blank:]' ";
  if (std::getenv("TDAQ_PARTITION") == nullptr) {
    std::cout << "FYI: failed to find $TDAQ_PARTITION" << std::endl;
    return dummy;
  }
  auto partition_exists = exec(cmd_partition.c_str());
  if (partition_exists.size() == 0) {
    std::cout << "FYI: $TDAQ_PARTITION is not active" << std::endl;
    return dummy;
  }
  auto run_number = exec(cmd_run_number.c_str());
  if (run_number.size() == 0) {
    std::cout << "FYI: rc_getrunnumber failed" << std::endl;
    return dummy;
  }
  try {
    std::stoi(run_number);
  } catch (std::exception & ex) {
    std::cout << "Cant convert this to int: " << run_number << std::endl;
    return dummy;
  }
  auto lab_and_sector = exec(cmd_lab_sector.c_str());
  if (lab_and_sector.size() == 0) {
    std::cout << "FYI: rc_print_tree failed" << std::endl;
    return dummy;
  }
  std::string ret(run_number + "." + lab_and_sector);
  ret.erase(std::remove(ret.begin(), ret.end(), '\n'), ret.end());
  return ret;
}

uint32_t wordify(const std::vector<uint8_t> & vec) {
  if (vec.size() != nsw::NUM_BYTES_IN_WORD32) {
    std::runtime_error("Cannot convert std::vector<uint8_t> to uint32_t");
  }
  uint32_t ret = 0;
  for (size_t it = 0; it < vec.size(); it++) {
    ret += (static_cast<uint32_t>(vec.at(it)) << it*nsw::NUM_BITS_IN_BYTE);
  }
  return ret;
}

std::string swrod_current_file() {

  try {

    //
    // get partition environment
    //
    const auto part = std::getenv("TDAQ_PARTITION");
    if (part == nullptr)
      throw std::runtime_error("Error: TDAQ_PARTITION not defined");
    const std::string part_name(part);

    //
    // setup
    //
    int argc = 0;
    char* argv[1];
    IPCCore::init(argc, argv);
    IPCPartition partition(part_name);

    //
    // query IS
    //
    const std::string server  = "DF";
    const std::string writer  = "FileWriter";
    const std::string rates   = "ChannelRates";
    const std::string current = "currentFile";
    ISInfoIterator ii(partition, server, ISCriteria(".*" + writer + ".*"));
    while( ii() ) {

      if (ii.name().find(rates) == std::string::npos) {
        continue;
      }

      ISInfoDynAny isa;
      ii.value(isa);

      const auto currentfile = isa.getAttributeValue<std::string>("currentFile");
      return currentfile;

    }

  } catch (std::exception & ex) {

    std::cout << "Caught exception: " << ex.what() << std::endl;
    return "";

  }

  return "";
}

std::string nswconfig_dbconn() {

  try {

    //
    // I tried my best to retrieve this in a proper way
    // Just know, reader, than you cannot hate this solution more than I do
    //
    std::string cmd = "";
    cmd = cmd + " oks_dump ${TDAQ_DB/oksconfig:/} -c NSWConfigApplication";
    cmd = cmd + " | grep dbConnection ";
    cmd = cmd + " | grep -v name ";
    auto return_value = exec(cmd.c_str());

    //
    // beautify
    //
    std::vector<std::string> remove_me = {};
    remove_me.push_back("dbConnection:");
    remove_me.push_back("json://");
    remove_me.push_back(" ");
    remove_me.push_back("\"");
    remove_me.push_back("\n");
    remove_me.push_back("\r");
    for (const auto& remove: remove_me) {
      while (return_value.find(remove) != std::string::npos) {
        return_value.erase(return_value.find(remove), remove.size());
      }
    }

    //
    // fin
    //
    return return_value;

  } catch (std::exception & ex) {

    std::cout << "Caught exception: " << ex.what() << std::endl;
    return "";

  }

  return "";
}

