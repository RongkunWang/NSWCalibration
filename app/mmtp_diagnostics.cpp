// Program to read MMTP status registers

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <cmath>
#include <signal.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/TPConfig.h"
#include "NSWConfiguration/Utility.h"
#include "NSWConfiguration/Constants.h"
#include "TFile.h"
#include "TTree.h"
#include "TInterpreter.h"

#include "boost/program_options.hpp"
namespace po = boost::program_options;

int tp_watchdog(nsw::TPConfig tp, int sleep_time, bool reset_l1a, bool sim, bool debug);
std::string exec(const char* cmd);
std::string metadata();
uint32_t wordify(const std::vector<uint8_t> & vec);
std::string strf_time();
std::atomic<bool> end(false);
std::atomic<bool> interrupt(false);

int main(int argc, const char *argv[])
{
    std::string config_files = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files";
    std::string config_filename;
    std::string board_name;
    int sleep_time;
    bool sim;
    bool reset_l1a;
    bool debug;

    // command line args
    po::options_description desc(std::string("TP diagnostics reader"));
    desc.add_options()
        ("help,h", "produce help message")
        ("config_file,c", po::value<std::string>(&config_filename)->
         default_value(config_files+"/config_json/BB5/A10/full_small_sector_a10_bb5_ADDC_TP.json"),
         "Configuration file path")
        ("sim", po::bool_switch()->
         default_value(false), "Option to disable all I/O with the hardware")
        ("reset_l1a,r", po::bool_switch()->
         default_value(false), "Option to reset L1A packet builder on each iteration")
        ("debug,d", po::bool_switch()->
         default_value(false), "Option to print info to the screen")
        ("sleep", po::value<int>(&sleep_time)->
         default_value(5), "The amount of time to sleep between each iteration")
        ("name,n", po::value<std::string>(&board_name)->
         default_value(""), "The name of frontend to configure (should start with MMTP_).");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    sim       = vm.at("sim")       .as<bool>();
    reset_l1a = vm.at("reset_l1a") .as<bool>();
    debug     = vm.at("debug")     .as<bool>();
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    // TP objects
    auto cfg = "json://" + config_filename;
    auto tps = nsw::ConfigReader::makeObjects<nsw::TPConfig>(cfg, "TP", board_name);
    if (tps.size() > 1) {
      std::cout << "Can only analyze 1 TP for now." << std::endl;
      return 1;
    }

    // launch monitoring thread
    auto tp = tps.at(0);
    auto watchdog = std::async(std::launch::async, tp_watchdog,
                               tp, sleep_time, reset_l1a, sim, debug);

    // wait for user to end
    std::cout << "Press [Enter] to end" << std::endl;
    std::cin.get();
    end = 1;
    usleep(1e6);

    return 0;
}

static void sig_handler(int sig) {
  std::cout << "Caught: " << sig << std::endl;
  end = 1;
  interrupt = 1;
}

