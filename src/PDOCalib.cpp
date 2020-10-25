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
void nsw::PDOCalib::configure(int i_par, bool pdo, bool tdo, int chan, bool all_chan){
  try{
    send_pulsing_configs(i_par, pdo, tdo, chan, all_chan);
    if(pdo)ERS_INFO("PDOCalib::Configuring test pulse amplitude"); 
    if(tdo)ERS_INFO("PDOCalib::Configuring test pulse delay"); 
  }catch(std::exception &e){
     ERS_INFO("PDOCalib::configure - Found problem:"<<e.what());
  }
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
       try {// in case chan-by-chan pulsing does not work - select few mmfe and pulse separately 
          //if(name.find("L2")!=std::string::npos){
//          if((name.find("L1")!=std::string::npos) and (name.find("HO")!=std::string::npos)){
            feconfigs.emplace_back(reader1.readConfig(name));
            ERS_INFO("Including config for: "<<name);
//          }else{
//          continue;
//          } 
         //feconfigs.emplace_back(reader1.readConfig(name)); 
       } catch (std::exception & e) { 
       //std::cout << name << " - ERROR: Skipping this FE!" 
       //          << " - Problem constructing configuration due to : [" << e.what() <<"]"<< std::endl; 
          std::runtime_error("Configuration construction failed");
         ERS_INFO("PDOCalib::ReadPulsingConfig - "<<e.what()); 
       } 
    }  

	  //int nfebs = fenames.size();
	  int nfebs = feconfigs.size();
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
    ERS_INFO("PDOCalib::ReadPulsingConfig [ "<<nfebs << " ] will be pulsed");
  
    return feconfigs;
}

void nsw::PDOCalib::send_pulsing_configs(int i_par, bool pdo, bool tdo, int chan, bool all_chan){

//    ERS_INFO("feconfig size (nr of FEBS to pulse) - "<<feconfigs.size());
  try{
    for(unsigned int thrd=0; thrd<feconfigs.size(); thrd++){
      if(pdo){conf_threads.push_back(std::thread(&nsw::PDOCalib::setup_pulses, this, thrd, i_par, chan, all_chan));}
      if(tdo){
        conf_threads.push_back(std::thread(&nsw::PDOCalib::setup_pulse_delay, this, thrd, i_par, chan, all_chan));
        ERS_INFO("Sent out config for feb nr "<<thrd);
      }
//    usleep(100e3);
//     if(thrd==99){sleep(5);}
    }
  }catch(std::exception &e){
     std::string errmsg = e.what();
     throw std::runtime_error("PDOCalib::send_pulsing_configs::ERROR - "+errmsg);
  }
        ERS_INFO("Config sending loop ended");
//  sleep(2);
  for(unsigned int thrd=0; thrd<feconfigs.size(); thrd++){
    conf_threads[thrd].join();
  }
//  sleep(2);
        ERS_INFO("threads joined");
  
  conf_threads.clear();
}

void nsw::PDOCalib::disable_pulser(){

  for(unsigned int thrd=0; thrd<feconfigs.size(); thrd++){
    conf_threads.push_back(std::thread(&nsw::PDOCalib::turn_off_pulses, this, thrd));
  }
  sleep(2);
  for(unsigned int thrd=0; thrd<feconfigs.size(); thrd++){
    conf_threads[thrd].join();
  }
  sleep(2);
  conf_threads.clear();
}

void nsw::PDOCalib::setup_pulses(int which_feb, int i_tpdac, int pulse_this_chan, bool all_chan)					
																			
{
  std::string yep = "true";
  std::string nope = "false";
  std::string msg;
  if(all_chan!=0){msg = yep;}
  else{msg=nope;}
  ERS_INFO("CHECK: all_chan var = "<<msg);
  nsw::ConfigSender cs;

  auto feb = feconfigs.at(which_feb); 
//--------- what board am i? ---------------------------------- 
	auto VMMS = feb.getVmms();
	int VmmSize = VMMS.size();

  int n_vmms = VmmSize;

  cs.sendRocConfig(feb);
 
  //-------------------------------------------------------------
 ///////////// setting appropriate pulser DAC for calibration //////////////////////////////////////////////////////

    for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++){
//      feb.getVmm(vmm_id).setGlobalThreshold((size_t)(thdac));
			feb.getVmm(vmm_id).setTestPulseDAC((size_t)(i_tpdac));
      if(all_chan){
        feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 1);    //lets start with all of channels
        feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 0);    
      }
      if(!all_chan){
        for(int channel=0; channel<64; channel++){
          if(channel == pulse_this_chan){
           feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, channel);
           feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, channel);
         }else{
           feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 0, channel);
           feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 1, channel);      
         }
        }
       }
      else{
        std::runtime_error("PDOCalib::setup_pulses - none of the channel pulsing options were applied");
        exit(0);
       }
     
