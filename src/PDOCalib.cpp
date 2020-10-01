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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    purely calibration related things
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

nsw::PDOCalib::PDOCalib(std::string calibType){
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::PDOCalib::setup(std::string db){

  feconfigs = ReadPulsingConfig(db);

  ERS_INFO("PDOCalib::Reading frontend configuration");

}


void nsw::PDOCalib::configure(){

  ERS_INFO("PDOCalib::Configuring Frontends");
}

//void configure(std::vector<nsw::FEBConfig> &feconfigs, int i_tpdac){
void nsw::PDOCalib::configure(int i_par, bool pdo, bool tdo, int chan){

  send_pulsing_configs(i_par, pdo, tdo, chan);
  if(pdo)ERS_INFO("PDOCalib::Configuring test pulse amplitude"); 
  if(tdo)ERS_INFO("PDOCalib::Configuring test pulse delay"); 
}
//
void nsw::PDOCalib::unconfigure(){

  disable_pulser();
  ERS_INFO("PDOCalib::Unconfiguring"); 

}

std::vector<nsw::FEBConfig> nsw::PDOCalib::ReadPulsingConfig(std::string config_filename){

    ERS_INFO("PDOCalib::reading configuration file << " << config_filename);
    nsw::ConfigReader reader1(config_filename);  //<----INJECT FILE FROM lxplus_input_data.json 

    try { 
      auto config1 = reader1.readConfig();
    } catch (std::exception & e) { 
      std::runtime_error("PDOCalib::Configuration file is improperly assembled");
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
    ERS_INFO("PDOCalib::ReadPulsingConfig[ "<<nfebs << "will be pulsed ]");
  
    return feconfigs;
}

void nsw::PDOCalib::send_pulsing_configs(int i_par, bool pdo, bool tdo, int chan){

  for(unsigned int thrd=0; thrd<fenames.size(); thrd++){
    if(pdo){conf_threads.push_back(std::thread(&nsw::PDOCalib::setup_pulses, this, thrd, i_par, chan));}
    if(tdo){conf_threads.push_back(std::thread(&nsw::PDOCalib::setup_pulse_delay, this, thrd, i_par));}
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
    conf_threads.push_back(std::thread(&nsw::PDOCalib::turn_off_pulses, this, thrd));
  }
  sleep(2);
  for(unsigned int thrd=0; thrd<fenames.size(); thrd++){
    conf_threads[thrd].join();
  }
  sleep(2);
  conf_threads.clear();
}

void nsw::PDOCalib::setup_pulses(int which_feb, int i_tpdac, int pulse_this_chan)					
																			
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

//     feb.getVmm(vmm_id).setGlobalThreshold((size_t)(thdac));
//			feb.getVmm(vmm_id).setTestPulseDAC((size_t)(i_tpdac));
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 1);    //lets start with all of channels
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 0);    
///////////// code  below for pulsing odd ro even channel sets ////////////////////////////
//      for(int channel=0; channel<64; channel++){
//        if(channel % 2 != 0){
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, channel);
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, channel);
//        }else{
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 0, channel);
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 1, channel);      
//        }
//       }
////////////////////////////////////////////////////////////////////////////////////////////
      for(int channel=0; channel<64; channel++){
        if(channel == pulse_this_chan){
          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, channel);
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, channel);
        }else{
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 0, channel);
          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 1, channel);      
        }
       }
    
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
  ERS_INFO("PDOCalib::setup_pulses() - Configuration sent");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
  ERS_INFO("PDOCalib::disable_pulses() - Configuration sent");

}
///////////////////////////////////////////////////////////////////////////////////////
/////////////////////// TDO calibration ///////////////////////
//////////////////////////////////////////////////////////////////////////////////////
void nsw::PDOCalib::setup_pulse_delay(int which_feb, int i_delay)
{
  nsw::ConfigSender cs;

  auto feb = feconfigs.at(which_feb); 
//--------- what board am i? ---------------------------------- 
	auto VMMS = feb.getVmms();
	int VmmSize = VMMS.size();

  int n_vmms = VmmSize;
//------------- tryna configure ROC ----------------------------------------------------------
  auto & roc_analog = feb.getRocAnalog();
//////////////// setting gain, puilse height, peak time, tac slope //////////////////////////
  for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) 
	{
//			if(debug){std::cout<<"\nINFO - "<<feb.getAddress()<< " VMM_"<<vmm_id<<"\t-> pulser diabled"<<std::endl;}
    feb.getVmm(vmm_id).setGlobalRegister("stc",0); // TAC slope (60 ns for now)
    feb.getVmm(vmm_id).setGlobalRegister("st",3); // peak time (50 ns)
    feb.getVmm(vmm_id).setGlobalRegister("sg",5); // gain (9 mV/fC)
    feb.getVmm(vmm_id).setTestPulseDAC(400); 
    feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 1);    
    feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 0);    
	//		cs.sendVmmConfigSingle(feb,vmm_id);
	}		
///////////// this sets delay for vmms 0 to 3 ///////////////////////////
  roc_analog.setRegisterValue("reg073ePllVmm0","tp_phase_0",i_delay);
  roc_analog.setRegisterValue("reg073ePllVmm0","tp_phase_1",i_delay);
  roc_analog.setRegisterValue("reg074ePllVmm0","tp_phase_2",i_delay);
  roc_analog.setRegisterValue("reg074ePllVmm0","tp_phase_3",i_delay);
//////////// delaying vmms 4 to 7 ///////////////////
  roc_analog.setRegisterValue("reg089ePllVmm1","tp_phase_0",i_delay);
  roc_analog.setRegisterValue("reg089ePllVmm1","tp_phase_1",i_delay);
  roc_analog.setRegisterValue("reg090ePllVmm1","tp_phase_2",i_delay);
  roc_analog.setRegisterValue("reg090ePllVmm1","tp_phase_3",i_delay);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cs.sendRocConfig(feb);

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
  ERS_INFO("PDOCalib::setup_pulse_delay() - ROC Configuration sent");

}

