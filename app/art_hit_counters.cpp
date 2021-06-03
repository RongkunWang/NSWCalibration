// Program to read ART hit counters

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <future>

#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/ADDCConfig.h"
#include "NSWConfiguration/Utility.h"
#include "TFile.h"
#include "TTree.h"

#include "boost/program_options.hpp"

namespace po = boost::program_options;

int addc_hit_watchdog(const std::vector<nsw::ADDCConfig>& addcs, bool simulation);
std::vector<int> addc_read_register(const nsw::ADDCConfig& addc, int art, bool simulation);
std::atomic<bool> end(false);
std::string metadata();
std::string exec(const char* cmd);
std::string nswconfig_dbconn();

int main(int argc, const char *argv[])
{
    std::string config_files = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files";
    std::string config_filename;
    std::string board_name;
    bool simulation;
    bool once;

    // command line args
    po::options_description desc(std::string("ART hit counters reader"));
    desc.add_options()
        ("help,h", "produce help message")
        ("config_file,c", po::value<std::string>(&config_filename)->
         default_value(""), "Configuration file path")
        ("simulation", po::bool_switch()->
         default_value(false), "Option to disable all I/O with the hardware")
        ("once", po::bool_switch()->
         default_value(false), "Option to read the ART counters once and only once")
        ("name,n", po::value<std::string>(&board_name)->
         default_value(""), "The name of frontend to configure (should start with ADDC_).");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    simulation = vm["simulation"].as<bool>();
    once       = vm["once"]      .as<bool>();
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
      if (config_filename.size() == 0) {
        std::cout << "No json config was provided (-c)"
                  << " and no TDAQ partition environment was found."
                  << " Exiting."
                  << std::endl;
        return -1;
      }
      std::cout << "Guessed: " << config_filename << std::endl;
      std::cout << std::endl;
    }

    //
    // addc objects
    //
    auto cfg = "json://" + config_filename;
    auto addc_configs = nsw::ConfigReader::makeObjects<nsw::ADDCConfig>(cfg, "ADDC", board_name);

    //
    // launch monitoring thread
    //
    auto watchdog = std::async(std::launch::async, addc_hit_watchdog, addc_configs, simulation);

    //
    // wait for user to end, or read once
    //
    if (once) {
      usleep(1e6);
    } else {
      std::cout << "Press [Enter] to end" << std::endl;
      std::cin.get();
    }
    end = 1;
    usleep(1e6);

    return 0;
}

int addc_hit_watchdog(const std::vector<nsw::ADDCConfig>& addcs, bool simulation) {

  //
  // Sorry for all the layers of vectors.
  // 1 std::vector because looping over ADDCs
  // 1 std::vector because reading multiple registers
  //

  // output
  auto now = nsw::calib::utils::strf_time();
  std::string rname = "art_counters." + metadata() + "." + now + ".root";
  auto rfile        = std::make_unique< TFile >(rname.c_str(), "recreate");
  auto rtree        = std::make_shared< TTree >("nsw", "nsw");
  int event                = -1;
  std::string addc_address = "";
  std::string art_name     = "";
  int art_index            = -1;
  auto art_hits            = std::make_unique< std::vector<int> >();
  rtree->Branch("time",         &now);
  rtree->Branch("event",        &event);
  rtree->Branch("addc_address", &addc_address);
  rtree->Branch("art_name",     &art_name);
  rtree->Branch("art_index",    &art_index);
  rtree->Branch("art_hits",     art_hits.get());

  // polling
  try {

    auto threads = std::make_unique< std::vector< std::future< std::vector<int> > > >();

    while (true) {

      //
      // loop init
      //
      threads->clear();
      event = event + 1;
      now = nsw::calib::utils::strf_time();

      //
      // launch threads
      // https://its.cern.ch/jira/browse/OPCUA-2188
      //
      for (auto & addc : addcs)
        for (auto art: addc.getARTs())
          threads->push_back(std::async(std::launch::async, addc_read_register, addc, art.index(), simulation));

      //
      // threads results
      //
      size_t it = 0;
      for (auto & addc : addcs) {
        for (auto art: addc.getARTs()) {
          auto result = threads->at(it).get();

          addc_address = addc.getAddress();
          art_name     = art.getName();
          art_index    = it;
          art_hits->clear();
          for (auto val : result)
            art_hits->push_back(val);
          rtree->Fill();

          //
          // cout for fun
          //
          std::cout << addc.getAddress() + "." + art.getName() << " " << now << std::endl;
          int counter = 0;
          for (auto val : result) {
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(8) << val;
            std::cout << " 0x" << ss.str();
            counter++;
            if (counter % 16 == 0)
              std::cout << std::dec << std::endl;
          }
          it++;
        }
      }

      //
      // pause
      //
      usleep(1e6);

      //
      // end
      //
      if (end) {
        std::cout << "Breaking" << std::endl;
        break;
      }
    }
  } catch (std::exception & e) {
    std::cout << "addc_hit_watchdog caught exception: " << e.what() << std::endl;
  }

  //
  // close
  //
  std::cout << "Closing " << rname << std::endl;
  rtree->Write();
  rfile->Close();

  return 0;
}

