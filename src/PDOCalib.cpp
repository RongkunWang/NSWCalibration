#include <iostream> 
#include <thread> 
#include <sys/types.h>
#include <chrono>
#include <string>
#include <cstring>
#include <vector>
#include <iomanip>
#include <fstream>
#include <thread>
#include <numeric>
#include <set>
#include <map>
#include <stdio.h>
#include <dirent.h>
#include <ctime>
#include <mutex>
#include <math.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "NSWCalibration/CalibrationMath.h"
#include "NSWCalibration/CalibrationSca.h"
#include "NSWCalibration/PDOCalib.h"

//
//
nsw::PDOCalib::PDOCalib(std::string calibType){
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::PDOCalib::setup(std::string db){

  feconfigs = ReadPulsingConfig(db);

  ERS_INFO("Setting up things");

}

//void configure(std::vector<nsw::FEBConfig> &feconfigs, int i_tpdac){
void nsw::PDOCalib::configure(int i_tpdac){

  send_pulsing_configs(i_tpdac);
  ERS_INFO("Configuring"); 
  
}
//
void nsw::PDOCalib::unconfigure(){

  disable_pulser();
  ERS_INFO("Unconfiguring"); 

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    purely calibration related things
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//void nsw::PDOCalib::ReadPulsingConfig(std::string config_filename, std::vector<nsw::FEBConfig> &feconfigs, std::set<std::string> &fenames){
std::vector<nsw::FEBConfig> nsw::PDOCalib::ReadPulsingConfig(std::string config_filename){
//void nsw::PDOCalib::ReadPulsingConfig(std::string config_filename){

// in header put defibnitions of :  fenames<vector of strings>, feconfigs<set of nswconfigs> 

    nsw::ConfigReader reader1("json://" + config_filename);  //<----INJECT FILE FROM lxplus_input_data.json 

    try { 
      auto config1 = reader1.readConfig(); 
    } catch (std::exception & e) { 
      std::runtime_error("Configuration file is improperly assembled");
      exit(0); 
    } 
    //--------------------------------------------------------------------------------- 
    fenames = reader1.getAllElementNames(); 

   // Reading configuration for the first indicated entries of FULL SET of FEBs\n"; 
    for (auto & name : fenames) { 
       try { 
         feconfigs.emplace_back(reader1.readConfig(name)); 
         //feconfigs.emplace_back(reader1.readConfig(name)); 
       } catch (std::exception & e) { 
       std::cout << name << " - ERROR: Skipping this FE!" 
                 << " - Problem constructing configuration due to : [" << e.what() <<"]"<< std::endl; 
       } 
    }  

	  int nfebs = fenames.size();

//-------------- later should be an option to choose specific febs ---------------    
//		for(unsigned int l=0; l<fenames.size();l++)
//		{
//			if(fe_names_v[l].find(dw_layer)!=std::string::npos)
//			{
//				std::cout<<"found - "<<fe_names_v[l]<<std::endl;
//				nfebs++;
//			}
//			else{continue;}
//		}

    ERS_INFO("PDOCalib::RaedPulsingConfig[ "<<nfebs << "will be pulsed ]");
  
    return feconfigs;
}

//void nsw::PDOCalib::send_pulsing_configs(std::vector<nsw::FEBConfig> &feconfigs, std::set<std::string> &fenames, int tpdac){
void nsw::PDOCalib::send_pulsing_configs(int i_tpdac){

  for(unsigned int thrd=0; thrd<fenames.size(); thrd++){
//    nsw::PDOCalib * calib_ptr = new nsw::PDOCalib;
    //conf_threads[thrd] = std::thread(&nsw::PDOCalib::setup_pulses, calib_ptr, feconfigs, thrd, tpdac);
    conf_threads.push_back(std::thread(&nsw::PDOCalib::setup_pulses, this, thrd, i_tpdac));
  }
  sleep(2);
  for(unsigned int thrd=0; thrd<fenames.size(); thrd++){
    conf_threads[thrd].join();
  }
  sleep(2);
  conf_threads.clear();

}

void nsw::PDOCalib::disable_pulser(){

  for(unsigned int thrd=0; thrd<fenames.size(); thrd++){
//    nsw::PDOCalib * calib_ptr = new nsw::PDOCalib;
    //conf_threads[thrd] = std::thread(&nsw::PDOCalib::setup_pulses, calib_ptr, feconfigs, thrd, tpdac);
    conf_threads.push_back(std::thread(&nsw::PDOCalib::turn_off_pulses, this, thrd));
  }
  sleep(2);
  for(unsigned int thrd=0; thrd<fenames.size(); thrd++){
    conf_threads[thrd].join();
  }
  sleep(2);
  conf_threads.clear();
}

//void nsw::PDOCalib::setup_pulses(std::vector<nsw::FEBConfig> feconfigs, int which_feb, int tpdac_i)					
void nsw::PDOCalib::setup_pulses(int which_feb, int i_tpdac)					
																			
{
  nsw::ConfigSender cs;

  auto feb = feconfigs.at(which_feb); 
//--------- what board am i? ---------------------------------- 
	auto VMMS = feb.getVmms();
	int VmmSize = VMMS.size();

  int n_vmms = VmmSize;

  cs.sendRocConfig(feb);
  //-------------------------------------------------------------
 ///////////// setting appropriate pulser DAC for calibration //////////////////////////////////////////////////////
		for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) 
		{
//			if(debug){std::cout<<"\nINFO - "<<feb.getAddress()<< " VMM_"<<vmm_id<<"\t-> setting pulser DAC at ["<<tpdac_i<<"]"<<std::endl;}
 //     feb.getVmm(vmm_id).setGlobalThreshold((size_t)(thdac));
			feb.getVmm(vmm_id).setTestPulseDAC((size_t)(i_tpdac));
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 1);    //lets start with all of channels
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 0);    
      for(int channel=0; channel<64; channel++){
        if(channel % 2 != 0){
          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, channel);
          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, channel);
        }else{
          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 0, channel);
          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 1, channel);      
        }
       }
