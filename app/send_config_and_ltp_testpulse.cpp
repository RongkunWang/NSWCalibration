//
// A program to send a VMM configuration (specifically, VMM test pulse bits),
//    and then tell a LTP to generate pulses
//

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <numeric>
#include <ctime> 
#include <chrono> 

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "boost/program_options.hpp"

namespace po  = boost::program_options;
namespace chr = std::chrono;

int main(int ac, const char *av[]) {

    std::string base_folder = "/eos/atlas/atlascerngroupdisk/det-nsw/sw/configuration/config_files/";
    std::string ttc_folder  = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/";
    std::string description = "This program loops over VMM configurations and runs LTP TP+L1As";

    int dry_run;
    int sleep_time;
    int targeted_feb_id;
    int targeted_vmm_id;
    int targeted_channel_id;
    int n_loops;
    int n_simul_ch;
    int n_simul_vmm;
    int n_simul_feb;
    int mask;
    std::string config_filename;
    std::string fe_name;
    std::string ltp_rst;
    std::string ltp_script;
    po::options_description desc(description);
    desc.add_options()
        ("help,h", "produce help message")
        ("configfile,c", po::value<std::string>(&config_filename)->
        default_value(base_folder + "integration_config.json"),
        "Configuration file path")
        ("name,n", po::value<std::string>(&fe_name)->
        default_value(""),
        "The name of frontend to configure (must contain MMFE8, SFEB or PFEB).\n"
        "If this option is left empty, all front end elements in the config file will be scanned.")
        ("feb,F", po::value<int>(&targeted_feb_id)->
        default_value(-1), "FEB id to read (otherwise: loop)")
        ("vmm,V", po::value<int>(&targeted_vmm_id)->
        default_value(-1), "VMM id (0-7) to read (otherwise: loop)")
        ("channel,C", po::value<int>(&targeted_channel_id)->
        default_value(-1), "VMM channel to read (otherwise: loop)")
        ("sleep", po::value<int>(&sleep_time)->
        default_value(5), "Time for sleeping between measurements")
        ("rst,R", po::value<std::string>(&ltp_rst)->
        default_value(ttc_folder + "x86_64-centos7-gcc8-opt/NSWTTCConfig/nswttc_send_commands -s 128"), "LTP reset script")
        ("ltp,L", po::value<std::string>(&ltp_script)->
        default_value(ttc_folder + "NSWTTCConfig/scripts/menuRCDLtp.expect"), "Path to the LTP expect script")
        ("nloops,N", po::value<int>(&n_loops)->
        default_value(1), "Number of loops to run the LTP")
        ("nch", po::value<int>(&n_simul_ch)->
        default_value(1), "Number of channels to test pulse simultaneously")
        ("nvmm", po::value<int>(&n_simul_vmm)->
        default_value(1), "Number of VMMs to test pulse simultaneously")
        ("nfeb", po::value<int>(&n_simul_feb)->
        default_value(1), "Number of FEBs to test pulse simultaneously")
        ("dry", po::value<int>(&dry_run)->
        default_value(0), "Dry run (1 for yes, 0 for no)")
        ("mask", po::value<int>(&mask)->
        default_value(1), "Mask other channels on the VMM (1 for yes, 0 for no)")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    nsw::ConfigReader reader1("json://" + config_filename);
    try {
      auto config1 = reader1.readConfig();
    } catch (std::exception & e) {
      std::cout << "Make sure the json is formed correctly. "
                << "Can't read config file due to : " << e.what() << std::endl;
      std::cout << "Exiting..." << std::endl;
      exit(0);
    }

    // If no name is given, find all elements
    std::set<std::string> frontend_names;
    if (fe_name != "")
      frontend_names.emplace(fe_name);
    else
      frontend_names = reader1.getAllElementNames();

    // Create config objects
    std::vector<nsw::FEBConfig> frontend_configs;
    for (auto & name : frontend_names) {
      try {
        frontend_configs.emplace_back(reader1.readConfig(name));
      } 
      catch (std::exception & e) {
        std::cout << name << " - ERROR: Skipping this FE!"
                  << " - Problem constructing configuration due to : " << e.what() << std::endl;
      }
    }

    nsw::ConfigSender cs;
    int VMMS = 8;
    int CHS  = 64;
    int FEBS = (int)(frontend_configs.size());
    int time_now_s = chr::duration_cast<chr::seconds>
      (chr::system_clock::now().time_since_epoch()).count();
    int feb_id = -1;

    // the LTP commands
    std::string ltp_cmd = ltp_script + " " + std::to_string(n_loops);
    std::cout << "LTP reset   :: " << ltp_rst << std::endl;
    std::cout << "LTP command :: " << ltp_cmd << std::endl;

    for (auto & feb : frontend_configs) {

      feb_id++;

      for (int vmm_id = 0; vmm_id < VMMS; vmm_id++) {

        for (int channel_id = 0; channel_id < CHS; channel_id++) {

          if (targeted_feb_id     != -1 && feb_id     != targeted_feb_id)     continue;
          if (targeted_vmm_id     != -1 && vmm_id     != targeted_vmm_id)     continue;
          if (targeted_channel_id != -1 && channel_id != targeted_channel_id) continue;
          if (channel_id % n_simul_ch  > 0)                                   continue;
          if (vmm_id     % n_simul_vmm > 0)                                   continue;
          if (feb_id     % n_simul_feb > 0)                                   continue;

          // reset the VMM
          feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 0);
          if(mask) 
            feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 1);

          // loop
          // NB: if n_simul=1 everywhere, this is not really a "loop"
          for (int f_id = feb_id; f_id < feb_id + n_simul_feb; f_id++) {
            auto& febx = frontend_configs[f_id];
            for (int v_id = vmm_id; v_id < vmm_id + n_simul_vmm; v_id++) {
              for (int ch_id = channel_id; ch_id < channel_id + n_simul_ch; ch_id++) {

                if (ch_id >= CHS) break;
                if (v_id >= VMMS) break;
                if (f_id >= FEBS) break;

                // announce
                std::cout << "INFO"
                          << " " << "Setting test pulse for "
                          << " " << febx.getAddress()
                          << " " << " VMM_" << v_id
                          << " " << " CH_"  << ch_id
                          << std::endl;
                
                // configure this channel for test pulsing
                febx.getVmm(v_id).setChannelRegisterOneChannel("channel_st", 1, ch_id);
                if(mask)
                  febx.getVmm(v_id).setChannelRegisterOneChannel("channel_sm", 0, ch_id);
              }
              // configure
              if(!dry_run)
                cs.sendVmmConfigSingle(febx, v_id);
            }
          }
          if (n_simul_feb > 1 || n_simul_vmm > 1 || n_simul_ch > 1)
            std::cout << std::endl;

          // send the LTP command
          if(!dry_run){
            system(ltp_rst.c_str());
            system(ltp_cmd.c_str());
          }

          // reset these VMM
          for (int f_id = feb_id; f_id < feb_id + n_simul_feb; f_id++) {
            auto& febx = frontend_configs[f_id];
            for (int v_id = vmm_id; v_id < vmm_id + n_simul_vmm; v_id++) {
                if (v_id >= VMMS) break;
                if (f_id >= FEBS) break;
                febx.getVmm(v_id).setChannelRegisterAllChannels("channel_st", 0);
                if(mask)
                  febx.getVmm(v_id).setChannelRegisterAllChannels("channel_sm", 0);
                if(!dry_run)
                  cs.sendVmmConfigSingle(febx, v_id);
            }
          }

          // wait for the pulse to be sent and readout before moving on
          sleep(sleep_time);

        }
      }
    }

    int delta_t = chr::duration_cast<chr::seconds>
      (chr::system_clock::now().time_since_epoch()).count() - time_now_s;
    std::cout << "Time elapsed: " << (float)(delta_t)/60 << " minutes" << std::endl;

    return 0;
}