int tp_watchdog(nsw::TPConfig tp, int sleep_time, bool reset_l1a, bool sim, bool debug) {

  //
  // output
  //
  bool quiet = !debug;
  auto now = strf_time();
  std::string rname = "mmtp_diagnostics." + metadata() + "." + now + ".root";
  auto rfile = std::make_unique< TFile >(rname.c_str(), "recreate");
  auto rtree = std::make_shared< TTree >("nsw", "nsw");
  std::string opc_ip     = tp.getOpcServerIp();
  std::string tp_address = tp.getAddress();
  uint32_t event            = -1;
  uint32_t overflow_word    = -1;
  uint32_t fiber_align_word = -1;
  auto fiber_index = std::make_unique< std::vector<uint32_t> >();
  auto fiber_align = std::make_unique< std::vector<uint32_t> >();
  auto fiber_masks = std::make_unique< std::vector<uint32_t> >();
  auto fiber_hots  = std::make_unique< std::vector<uint32_t> >();
  rtree->Branch("time",          &now);
  rtree->Branch("event",         &event);
  rtree->Branch("opc_ip",        &opc_ip);
  rtree->Branch("tp_address",    &tp_address);
  rtree->Branch("overflow_word", &overflow_word);
  rtree->Branch("sleep_time",    &sleep_time);
  rtree->Branch("reset_l1a",     &reset_l1a);
  rtree->Branch("fiber_index",   fiber_index.get());
  rtree->Branch("fiber_align",   fiber_align.get());
  rtree->Branch("fiber_masks",   fiber_masks.get());
  rtree->Branch("fiber_hots",    fiber_hots.get());
  std::cout << "Writing to " << rname << std::endl;

  //
  // TP I/O
  //
  auto cs = std::make_unique<nsw::ConfigSender>();
  std::vector<uint8_t> readback;

  //
  // protect against Ctrl+C
  //
  signal(SIGINT, sig_handler);

  //
  // polling
  //
  try {

    while (true) {

      // enable channel rate reporting
      if (event == 0) {
        try {
          std::cout << "Writing 0x01 to nsw::mmtp::REG_CHAN_RATE_ENABLE" << std::endl;
          if (!sim) {
            cs->sendTpConfigRegister(tp, nsw::mmtp::REG_CHAN_RATE_ENABLE, 0x01, quiet);
          }
        } catch (std::exception & ex) {
          std::cout << "Failed to write 0x01 to nsw::mmtp::REG_CHAN_RATE_ENABLE: " << ex.what() << std::endl;
        }
      }

      //
      // loop init
      //
      event = event + 1;
      now = strf_time();
      fiber_index->clear();
      fiber_align->clear();
      fiber_masks->clear();
      fiber_hots ->clear();

      //
      // write the fiber for easier navigating
      //
      for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
        fiber_index->push_back(fiber);
      }

      //
      // read buffer overflow
      //
      if (!sim) {
        cs           ->sendTpConfigRegister(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW, 0x00, quiet);
        readback = cs->readTpConfigRegister(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW);
      } else {
        readback = std::vector<uint8_t>(nsw::NUM_BYTES_IN_WORD32);
      }
      overflow_word = static_cast<uint32_t>(readback.at(0));
      if (debug)
        std::cout << "Overflow word: 0b" << std::bitset<nsw::NUM_BITS_IN_BYTE>(overflow_word) << std::endl;

      //
      // read fiber alignment
      //
      if (!sim) {
        readback = cs->readTpConfigRegister(tp, nsw::mmtp::REG_FIBER_ALIGNMENT);
      } else {
        readback = std::vector<uint8_t>(nsw::NUM_BYTES_IN_WORD32);
      }
      fiber_align_word = wordify(readback);
      if (debug)
        std::cout << "Fiber align word: 0b" << std::bitset<nsw::mmtp::NUM_FIBERS>(fiber_align_word) << std::endl;
      for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
        fiber_align->push_back(fiber_align_word & static_cast<uint32_t>(pow(2, fiber)));
      }

      //
      // loop over fibers
      //
      for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {

        //
        // setting the fiber of interest
        //
        if (!sim) {
          cs->sendTpConfigRegister(tp, nsw::mmtp::REG_FIBER_HOT_MUX,  fiber, quiet);
          cs->sendTpConfigRegister(tp, nsw::mmtp::REG_FIBER_MASK_MUX, fiber, quiet);
        }
        if (debug)
          std::cout << "Fiber " << fiber << std::endl;

        //
        // read hot fibers
        //
        uint32_t fiber_hot = 0xffff;
        if (!sim) {
          readback = cs->readTpConfigRegister(tp, nsw::mmtp::REG_FIBER_HOT_READ);
          fiber_hot = wordify(readback);
        } else {
          readback  = std::vector<uint8_t>(nsw::NUM_BYTES_IN_WORD32);
          fiber_hot = std::pow(2, fiber);
        }
        fiber_hots->push_back(fiber_hot);

        //
        // read masked fibers
        //
        uint32_t fiber_mask = 0xffff;
        if (!sim) {
          readback = cs->readTpConfigRegister(tp, nsw::mmtp::REG_FIBER_MASK_WRITE);
          fiber_mask = wordify(readback);
        } else {
          readback   = std::vector<uint8_t>(nsw::NUM_BYTES_IN_WORD32);
          fiber_mask = std::pow(2, fiber);
        }
        fiber_masks->push_back(fiber_mask);

      }

      //
      // debug
      //
      for (auto val: *(fiber_hots.get()))
        if (debug)
          std::cout << "Hot VMM : " << std::bitset<nsw::mmtp::NUM_VMMS_PER_FIBER>(val) << std::endl;
      for (auto val: *(fiber_masks.get()))
        if (debug)
          std::cout << "Mask VMM: " << std::bitset<nsw::mmtp::NUM_VMMS_PER_FIBER>(val) << std::endl;

      //
      // reset L1A packet builder
      //
      if (reset_l1a) {
        if (debug)
          std::cout << "Sending TP config to reset L1A builder" << std::endl;
        bool quiet = true;
        if (!sim)
          cs->sendTpConfig(tp, quiet);
      }

      //
      // fill
      //
      rtree->Fill();

      //
      // pause
      //
      if (debug) {
        std::cout << "Finished iteration " << event << " at " << now << std::endl;
        std::cout << "Press [Enter] to end" << std::endl;
      } else {
        std::cout << "." << std::flush;
      }
      sleep(sleep_time);

      //
      // end
      //
      if (end) {
        std::cout << std::endl;
        std::cout << "Breaking" << std::endl;
        break;
      }
    }
  } catch (std::exception & ex) {
    std::cout << "tp_watchdog caught exception: " << ex.what() << std::endl;
  }

  // disable channel rate reporting
  try {
    std::cout << "Writing 0x00 to nsw::mmtp::REG_CHAN_RATE_ENABLE" << std::endl;
    if (!sim) {
      cs->sendTpConfigRegister(tp, nsw::mmtp::REG_CHAN_RATE_ENABLE, 0x00, quiet);
    }
  } catch (std::exception & ex) {
    std::cout << "Failed to write 0x00 to nsw::mmtp::REG_CHAN_RATE_ENABLE: " << ex.what() << std::endl;
  }

  //
  // close
  //
  std::cout << "Closing " << rname << std::endl;
  rtree->Write();
  rfile->Close();
  std::cout << "Closed." << std::endl;
  if (interrupt)
    std::cout << "Press [Enter] again please." << std::endl;

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

std::string strf_time() {
    std::stringstream ss;
    std::string out;
    std::time_t result = std::time(nullptr);
    std::tm tm = *std::localtime(&result);
    ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
    ss >> out;
    return out;
}