//			cs.sendVmmConfigSingle(feb,vmm_id);
		}		
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  auto &vmms = feb.getVmms();
  std::vector<unsigned> reset_orig;
  
  for(auto &vmm:vmms){
    reset_orig.push_back(vmm.getGlobalRegister("reset"));
    vmm.setGlobalRegister("reset",3);
  }
  cs.sendVmmConfig(feb);
  size_t i=0;
  for(auto &vmm: vmms){
    vmm.setGlobalRegister("reset", reset_orig[i++]);
  }
  cs.sendVmmConfig(feb);
  ERS_INFO("PDOCalib::setup_pulses - Configuration sent");
  //		for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) 
//		{
////			if(debug){std::cout<<"\nINFO - "<<feb.getAddress()<< " VMM_"<<vmm_id<<"\t-> setting pulser DAC at ["<<tpdac_i<<"]"<<std::endl;}
// //     feb.getVmm(vmm_id).setGlobalThreshold((size_t)(thdac));
//			feb.getVmm(vmm_id).setTestPulseDAC((size_t)(i_tpdac));
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 1);    //lets start with all of channels
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 0);    
//			
////			cs.sendVmmConfigSingle(feb,vmm_id);
//		}		
//	cs.sendVmmConfig(feb);
//	cs.sendVmmConfigSingle(feb,vmm_id);
}

void nsw::PDOCalib::turn_off_pulses(int which_feb)
{
  nsw::ConfigSender cs;

  auto feb = feconfigs.at(which_feb); 

//--------- what board am i? ---------------------------------- 
	auto VMMS = feb.getVmms();
	int VmmSize = VMMS.size();

	int n_vmms = VmmSize;
 
  cs.sendRocConfig(feb);
 //-------------------------------------------------------------

		for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) 
		{
//			if(debug){std::cout<<"\nINFO - "<<feb.getAddress()<< " VMM_"<<vmm_id<<"\t-> pulser diabled"<<std::endl;}
      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 0);    
      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 1);    

	//		cs.sendVmmConfigSingle(feb,vmm_id);
		}		

   auto &vmms = feb.getVmms();
  std::vector<unsigned> reset_orig;
  
  for(auto &vmm:vmms){
    reset_orig.push_back(vmm.getGlobalRegister("reset"));
    vmm.setGlobalRegister("reset",3);
  }
  cs.sendVmmConfig(feb);
  size_t i=0;
  for(auto &vmm: vmms){
    vmm.setGlobalRegister("reset", reset_orig[i++]);
  }
  cs.sendVmmConfig(feb);
  ERS_INFO("PDOCalib::disable_pulses - Configuration sent");

}