std::vector<int> addc_read_register(const nsw::ADDCConfig& addc, int art, bool simulation) {
  //
  // https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?
  // sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ART/art2_registers_v.xlsx&action=default
  //
  auto cs = std::make_unique<nsw::ConfigSender>();
  uint8_t art_data[] = {0x0, 0x0};
  auto opc_ip    = addc.getOpcServerIp();
  auto sca_addr  = addc.getAddress() + "." + addc.getART(art).getNameCore();
  size_t reg_local  = 0;
  size_t word32     = 0;
  size_t index      = 0;
  std::vector<uint8_t> readback = {};
  std::vector<int> results      = {};

  //
  // query registers
  //
  for (size_t reg = nsw::art::REG_COUNTERS_START; reg < nsw::art::REG_COUNTERS_END; reg++) {

    //
    // read N registers per transaction
    //
    reg_local = reg - nsw::art::REG_COUNTERS_START;
    if ((reg_local % nsw::art::REG_COUNTERS_SIMULT) > 0)
      continue;

    //
    // register address
    //
    art_data[0] = static_cast<uint8_t>(reg);

    //
    // read the register
    //
    if (!simulation) {
      readback = cs->readI2cAtAddress(opc_ip, sca_addr, art_data, nsw::art::ADDRESS_SIZE, nsw::art::REG_COUNTERS_SIMULT);
    } else {
      readback.clear();
      for (size_t it = 0; it < nsw::art::REG_COUNTERS_SIMULT; it++)
        readback.push_back(static_cast<uint8_t>(it));
    }
    if (readback.size() != static_cast<size_t>(nsw::art::REG_COUNTERS_SIMULT))
      throw std::runtime_error("Problem reading ART register: " + addc.getAddress());

    //
    // convert N 1-byte registers into N/4 32-bit word
    //
    for (size_t it = 0; it < nsw::art::REG_COUNTERS_SIMULT; it++) {
      index = it % nsw::art::REG_COUNTERS_SIZE;
      if (index == 0)
        word32 = 0;
      word32 += (readback.at(it) << index*nsw::NUM_BITS_IN_BYTE);
      if (index == nsw::art::REG_COUNTERS_SIZE - 1)
        results.push_back(static_cast<int>(word32));
    }

  }
  return results;
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
    // If that failed, check NSWCalibApplication
    //
    if (return_value.size() == 0) {
      cmd = "";
      cmd = cmd + " oks_dump ${TDAQ_DB/oksconfig:/} -c NSWCalibApplication";
      cmd = cmd + " | grep dbConnection ";
      cmd = cmd + " | grep -v name ";
      return_value = exec(cmd.c_str());
    }

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