/////////// code  below for pulsing odd ro even channel sets ////////////////////////////
//      for(int channel=0; channel<64; channel++){
//        if(channel % 2 != 0){
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, channel);
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, channel);
//        }else{
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 0, channel);
//          feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 1, channel);      
//        }
//      } 
////////////////////////////////////////////////////////////////////////////////////////////
   }// vvm loop ends
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
void nsw::PDOCalib::setup_pulse_delay(int which_feb, int i_delay, int chan, bool all_chan)
{
  std::string yep = "true";
  std::string nope = "false";
  std::string msg;
  if(all_chan!=0){msg = yep;}
  else{msg=nope;}
  ERS_INFO("CHECK: all_chan var = "<<msg);
 
  //  ERS_INFO("a0.1");
  nsw::ConfigSender cs;

//  ERS_INFO("a");
  ERS_INFO("Config Params: ifeb "<<which_feb<<" - idelay "<<i_delay<<" - chan "<<chan<<" - ALLch_flag"<<all_chan);
  auto feb = feconfigs.at(which_feb); 
//--------- what board am i? ---------------------------------- 
	auto VMMS = feb.getVmms();
	int VmmSize = VMMS.size();

  int n_vmms = VmmSize;
//------------- tryna configure ROC ----------------------------------------------------------
  auto & roc_analog = feb.getRocAnalog();
  //  ERS_INFO("b");
//////////////// setting gain, puilse height, peak time, tac slope //////////////////////////
  for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) 
	{
//			if(debug){std::cout<<"\nINFO - "<<feb.getAddress()<< " VMM_"<<vmm_id<<"\t-> pulser diabled"<<std::endl;}
    feb.getVmm(vmm_id).setGlobalThreshold((size_t)(180));
  	feb.getVmm(vmm_id).setGlobalRegister("stc",0); // TAC slope (60 ns for now)
  	feb.getVmm(vmm_id).setGlobalRegister("st",1); // peak time (50 ns)
//    feb.getVmm(vmm_id).setGlobalRegister("sg",5); // gain (9 mV/fC)
    feb.getVmm(vmm_id).setTestPulseDAC(350); 
    //if(all_chan==true){
    if(all_chan){
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_st", 1);    
//      feb.getVmm(vmm_id).setChannelRegisterAllChannels("channel_sm", 0);    
      for(int i=0; i<64; i++){
         feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, i);
         feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, i);
      }
    }
    else if(!all_chan){ //single channel pulsing while masking all other channels
      for(int ch=0; ch<64; ch++){
       if(ch==chan){
         feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 1, ch);
         feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 0, ch);
       }else{
         feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_sm", 1, ch);
         feb.getVmm(vmm_id).setChannelRegisterOneChannel("channel_st", 0, ch);
       }
      }
    }
    else{ERS_INFO("Huston we have a problem "<< all_chan << feb.getAddress());}
  }// vmm loop ends	
//  ERS_INFO("c");
///////////// this sets delay for vmms 0 to 3 ///////////////////////////
//  roc_analog.setRegisterValue("reg073ePllVmm0","tp_phase_0",i_delay);
//  roc_analog.setRegisterValue("reg073ePllVmm0","tp_phase_1",i_delay);
//  roc_analog.setRegisterValue("reg074ePllVmm0","tp_phase_2",i_delay);
//  roc_analog.setRegisterValue("reg074ePllVmm0","tp_phase_3",i_delay);
////////////// delaying vmms 4 to 7 ///////////////////
//  roc_analog.setRegisterValue("reg089ePllVmm1","tp_phase_0",i_delay);
//  roc_analog.setRegisterValue("reg089ePllVmm1","tp_phase_1",i_delay);
//  roc_analog.setRegisterValue("reg090ePllVmm1","tp_phase_2",i_delay);
//  roc_analog.setRegisterValue("reg090ePllVmm1","tp_phase_3",i_delay);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  roc_analog.setRegisterValue("reg119","tp_phase_global",i_delay); //apparently sets delay(3ns/val[0,7]) for all vmms at once, try later
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cs.sendRocConfig(feb);
///////// reseting vmms ////////////////////////
  auto &vmms = feb.getVmms();
  std::vector<unsigned> reset_orig;
  
//  ERS_INFO("d");
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

