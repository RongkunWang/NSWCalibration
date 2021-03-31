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

int tp_watchdog(nsw::TPConfig tp, int sleep_time, bool reset_l1a, bool channel_rates, bool sim, bool debug);
uint32_t wordify(const std::vector<uint8_t> & vec);
std::string strf_time();
std::atomic<bool> end(false);
std::atomic<bool> interrupt(false);

int main(int argc, const char *argv[])
{
    gInterpreter->GenerateDictionary("vector<vector<uint32_t> >", "vector");
    std::string config_files = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files";
    std::string config_filename;
    std::string board_name;
    int sleep_time;
    bool sim;
    bool reset_l1a;
    bool channel_rates;
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
        ("channel_rates", po::bool_switch()->
         default_value(false), "Option to read channel rates for every channel")
        ("debug,d", po::bool_switch()->
         default_value(false), "Option to print info to the screen")
        ("sleep", po::value<int>(&sleep_time)->
         default_value(5), "The amount of time to sleep between each iteration.")
        ("name,n", po::value<std::string>(&board_name)->
         default_value(""), "The name of frontend to configure (should start with MMTP_).");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    sim           = vm.at("sim")           .as<bool>();
    reset_l1a     = vm.at("reset_l1a")     .as<bool>();
    channel_rates = vm.at("channel_rates") .as<bool>();
    debug         = vm.at("debug")         .as<bool>();
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
                               tp, sleep_time, reset_l1a, channel_rates, sim, debug);

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

int tp_watchdog(nsw::TPConfig tp, int sleep_time, bool reset_l1a, bool channel_rates, bool sim, bool debug) {

  //
  // output
  //
  bool quiet = !debug;
  auto now = strf_time();
  std::string rname = "mmtp_diagnostics." + now + ".root";
  auto rfile        = std::make_unique< TFile >(rname.c_str(), "recreate");
  auto rtree        = std::make_shared< TTree >("nsw", "nsw");
  std::string opc_ip     = tp.getOpcServerIp();
  std::string tp_address = tp.getAddress();
  uint32_t event            = -1;
  uint32_t overflow_word    = -1;
  uint32_t fiber_align_word = -1;
  uint32_t feb_id_word      = -1;
  auto fiber_index       = std::make_unique< std::vector<uint32_t> >();
  auto fiber_align       = std::make_unique< std::vector<uint32_t> >();
  auto fiber_masks       = std::make_unique< std::vector<uint32_t> >();
  auto fiber_hots        = std::make_unique< std::vector<uint32_t> >();
  auto feb_id            = std::make_unique< std::vector<uint32_t> >();
  auto feb_channel_rates = std::make_unique< std::vector< std::vector<uint32_t> > >();
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
  if (channel_rates) {
    rtree->Branch("feb_id",            feb_id.get());
    rtree->Branch("feb_channel_rates", feb_channel_rates.get());
  }

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

      //
      // loop init
      //
      event = event + 1;
      now = strf_time();
      fiber_index       ->clear();
      fiber_align       ->clear();
      fiber_masks       ->clear();
      fiber_hots        ->clear();
      feb_id            ->clear();
      feb_channel_rates ->clear();

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
      // channel rates
      // loop over fibers and febs
      //
      if (channel_rates) {
        uint32_t channel_rate = 0xffff;
        for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
          for (uint32_t mmfe8 = 0; mmfe8 < nsw::mmtp::NUM_MMFE8_PER_FIBER; mmfe8++) {
            feb_id_word = mmfe8 + (fiber << static_cast<uint32_t>(log2(nsw::mmtp::NUM_MMFE8_PER_FIBER)));
            feb_id->push_back(feb_id_word);
            if (!sim) {
              cs->sendTpConfigRegister(tp, nsw::mmtp::REG_CHAN_RATE_FIBER, fiber, quiet);
              cs->sendTpConfigRegister(tp, nsw::mmtp::REG_CHAN_RATE_MMFE8, mmfe8, quiet);
              usleep(nsw::mmtp::CHAN_RATE_USLEEP);
            }
            feb_channel_rates->push_back(std::vector<uint32_t>());
            for (uint32_t chan = 0; chan < nsw::mmfe8::NUM_CH_PER_MMFE8; chan++) {
              if (!sim) {
                readback = cs->readTpConfigRegister(tp, nsw::mmtp::REG_CHAN_RATE_CHAN);
              } else {
                readback = std::vector<uint8_t>(nsw::NUM_BYTES_IN_WORD32);
              }
              channel_rate = wordify(readback);
              feb_channel_rates->back().push_back(channel_rate);
            }
          }
        }
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
