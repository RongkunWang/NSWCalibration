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

int main(int argc, const char *argv[])
{
    std::string config_files = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files";
    std::string config_filename;
    std::string board_name;
    bool simulation;

    // command line args
    po::options_description desc(std::string("ART hit counters reader"));
    desc.add_options()
        ("help,h", "produce help message")
        ("config_file,c", po::value<std::string>(&config_filename)->
         default_value(config_files+"/config_json/BB5/A10/full_small_sector_a10_bb5_ADDC_TP.json"),
         "Configuration file path")
        ("simulation", po::bool_switch()->
         default_value(false), "Option to disable all I/O with the hardware")
        ("name,n", po::value<std::string>(&board_name)->
         default_value(""), "The name of frontend to configure (should start with ADDC_).");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    simulation = vm["simulation"].as<bool>();
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    // addc objects
    auto cfg = "json://" + config_filename;
    auto addc_configs = nsw::ConfigReader::makeObjects<nsw::ADDCConfig>(cfg, "ADDC", board_name);

    // launch monitoring thread
    auto watchdog = std::async(std::launch::async, addc_hit_watchdog, addc_configs, simulation);

    // wait for user to end
    std::cout << "Press [Enter] to end" << std::endl;
    std::cin.get();
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
  std::string rname = "addc_hits." + now + ".root";
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
