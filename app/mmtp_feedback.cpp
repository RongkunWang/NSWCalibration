// Program to read MMTP channel rates and mask channels / set thresholds

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Utility.h"
#include "NSWConfiguration/Constants.h"
#include "NSWCalibration/MMTPFeedback.h"

#include "boost/program_options.hpp"
namespace po = boost::program_options;

std::string pad(size_t val, size_t width);

int main(int argc, const char *argv[]) {

  //
  // defaults
  //
  size_t loops = 1;
  size_t noisy_c = 1e4;
  size_t noisy_v = 10;
  size_t nsleep = 128;
  std::string config_files = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files/";
  std::string dname = "/tmp/tmp.data";
  std::string rname = "/tmp/tmp.root";
  std::string jname = config_files + "config_json/BB5/C10/full_small_sector_c10_bb5_MMTP_cosmics_x9_plus12_std_dac_OFF.json";
  std::string oname = "/tmp/tmp.json";

  //
  // command line args
  //
  po::options_description desc(std::string("MMTP threshold/mask feedback app"));
  desc.add_options()
    ("help,h", "produce help message")
    ("data_file,d",     po::value<std::string>(&dname)->default_value(dname), "data file path")
    ("root_file,r",     po::value<std::string>(&rname)->default_value(rname), "ROOT file path")
    ("json_file,j",     po::value<std::string>(&jname)->default_value(jname), "JSON file path (input)")
    ("out_file,o",      po::value<std::string>(&oname)->default_value(oname), "JSON file path (output)")
    ("loops,l",         po::value<size_t>(&loops)  ->default_value(loops), "Number of loops to run feedback (-1 for infinite)")
    ("noisy_channel,c", po::value<size_t>(&noisy_c)->default_value(noisy_c),  "Noisy channel threshold")
    ("noisy_vmm,v",     po::value<size_t>(&noisy_v)->default_value(noisy_v),  "Noisy VMM threshold")
    ("sleep,s",         po::value<size_t>(&nsleep) ->default_value(nsleep),   "Number of seconds to sleep")
    ;
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << std::endl;;
    return 1;
  }

  //
  // go forth
  //
  try {

    //
    // loop
    //
    std::string jname_loop;
    std::string oname_loop;
    for (size_t loop = 0; loop < loops; loop++) {

      auto feedback = std::make_unique<nsw::MMTPFeedback>();

      //
      // set for all loops
      //
      feedback->SetDebug(false);
      feedback->SetSimulation(true);
      feedback->SetChannelRateDataFile(dname);
      feedback->SetChannelRateRootFile(rname);
      feedback->SetNoisyChannelFactor(noisy_c);
      feedback->SetNoisyVMMFactor(noisy_v);

      //
      // manage the input/output json files
      //
      jname_loop = (loop == 0) ? std::string(jname) : std::string(oname_loop);
      oname_loop = std::string(oname + "_" + pad(loop, 6));

      //
      // last loop: always user output
      //
      if (loop == loops-1) {
        oname_loop = oname;
      }

      //
      // set
      //
      feedback->SetInputJsonFile(jname_loop);
      feedback->SetOutputJsonFile(oname_loop);

      std::cout << std::endl;
      std::cout << jname_loop << std::endl;
      std::cout << oname_loop << std::endl;
      std::cout << std::endl;

      //
      // do
      //
      feedback->SetLoop(loop);
      feedback->AnalyzeNoise();
      if (loop != loops-1) {
        sleep(nsleep);
      }

    }

  } catch (std::exception & ex) {
    std::cout << "Caught exception: " << ex.what() << std::endl;
    return -1;
  }

  return 0;
}

std::string pad(size_t val, size_t width) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(width) << val;
  return ss.str();
}
