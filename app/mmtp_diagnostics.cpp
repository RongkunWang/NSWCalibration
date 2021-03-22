// Program to read MMTP status registers

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <signal.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/TPConfig.h"
#include "NSWConfiguration/Utility.h"
#include "TFile.h"
#include "TTree.h"

#include "boost/program_options.hpp"
namespace po = boost::program_options;

int tp_watchdog(nsw::TPConfig tp, int sleep_time, bool reset_l1a, bool sim, bool debug);
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
         default_value(5), "The amount of time to sleep between each iteration.")
        ("name,n", po::value<std::string>(&board_name)->
         default_value(""), "The name of frontend to configure (should start with MMTP_).");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    sim        = vm["sim"].as<bool>();
    reset_l1a  = vm["reset_l1a"] .as<bool>();
    debug      = vm["debug"] .as<bool>();
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
  auto now = strf_time();
  std::string rname = "mmtp_diagnostics." + now + ".root";
  auto rfile        = std::make_unique< TFile >(rname.c_str(), "recreate");
  auto rtree        = std::make_shared< TTree >("nsw", "nsw");
  std::string opc_ip     = tp.getOpcServerIp();
  std::string tp_address = tp.getAddress();
  uint32_t event         = -1;
  uint32_t overflow_word = -1;
  auto fiber_masks       = std::make_unique< std::vector<uint32_t> >();
  auto fiber_hots        = std::make_unique< std::vector<uint32_t> >();
  rtree->Branch("time",          &now);
  rtree->Branch("event",         &event);
  rtree->Branch("opc_ip",        &opc_ip);
  rtree->Branch("tp_address",    &tp_address);
  rtree->Branch("overflow_word", &overflow_word);
  rtree->Branch("sleep_time",    &sleep_time);
  rtree->Branch("reset_l1a",     &reset_l1a);
  rtree->Branch("fiber_masks",   fiber_masks.get());
  rtree->Branch("fiber_hots",    fiber_hots.get());

  //
  // protect against Ctrl+C
  //
  signal(SIGINT, sig_handler);

  //
  // constants
  //
  constexpr uint32_t fiber_hot_mux     = 0x0d;
  constexpr uint32_t fiber_hot_read    = 0x0e;
  constexpr uint32_t fiber_mask_mux    = 0x1c;
  constexpr uint32_t fiber_mask_write  = 0x1d;
  constexpr uint32_t pipeline_overflow = 0x20;
  constexpr uint32_t fiber_n           = 32;
  constexpr uint32_t vmm_n             = 32;
  auto hot_read_vec  = nsw::intToByteVector(fiber_hot_read,    4, true);
  auto mask_read_vec = nsw::intToByteVector(fiber_mask_write,  4, true);
  auto overflow_vec  = nsw::intToByteVector(pipeline_overflow, 4, true);
  std::vector<uint8_t> readback;

  //
  // function for reading
  //
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto build = [](uint32_t tmp_addr, int tmp_message) {
    std::vector<uint8_t> tmp_data = nsw::intToByteVector(tmp_message, 4, true );
    std::vector<uint8_t> tmp_addrVec = nsw::intToByteVector(tmp_addr, 4, true);
    std::vector<uint8_t> tmp_entirePayload(tmp_addrVec);
    tmp_entirePayload.insert(tmp_entirePayload.end(), tmp_data.begin(), tmp_data.end() );
    return tmp_entirePayload;
  };

  // polling
  try {

    while (true) {

      //
      // loop init
      //
      event = event + 1;
      now = strf_time();
      fiber_masks->clear();
      fiber_hots->clear();

      //
      // read buffer overflow
      //
      auto msg_overflow_write = build(pipeline_overflow, 0x0);
      if (!sim) {
        cs->sendI2c(opc_ip, tp_address, msg_overflow_write);
        readback = cs->readI2cAtAddress(opc_ip, tp_address,
                                        overflow_vec.data(), overflow_vec.size(), 4);
      } else {
        readback = std::vector<uint8_t>(4);
      }
      overflow_word = static_cast<uint32_t>(readback.at(0));
      if (debug)
        std::cout << "Overflow word: 0b" << std::bitset<8>(overflow_word) << std::endl;

      //
      // loop over fibers
      //
      for (uint32_t fiber = 0; fiber < fiber_n; fiber++) {

        //
        // setting the fiber of interest
        //
        auto msg_hot_mux  = build(fiber_hot_mux,  fiber);
        auto msg_mask_mux = build(fiber_mask_mux, fiber);
        if (!sim) {
          cs->sendI2c(opc_ip, tp_address, msg_hot_mux);
          cs->sendI2c(opc_ip, tp_address, msg_mask_mux);
        }
        if (debug)
          std::cout << "Fiber " << fiber << std::endl;

        //
        // read hot fibers
        //
        uint32_t fiber_hot = 0xffff;
        if (!sim) {
          readback = cs->readI2cAtAddress(opc_ip, tp_address,
                                          hot_read_vec.data(), hot_read_vec.size(), 4);
          fiber_hot = (static_cast<uint32_t>(readback[0]) <<  0) + \
                      (static_cast<uint32_t>(readback[1]) <<  8) + \
                      (static_cast<uint32_t>(readback[2]) << 16) + \
                      (static_cast<uint32_t>(readback[3]) << 24);
        } else {
          readback  = std::vector<uint8_t>(4);
          fiber_hot = std::pow(2, fiber);
        }
        fiber_hots->push_back(fiber_hot);

        //
        // read masked fibers
        //
        uint32_t fiber_mask = 0xffff;
        if (!sim) {
          readback = cs->readI2cAtAddress(opc_ip, tp_address,
                                          mask_read_vec.data(), mask_read_vec.size(), 4);
          fiber_mask = (static_cast<uint32_t>(readback[0]) <<  0) + \
                       (static_cast<uint32_t>(readback[1]) <<  8) + \
                       (static_cast<uint32_t>(readback[2]) << 16) + \
                       (static_cast<uint32_t>(readback[3]) << 24);
        } else {
          readback   = std::vector<uint8_t>(4);
          fiber_mask = std::pow(2, fiber);
        }
        fiber_masks->push_back(fiber_mask);

      }

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
      for (auto val: *(fiber_hots.get()))
        if (debug)
          std::cout << "Hot VMM : " << std::bitset<vmm_n>(val) << std::endl;
      for (auto val: *(fiber_masks.get()))
        if (debug)
          std::cout << "Mask VMM: " << std::bitset<vmm_n>(val) << std::endl;
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

std::string strf_time() {
    std::stringstream ss;
    std::string out;
    std::time_t result = std::time(nullptr);
    std::tm tm = *std::localtime(&result);
    ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
    ss >> out;
    return out;
}
