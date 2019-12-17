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

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "include/CalibrationMath.h"
#include "include/CalibrationSca.h"

nsw::CalibrationSca::CalibrationSca(){
}

//------------- function for the particular processes -----------------------------------
void nsw::CalibrationSca::read_config(std::string  config_filename, 
													 std::string fe_name,
													 bool full_set,
                           std::set<std::string> &frontend_names, 
                           std::vector<std::string> &fe_names_v, 
                           std::vector<nsw::FEBConfig> & frontend_configs)
  { 
 //-------------- reads the configuratin json file ----------------------------------   
		 nsw::ConfigReader reader1("json://" + config_filename); 	//<----INJECT FILE FROM lxplus_input_data.json 
     try { 
       auto config1 = reader1.readConfig(); 
     } catch (std::exception & e) { 
       std::cout << "Make sure the json is formed correctly. " 
                 << "Can't read config file due to : " << e.what() << std::endl; 
       std::cout << "Exiting..." << std::endl; 
       exit(0); 
     } 
 //--------------------------------------------------------------------------------- 
    frontend_names = reader1.getAllElementNames(); 
 //----------- pushing set into vector to sort FEB names by their serial numbers -------------------------
		fe_names_v.assign(frontend_names.begin(), frontend_names.end());
		std::sort(fe_names_v.begin(),fe_names_v.end());
 //---------------- if there are no specific board entry do following ----------------------------------------------------	
		if(full_set){
     std::cout << "\n Reading configuration for the first indicated entries of FULL SET of FEBs\n"; 
     std::cout <<   "============================================================================\n"; 
     for (auto & name : frontend_names) { 
       try { 
         frontend_configs.emplace_back(reader1.readConfig(name)); 
       } catch (std::exception & e) { 
         std::cout << name << " - ERROR: Skipping this FE!" 
                   << " - Problem constructing configuration due to : [" << e.what() <<"]"<< std::endl; 
       } 
       // frontend_configs.back().dump(); 
     }
		}
	//---------- if board "integer names" are specified fill config vector from sorted FEB name vector ----------------------------
		else{
		 std::cout << "\n Reading configuration for the specified FEBs\n"; 
     std::cout <<   "=======================================================\n"; 
     for (auto & name : fe_names_v) { 
       try { 
         frontend_configs.emplace_back(reader1.readConfig(name)); 
       } catch (std::exception & e) { 
         std::cout << name << " - ERROR: Skipping this FE!" 
                   << " - Problem constructing configuration due to : [" << e.what() <<"]"<<std::endl; 
       } 
       // frontend_configs.back().dump(); 
     }
		}	 
  }

////-----------------------------------------------------------------------------------------
void nsw::CalibrationSca::configure_feb(std::vector<nsw::FEBConfig>  frontend_configs, int fe_name_sorted)
  {
		try{
			nsw::ConfigSender cs;
			auto feb = frontend_configs.at(fe_name_sorted);
			cs.sendRocConfig(feb); //configure roc
			cs.sendVmmConfig(feb); //configure vmm
		}catch(std::exception & e){std::cout<<"ERROR on thread: ["<<e.what()<<"]"<<std::endl;}
	}



std::vector<float> nsw::CalibrationSca::read_baseline(
				nsw::ConfigSender &cs,
				nsw::FEBConfig &feb,
				nsw::CalibrationMath &cm, 
				int i_vmm,
				int channel_id,
				int n_samples,
				std::vector<float> & fe_samples_tmp,
				std::map< std::pair< std::string,int>, float> & channel_baseline_med,
				std::map< std::pair< std::string,int>, float> & channel_baseline_rms,
				bool debug,
				int RMS_CUTOFF
				)
	{
	for(int i_try=0; i_try<5; i_try++)
	{
		try
		{
    feb.getVmm(i_vmm).setMonitorOutput(channel_id, nsw::vmm::ChannelMonitor);
    feb.getVmm(i_vmm).setChannelMOMode(channel_id, nsw::vmm::ChannelAnalogOutput);
    auto results = cs.readVmmPdoConsecutiveSamples(feb, i_vmm, n_samples*10);

    // calculate channel level baseline median, rms
    float sum    = std::accumulate(results.begin(), results.end(), 0.0);
    float mean   = sum / results.size();
    float stdev  = cm.take_rms(results, mean);
    float median = cm.take_median(results);

    std::pair<std::string,int> feb_ch(feb.getAddress(),channel_id);
    // add medians, baseline to (MMFE8, CH) map
    channel_baseline_med[feb_ch] = median;
    channel_baseline_rms[feb_ch] = stdev;
    if (debug)
      std::cout << "INFO - "      << feb.getAddress()
                << " VMM_"       << i_vmm
                << ", CH " << channel_id
                << " : [mean = "  << cm.sample_to_mV(mean)
                << "], [stdev = " << cm.sample_to_mV(stdev)
                << "]"	<<std::endl;
	  
		for (unsigned int i = 0; i < results.size(); i++)
		{
			fe_samples_tmp.push_back(results[i]);
		} //if gives crap data revert back to just filling all stuff inside!
		break;// <<<<<<<<<<<<<<<<without break will read 5 time same channel!!!!!>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ATENTION IMPORTANT NOTE TO CHECK!!!!!!!!!!!
 		}catch(std::exception &e)
		{
			std::cout<<"\nERROR - "<<feb.getAddress()<<" VMM "<<i_vmm<<" channel "<<channel_id<<" disobeys, reason =>> ["<<e.what()<<"]"<<std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
	}
	return fe_samples_tmp;	
	}


int nsw::CalibrationSca::calculate_thdac_value(nsw::ConfigSender & cs,
                          nsw::FEBConfig & feb,
													nsw::CalibrationMath &cm,
                          int vmm_id,
                          int n_samples,
                          int thdac_central_guess,
													std::tuple<std::string, float, float> &thdac_constants,
                          std::vector<int> & thdac_guess_variations,
													bool debug)
{
		
		std::vector<float> thdac_guesses_sample;

	  for (unsigned int i = 0; i < thdac_guess_variations.size(); i++)
		{
			try{	
	    feb.getVmm(vmm_id).setMonitorOutput(nsw::vmm::ThresholdDAC, nsw::vmm::CommonMonitor);
	    feb.getVmm(vmm_id).setGlobalThreshold((size_t)(thdac_guess_variations[i]));
	    auto all_results = cs.readVmmPdoConsecutiveSamples(feb, vmm_id, n_samples*2);

			float thdac_med = cm.take_median(all_results);
			float thdac_diff;
//------------ cleaning result vector from high deviating samples ----------------------
			std::vector<short unsigned int> results;		
			for(auto & r : all_results)
			{
				thdac_diff = fabs(thdac_med - r);
				if(thdac_diff > 50)//approx 15-20 mV
				{
					if(debug){std::cout<<"sample strongly deviates from THDAC median value "<< thdac_med<<std::endl;}
					continue;
				}
				else{results.push_back(r);}			
			}
//----------------------------------------------------------------------------------
	    float sum = std::accumulate(results.begin(), results.end(), 0.0);
	    float mean = sum / results.size();
	    thdac_guesses_sample.push_back(mean);
	    if (debug)
	      	std::cout << "INFO "
	          << feb.getAddress()
	          << " vmm" << vmm_id
	          << ", thdac " << thdac_guess_variations[i]
	          << ", thdac (mV) " << cm.sample_to_mV(mean)
	          << std::endl;
			
			}
			catch(std::exception &e){std::cout<<"ERROR -  while calculating "<<feb.getAddress()<<" VMM_"<<vmm_id<<" THDAC, reason: ["<<e.what()<<"]"<<std::endl;}
			}
		  // do fit to a line
		  float thdac_guess_mean = std::accumulate(thdac_guess_variations.begin(), thdac_guess_variations.end(), 0.0)/thdac_guess_variations.size();
		  float thdac_guess_sample_mean = std::accumulate(thdac_guesses_sample.begin(), thdac_guesses_sample.end(), 0.0)/thdac_guesses_sample.size();
		
		  float num = 0.;
		  float denom = 0.;
		  for (unsigned int i = 0; i < thdac_guess_variations.size(); i++){
		    num += (thdac_guess_variations[i]-thdac_guess_mean) * (thdac_guesses_sample[i]-thdac_guess_sample_mean);
		    denom += pow((thdac_guess_variations[i]-thdac_guess_mean),2);
		  }
		
		  float thdac_slope = num/denom;
		  float thdac_intercept = thdac_guess_sample_mean - thdac_slope * thdac_guess_mean;
				
			thdac_constants = std::make_tuple(feb.getAddress(), thdac_slope, thdac_intercept);
			
			if(thdac_slope > 2.2 or thdac_slope < 1.8){std::cout<<"\nINFO - "<<feb.getAddress()<<" VMM_"<<vmm_id<<" THDAC calculation yields: \n\t[slope = "<<thdac_slope<<"] [offset = "<<thdac_intercept<<"]"<<std::endl;}
			else{std::cout<<"INFO - "<<feb.getAddress()<<" VMM "<<vmm_id<<" DAC slope ok! [approx 2 DAC/mV]"<<std::endl;}
		  // (y-b) / m = x
		  int thdac = (cm.mV_to_sample(thdac_central_guess) - thdac_intercept)/thdac_slope;
			return thdac;

}


std::pair<float,int> nsw::CalibrationSca::find_linear_region_slope(nsw::ConfigSender & cs,
                            nsw::FEBConfig & feb,
                   					nsw::CalibrationMath &cm,
									          int vmm_id,
                            int channel_id,
                            int thdac,
                            int tpdac,
                            int n_samples,
                            float ch_baseline_med,
                            float ch_baseline_rms,
                            float & tmp_min_eff_threshold,
                            float & tmp_mid_eff_threshold,
                            float & tmp_max_eff_threshold,
                            int & nch_base_above_thresh,
                          	float & first_trim_slope,
														bool bad_bl,
														bool debug,
														int trim_hi,
														int trim_mid,
														int trim_lo
														/*int trim_hi = nsw::ref_val::TrimHi,
														int trim_mid = nsw::ref_val::TrimMid,
														int trim_lo = nsw::ref_val::TrimLo	
                            int trim_mid = TRIM_MID,
                            int trim_lo = TRIM_LO*/
														)
{

	for(int itry=0; itry<5; itry++){
		try{
					
      if (trim_hi <= trim_mid) return std::make_pair(0,0);
      if (trim_mid <= trim_lo) return std::make_pair(0,0);

			int factor;
			if(!bad_bl){factor = 1;}
			else{factor = 2;}
      float channel_mid_eff_thresh=0., channel_max_eff_thresh=0., channel_min_eff_thresh=0.;

      // loop through trims to get full range, then then middle of range
      std::vector<int> trims;

      trims.push_back(trim_lo );
      trims.push_back(trim_hi );
      trims.push_back(trim_mid);

      for (auto trim : trims) {

	      feb.getVmm(vmm_id).setMonitorOutput  (channel_id, nsw::vmm::ChannelMonitor);
 	      feb.getVmm(vmm_id).setChannelMOMode  (channel_id, nsw::vmm::ChannelTrimmedThreshold);
 	      feb.getVmm(vmm_id).setChannelTrimmer (channel_id, (size_t)(trim));
 	      feb.getVmm(vmm_id).setGlobalThreshold((size_t)(thdac));
 	      auto results = cs.readVmmPdoConsecutiveSamples(feb, vmm_id, n_samples*factor);
//--------------------------------------------------------------------------------------------------------			
//        float sum = std::accumulate(results.begin(), results.end(), 0.0);
//        float mean = sum / results.size();
//        float stdev = take_rms(results,mean);
        float median = cm.take_median(results);

        // calculate "signal" --> threshold - channel
        float eff_thresh = median - ch_baseline_med;

        if (trim == trim_mid){
          channel_mid_eff_thresh = eff_thresh;

				//------- check for the negative eff thr value ---------------------
					if(channel_mid_eff_thresh<0)
					{
						nch_base_above_thresh++;
						if(debug){std::cout<<feb.getAddress()<<" VMM_"<<vmm_id<<" chan - "<<channel_id<<" has neg. effective thr. = "<<channel_mid_eff_thresh<<std::endl;}
					}
				//------------------------------------------------------------------
        }
        else if (trim == trim_lo){
          channel_max_eff_thresh = eff_thresh;
        }
        else if (trim == trim_hi){
          channel_min_eff_thresh = eff_thresh;
        }
      } // end of trim loop
		/// highest trimmer value corresponds to the lowest set threshold! mc

      float min = channel_min_eff_thresh;
      float mid = channel_mid_eff_thresh;
      float max = channel_max_eff_thresh;

      tmp_min_eff_threshold = min;
      tmp_mid_eff_threshold = mid;
      tmp_max_eff_threshold = max;

      std::pair<float,float> slopes = cm.get_slopes(min,mid,max,trim_hi,trim_mid,trim_lo);
      float m1 = slopes.first;
      float m2 = slopes.second;
      float avg_m = (m1+m2)/2.;
			first_trim_slope = m1;
		
      if(debug) 
        std::cout << "INFO - " << feb.getAddress() 
                  << " VMM_"   << vmm_id 
                  << " CH " << channel_id 
                  << " TRIM " << trim_hi 
                  << " - { [avg_m " << avg_m 
                  << "], [m1 " << m1 
                  << "], [m2 " << m2 
                  << "] }" <<std::endl;

			float slope_check_val = nsw::ref_val::SlopeCheck;
      if (!cm.check_slopes(m1,m2,slope_check_val)){
        return find_linear_region_slope(cs,
                            feb,
														cm,
                            vmm_id,
                            channel_id,
                            thdac,
                            tpdac,
                            n_samples,
                            ch_baseline_med,
                            ch_baseline_rms,
                            tmp_min_eff_threshold,
                            tmp_mid_eff_threshold,
                            tmp_max_eff_threshold,
                            nch_base_above_thresh,
														first_trim_slope,		
                						bad_bl,
														debug,
								            trim_hi-2,
                            trim_mid,
                            trim_lo
                            );
      }
      // Got to a consistent set of two slopes! ///////// if statement below should go outside the loop function ///////////////
    return std::make_pair(avg_m,trim_hi);
		break; //break loop if ok
		}catch(std::exception &e){
			std::cout<<"[ERROR] - Failed to calculate "<<feb.getAddress()<<" VMM_"<<vmm_id<<" channel "<<channel_id<<" linear region, reason: ["<<e.what()<<"]"<<std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	}//try loop end
}


std::vector<float> nsw::CalibrationSca::vmm_averages
	(nsw::ConfigSender &cs,
	nsw::FEBConfig &feb,
	int i_vmm,
	int channel_id,
	int n_samples,
	int trim_mid,
	int thdac,
	std::vector<float> & fe_samples_tmp
	)
	{
		for(int i_try=0; i_try<5; i_try++){
			try{
			   std::pair<std::string,int> feb_ch(feb.getAddress(),channel_id);
		     
		     feb.getVmm(i_vmm).setMonitorOutput  (channel_id, nsw::vmm::ChannelMonitor);
		     feb.getVmm(i_vmm).setChannelMOMode  (channel_id, nsw::vmm::ChannelTrimmedThreshold);
		     feb.getVmm(i_vmm).setChannelTrimmer (channel_id, (size_t)(trim_mid));
		     feb.getVmm(i_vmm).setGlobalThreshold((size_t)(thdac));
		     auto results = cs.readVmmPdoConsecutiveSamples(feb, i_vmm, n_samples*10);
		     // add samples to the vector for a given fe
		     for (unsigned int i = 0; i < results.size(); i++) {
		       fe_samples_tmp.push_back((float)(results[i]));
		     }
				break;
				}catch(std::exception &e){
					std::cout<<"\nERROR - Couldn`t read "<<feb.getAddress()<<" VMM_"<<i_vmm<<" channel "<<channel_id<<", reason: ["<<e.what()<<"]"<<std::endl;
					std::this_thread::sleep_for(std::chrono::milliseconds(30));
		//			throw;
			}
		}
	return fe_samples_tmp;
	}

std::map< std::pair< std::string,int>, int>  nsw::CalibrationSca::analyse_trimmers
			(nsw::ConfigSender &cs,
			nsw::FEBConfig &feb,
			nsw::CalibrationMath &cm,
			int i_vmm,
			int channel_id,
			int n_samples,
			int TRIM_MID,
			std::map<std::pair<std::string, int>, float> & channel_trimmed_thr,
			int thdac_i,
			std::map< std::pair< std::string,int>, float> & channel_baseline_med,
			std::map< std::pair< std::string,int>, float> & channel_baseline_rms,
			std::map< std::string, float> & vmm_baseline_med,
			std::map< std::string,float> & vmm_mid_eff_thresh,
			std::map< std::pair< std::string,int>, float> & channel_mid_eff_thresh,
			std::map< std::pair< std::string,int>, float> &channel_eff_thresh_slope,
			std::map< std::pair< std::string,int>, float> &channel_trimmer_max,
			std::map< std::pair< std::string,int>, float> & eff_thr_w_best_trim,	
			std::vector<int> & channel_mask,
			std::map<std::pair<std::string,int>,int> & DAC_to_add,
			std::map< std::pair< std::string,int>, int> & best_channel_trim,
//			std::vector<float> & trim_perf,
			bool recalc,
			bool debug
			)
		{

	for(int i_try=0; i_try<5; i_try++)
	{
		try{
	        std::pair<std::string,int> feb_ch(feb.getAddress(),channel_id);
		
					bool mask=false;
					bool slope_ok;
		
					if(channel_mask.at(channel_id)!=0)
					{
						mask=true;
						if(debug)std::cout<<"\n VMM_"<<i_vmm<<" Channel "<<channel_id<<" is masked"<<std::endl;
					}
	        // again check if channel is sensible
	        if (fabs(channel_eff_thresh_slope[feb_ch]) < pow(10,-9.)) {
	      		printf("\n %s VMM_%i channels` %i slope is not usefull - %f",feb.getAddress().c_str(), i_vmm, channel_id, channel_eff_thresh_slope[feb_ch]);//temporary
				   	slope_ok = false;
	        }else{slope_ok = true;}
	
	        float ch_baseline_rms = channel_baseline_rms[feb_ch];
	        float ch_baseline_med = channel_baseline_med[feb_ch];
	        if (!cm.check_channel(ch_baseline_med, ch_baseline_rms, vmm_baseline_med[feb.getAddress()]))
					{
	       		printf("\n %s VMM_%i channel %i is noisy - %f mV RMS", feb.getAddress().c_str(), i_vmm, channel_id, ch_baseline_rms);//temporary	
					}
        // get desired trim value
	        float delta = channel_mid_eff_thresh[feb_ch] - vmm_mid_eff_thresh[feb.getAddress()];
	  
					if(slope_ok == true)
					{
			  	  int trim_guess = TRIM_MID + std::round(delta / channel_eff_thresh_slope[feb_ch]);
	   	 
						if(debug){std::cout<<"\nINFO - "<<feb.getAddress()<<" VMM_"<<i_vmm<<" CH - "<<channel_id
																						<<" >>> {ch_slope = "<<channel_eff_thresh_slope[feb_ch]
																						<<"}{delta = "<<delta<<"}{trim guess = "<<trim_guess<<"}"<<std::endl;}  
		     
					  best_channel_trim[feb_ch] = trim_guess > channel_trimmer_max[feb_ch] ? channel_trimmer_max[feb_ch] : trim_guess;
	 	        best_channel_trim[feb_ch] = best_channel_trim[feb_ch] < 0            ? 0 : best_channel_trim[feb_ch];
		
						if(debug){std::cout<<"INFO - "<<feb.getAddress()<<" VMM_"<<i_vmm<<" CH - "<<channel_id<<" >> best trim = {"<<best_channel_trim[feb_ch]<<"}"<<std::endl;}  
					}
					else
					{
						best_channel_trim[feb_ch] = TRIM_MID - 7;
						if(debug){std::cout<<"Setting trimmer bit to - 7 "<<std::endl;}// middle of usually linear trimmer part - temporary measure
					}
	        feb.getVmm(i_vmm).setMonitorOutput  (channel_id, nsw::vmm::ChannelMonitor);
	        feb.getVmm(i_vmm).setChannelMOMode  (channel_id, nsw::vmm::ChannelTrimmedThreshold);
	        feb.getVmm(i_vmm).setChannelTrimmer (channel_id, (size_t)(best_channel_trim[feb_ch]));
	        feb.getVmm(i_vmm).setGlobalThreshold((size_t)(thdac_i));
	        auto results = cs.readVmmPdoConsecutiveSamples(feb, i_vmm, n_samples); // REMOVE multiplier!!
	
	        float median = cm.take_median(results);
	        float eff_thresh = median - channel_baseline_med[feb_ch];
//					trim_perf.push_back(eff_thresh);					

					if(debug){std::cout<<"INFO - "<<feb.getAddress()<<" VMM_"<<i_vmm<<" channel "<<channel_id<<" - THR(eff) = "<<eff_thresh<<" ADC"<<std::endl;}

					
					if(eff_thresh<0 and mask==true)
					{
						std::cout<<"\nINFO - "<<feb.getAddress()<<" VMM_"<<i_vmm<<" channel "<<channel_id<<"with eff_thr<0 is masked - ignore"<<std::endl;
					}	
					if(eff_thresh<0 and mask==false)
					{
					 	if(!recalc and fabs(eff_thresh) <= 55){DAC_to_add[feb_ch] = fabs(eff_thresh);} //cutting of around 20mV //if neg eff thr is less than approx 30mV, raise the thdac
						else{
							DAC_to_add[feb_ch] = fabs(eff_thresh);
							channel_mask.at(channel_id) = 1;
							std::cout<<feb.getAddress()<<" VMM_"<<i_vmm<<" - masked in addition [CH - "<<channel_id<<"]"<<std::endl;
						}
					}
					eff_thr_w_best_trim[feb_ch] = eff_thresh;
					
					channel_trimmed_thr[feb_ch] = median;

				break;//breaks the loop if everything is ok		
				}
				catch(std::exception & e)
				{
					std::cout<<"\nERROR - ["<<feb.getAddress()<<" VMM "<<i_vmm<<" channel "<<channel_id<<"] disobeys, reason =>>"<<e.what()<<std::endl;
					std::this_thread::sleep_for(std::chrono::milliseconds(40));
				}
		}	
	return best_channel_trim; 
	}


void nsw::CalibrationSca::sca_calib( std::string config_filename,
								std::vector<nsw::FEBConfig> frontend_configs, // if here i use type "auto" for frontend_configs -> get overloaded function call error from cmake!
								std::string fe_name,
								std::string io_config_path,
								int fe_name_sorted,
								int n_samples,
								bool pFEB,
								bool debug,
								int rms_factor)
{
  auto start = std::chrono::high_resolution_clock::now();	
	
	int tpdac =-1;

	int NCH_PER_VMM = nsw::ref_val::NchPerVmm;
	int BASELINE_CUTOFF = nsw::ref_val::BaselineCutoff;
	int RMS_CUTOFF = nsw::ref_val::RmsCutoff;
	int TRIM_LO = nsw::ref_val::TrimLo;
	int TRIM_MID = nsw::ref_val::TrimMid;
	int TRIM_HI = nsw::ref_val::TrimHi;

//------------------ main business starts here !----------------------
  nsw::ConfigSender cs;
	nsw::CalibrationMath cm;
//----------------------------------------------------------------
  std::map< std::pair< std::string,int>, float> channel_baseline_med;
  std::map< std::pair< std::string,int>, float> channel_baseline_rms;

  std::map< std::string, float> vmm_baseline_med;
  std::map< std::string, float> vmm_baseline_rms;

  int offset_center = 14; // center the threshold in the middle of the window

  std::vector <float> fe_samples_tmp;
  std::vector <float> fe_samples_pruned;

  // first read baselines

  // calculate thdac
  std::map<std::string,int> thdacs;
  std::map<std::string,float> thdacs_sample;

  // then, take trimmer values

  std::map< std::pair< std::string,int>, float> channel_max_eff_thresh;
  std::map< std::pair< std::string,int>, float> channel_min_eff_thresh;
  std::map< std::pair< std::string,int>, float> channel_mid_eff_thresh;

  std::map< std::pair< std::string,int>, float> channel_max_eff_thresh_err;
  std::map< std::pair< std::string,int>, float> channel_min_eff_thresh_err;
  std::map< std::pair< std::string,int>, float> channel_mid_eff_thresh_err;

  std::map< std::string,float > vmm_mid_eff_thresh;
  std::map< std::pair< std::string,int>, float> channel_eff_thresh_slope;
  std::map< std::pair< std::string,int>, float> channel_trimmer_max;

	std::string hl_rms="_x"+std::to_string(rms_factor);

//------------- initializing prees for json file generation ---------------------------------
	namespace pt = boost::property_tree;
	
//--------------------------------------------------------------------------
	pt::ptree input_data;
	pt::read_json(io_config_path, input_data);
//-------------------------------------------------------------------------
	pt::ptree out_json_l1;
	pt::ptree out_json_l2;

  std::string out_chan_folder = input_data.get<std::string>("cal_data_output");
//------------- next 5 lines for the report log file ---------------------------------------------------------------------
	std::string cl_file = input_data.get<std::string>("report_log");
//-----------------------------------------------------------------------------------
	std::ofstream calibrep;
	calibrep.open(cl_file, std::ofstream::out | std::ofstream::app);
	calibrep.is_open();
	std::mutex mtx; //instance of mutex
//--------------- opening calibration data output file ------------------------------------------------------------------
	std::ofstream ch_calib_data(out_chan_folder+fe_name+"_data.txt");

// ---- account for the detector type -------------------------------
	int n_vmms = 8;	// MMFE and sFEB have 8 vmms										
	if(pFEB){n_vmms=4;}  // pFEB has 4 vmms					
//------------- Board level calibration starts ---------------------------------------------------	

	auto feb = frontend_configs.at(fe_name_sorted);

	std::string server=feb.getOpcServerIp();	

	out_json_l1.put("OpcServerIp",server); //	
	out_json_l1.put("OpcNodeId",fe_name);	
//------------------------------------------------------------------------------------
	ch_calib_data.is_open();// add header
		
	pt::ptree trimmer_node;
	pt::ptree mask_node;
	pt::ptree single_mask;
	pt::ptree single_trim;

	for(int i_vmm=0; i_vmm<n_vmms; i_vmm++)
		{
    //////////////////////////////////
	    // VMM-level calculations
	auto b0 = std::chrono::high_resolution_clock::now();
		std::cout<<"\n =============== Reading "<<fe_name<<" VMM "<<i_vmm<<" baseline ======================\n"<<std::endl;;
			
	    fe_samples_tmp.clear();
			for (int channel_id = 0; channel_id < NCH_PER_VMM; channel_id++)
			{
						fe_samples_tmp = read_baseline(
							cs,
							feb,
							cm,
							i_vmm,
							channel_id,
							n_samples,
							fe_samples_tmp,
							channel_baseline_med,
							channel_baseline_rms,
							debug,
							RMS_CUTOFF
							);
			 }

			auto b1 = std::chrono::high_resolution_clock::now();
	
//-------------- baseline loop ends here -------------------------------------------------------
					
	    float tmp_median = cm.take_median(fe_samples_tmp); // this thing raises the vmm average level ... i think ...																									//-----------------------------------
//-------------------------------------------	
			bool bad_bl = false;
//----------------- searching for hot and dead channels ---------------------------------------------------			
			float ch_med_dev;
			std::vector<int> channel_mask; // necessary for the channel check at best trimmer calculations
			int noisy_chan = 0, hot_chan = 0, dead_chan = 0;			
			float sum_rms = 0, ch_noise = 0;

			for(int channel_id=0; channel_id<NCH_PER_VMM; channel_id++)
			{
				std::pair<std::string,int> feb_ch(fe_name,channel_id);	
//				vmm_median = cm.cm.sample_to_mV(vmm_baseline_med[fe_name]); 
				ch_med_dev = cm.sample_to_mV(channel_baseline_med[feb_ch]) - cm.sample_to_mV(tmp_median);//vmm_baseline_med[feb.getAddress()]);
				ch_noise = cm.sample_to_mV(channel_baseline_rms[feb_ch]); //checking the noise
				sum_rms += ch_noise; 
				if(ch_noise > RMS_CUTOFF){noisy_chan++;}
				if(ch_med_dev > BASELINE_CUTOFF)//cutoff)
				{
					std::cout<<"\n"<<fe_name<<" VMM_"<<i_vmm<<" channel "<<channel_id<<" has large offset above vmm median ["<<ch_med_dev<<"]	-> ENABLING MASK\n"<<std::endl;
					channel_mask.push_back(1);
					hot_chan++;
				}
				else if(ch_med_dev < -BASELINE_CUTOFF)//-cutoff)
				{
					std::cout<<fe_name<<" VMM_"<<i_vmm<<" channel "<<channel_id<<" has large offset below vmm median ["<<ch_med_dev<<"]	-> ENABLING MASK\n"<<std::endl;
					channel_mask.push_back(1);
					dead_chan++;
				}
				else
				{
					channel_mask.push_back(0);
					continue;
				}
			}
			float	average_noise = sum_rms/64;
			if(noisy_chan >= 32){
				bad_bl = true;// later increases nr of samples for the trimmers;
				mtx.lock();
				calibrep<<fe_name<<" VMM_"<<i_vmm<<" - HALF or more channels have sample RMS > 30mV ["<<noisy_chan<<" CH], with average CH rms: ["<<average_noise<<" mV] - calibration might be flawed"<<std::endl;
				mtx.unlock();
			}
		//-------------------------------------------------------------------------------------------------------------------------------------------------			
			int bad_chan = hot_chan + dead_chan;

			if(bad_chan < 16 and bad_chan != 0 ){std::cout<<"INFO - "<<fe_name<<" VMM_"<<i_vmm<<" has few BAD channels - {HOT - "<<hot_chan<<"}{DEAD -"<<dead_chan<<"}"<<std::endl;}
			if(bad_chan >= 16 and bad_chan < 32)
			{
				std::cout<<"WARNING - "<<fe_name<<" VMM_"<<i_vmm<<" has more than quarter of BAD channels ["<<bad_chan<<"]-{H - "<<hot_chan<<"}{D - "<<dead_chan<<"}, proceeding with caution"<<std::endl;
				mtx.lock();
				calibrep<<fe_name<<" VMM_"<<i_vmm<<" - ["<<bad_chan<<"] channels are unusable!\n"<<std::endl;
				mtx.unlock();
			}
			if(bad_chan >= 32)
			{
				mtx.lock();
				calibrep<<fe_name<<" VMM_"<<i_vmm<<" - more than half of the channels are unusable! Skipping this VMM!\n"<<std::endl;
				mtx.unlock();
				std::cout<<"SEVERE PROBLEM - "<<fe_name<<" VMM_"<<i_vmm<<"has more than HALF BAD channels ["<<bad_chan<<"]-{H - "<<hot_chan<<"}{D - "<<dead_chan<<"}, skipping this VMM"<<std::endl;
				std::cout<<"SUGGESTION - read "<<fe_name<<" VMM_"<<i_vmm<<" full baseline to assess severity of the problem"<<std::endl;
				continue;
			}

	    fe_samples_pruned.clear();																																																																	//--- this part needs adjustment ----
			int CH = 0, counter = 0;
	    for (auto sample: fe_samples_tmp)
				{
					float samp_chan = counter/(n_samples*10);
					CH = std::floor(samp_chan);
				//	std::cout<<counter<<" -> "<<samp_chan<<" -> "<<CH<<" -> "<<sample<<std::endl; 
		      if ((channel_mask.at(CH)!=1) and fabs(cm.sample_to_mV(sample - tmp_median)) < RMS_CUTOFF)/*mV*/ // kicking out the samples with 30mV above median, with channel check
					{
		        fe_samples_pruned.push_back(sample);
					}else{
						counter+=1;
						continue;
					}  // here kicking out the samples above baseline cutoff, no info about which channel
			 		counter+=1;
				}
			
			  // calculate VMM median baseline and rms
		    float vmm_sum    = std::accumulate(fe_samples_pruned.begin(), fe_samples_pruned.end(), 0.0);
		    float vmm_mean   = vmm_sum / fe_samples_pruned.size();
		    float vmm_stdev  = cm.take_rms(fe_samples_pruned, vmm_mean);
		    float vmm_median = cm.take_median(fe_samples_pruned);

				if(debug){std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<
				" : Calculated global {tmp_vmm_median = "<<cm.sample_to_mV(tmp_median)<<"}{vmm_median = "<<cm.sample_to_mV(vmm_median)<<"}{vmm_stdev = "<<cm.sample_to_mV(vmm_stdev)<<"}"<<std::endl;}
		    vmm_baseline_med[fe_name] = vmm_median;
		    vmm_baseline_rms[fe_name] = vmm_stdev;

		//----------------------------------------------------------------------------------------------------------------------------	
		    fe_samples_pruned.clear();
		    fe_samples_tmp.clear();

		    //////////////////////////////////
		    //////////////////////////////////
		    // Global Threshold Calculations
		    std::tuple<std::string, float, float> thdac_constants;
				
	    	int thdac_central_guess = rms_factor * cm.sample_to_mV(vmm_baseline_rms[fe_name]) + cm.sample_to_mV(vmm_baseline_med[fe_name]) + offset_center;
		
				std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" THDAC guess - ["<<thdac_central_guess<<" (mV)]"<<std::endl;

		    std::cout << "\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" baseline_mean, baseline_med, baseline_rms, rms_factor: "
		                << cm.sample_to_mV(vmm_mean) << ", "
		                << cm.sample_to_mV(vmm_baseline_med[fe_name]) << ", "
		                << cm.sample_to_mV(vmm_baseline_rms[fe_name]) << ", "
		                << rms_factor
		                << std::endl;
		//------------------ against this value later calibration result is checked to see if the result is what was expected ------------------------------
				float expected_eff_thr = cm.sample_to_mV(vmm_baseline_rms[fe_name] * (rms_factor));
		//-------------------------------------------------------------------------------------------------------------------------------------------------	

		    std::vector<int> thdac_guess_variations = {100,150,200,250,300,350,400};
		
				auto t0 = std::chrono::high_resolution_clock::now();
				float mean;
				int thdac;
				for(int th_try = 0; th_try < 3; th_try++)
				{	
			    thdac = calculate_thdac_value(cs,feb,cm,i_vmm,n_samples,thdac_central_guess,thdac_constants,thdac_guess_variations, debug);

					std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<": Calcuated THDAC value -> ["<<thdac<<"] DAC counts"<<std::endl;	

		  		feb.getVmm(i_vmm).setMonitorOutput  (nsw::vmm::ThresholdDAC, nsw::vmm::CommonMonitor);
		   		feb.getVmm(i_vmm).setGlobalThreshold((size_t)(thdac));
		  		auto results = cs.readVmmPdoConsecutiveSamples(feb, i_vmm, n_samples);
		   		float sum = std::accumulate(results.begin(), results.end(), 0.0);
		   		mean = sum / results.size();
					float thr_diff = mean - cm.sample_to_mV(vmm_baseline_med[fe_name]);
					if(thr_diff <= 0)
					{
						std::cout<<"\nWARNING: "<<fe_name<<" VMM_"<<i_vmm<<" resulting threshold is ["<<thr_diff<<"] mV BELOW baseline! tryning once more after slignt delay"<<std::endl;
					 	std::this_thread::sleep_for(std::chrono::milliseconds(30));
						if(debug){std::cout<<"CHECK - [DAC-Slope = "<<std::get<1>(thdac_constants)<<" : DAC-offset = "<<std::get<2>(thdac_constants)<<"]"<<std::endl;}
						if(th_try!=2){continue;}	
					}
		   		thdacs_sample[feb.getAddress()] = mean;  //actually set threshold, previous samples were readout on guess
					float thdac_dev = fabs(cm.sample_to_mV(mean) - thdac_central_guess)/thdac_central_guess;
					if(thdac_dev < 0.1)
					{
			 			std::cout << "\nINFO - Threshold for " << feb.getAddress() << " vmm" << i_vmm << " is " << cm.sample_to_mV(mean) << " in mV with deviation from guess: ["<<thdac_dev*100<<" %]"<<std::endl;
						break;
					}
					else
					{
						std::cout<<"\nWARNING - "<<feb.getAddress()<<"VMM_"<<i_vmm<<" - {"<<thdac_dev*100<<"%} deviation in threshold calculation\n"<<"[guess = "<<thdac_central_guess<<" -> calc. = "<<cm.sample_to_mV(mean)<<"] : repeating calculation "<<th_try<<" time"<<std::endl;	
					std::this_thread::sleep_for(std::chrono::milliseconds(40));
					}		
		  	}
			  thdacs[feb.getAddress()] = thdac;			
				if((mean - cm.sample_to_mV(vmm_baseline_med[fe_name])) < 0){
					mtx.lock();
					calibrep<<fe_name<<" VMM_"<<i_vmm<<" threshold below baseline!\n"<<std::endl;
					mtx.unlock();
				}
				auto t1 = std::chrono::high_resolution_clock::now();
		    //////////////////////////////////
	 		 //////////////////////////////////
 			 // Get VMM-level averages.
		    fe_samples_tmp.clear();

				std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" - Calc. V_eff with trimmers at bit[14]\n"<<std::endl;

				auto tr0 = std::chrono::high_resolution_clock::now();
				
	    	for (int channel_id = 0; channel_id < NCH_PER_VMM; channel_id++)
				{	
					if(channel_mask.at(channel_id)!=0)
					{
						std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" Channel "<<channel_id<<" masked, so skipping it"<<std::endl;
						continue;
					}
	//-------------- vmm level average function test ------------------------------------
		
					fe_samples_tmp = vmm_averages(
							cs,
							feb,
							i_vmm,
							channel_id,
							n_samples,
							TRIM_MID,
							thdac,
							fe_samples_tmp
							);
		    	}
		
	auto tr1 = std::chrono::high_resolution_clock::now();
		    // find the median eff_thresh value for a given FE, vmm
		    size_t vmm_n = fe_samples_tmp.size() / 2;
		    std::nth_element(fe_samples_tmp.begin(), fe_samples_tmp.begin()+vmm_n, fe_samples_tmp.end());
		    float vmm_median_trim_mid = cm.take_median(fe_samples_tmp);
		    float vmm_eff_thresh = vmm_median_trim_mid - vmm_baseline_med[feb.getAddress()];
//--------- if effective thereshold on VMM level is less than 10 mV resample thresholds with trimmers and a raised THDAC -------------------------------------	
				if(vmm_eff_thresh < 30)// 30 ADC counts - roughly 10 mV;
			 	{
					fe_samples_tmp.clear();
					int noise_fact = cm.sample_to_mV(vmm_baseline_rms[feb.getAddress()]);	//converting noise rms adc to mV
					int AddFactor = std::round(noise_fact/std::get<1>(thdac_constants));		//calculating dac counts to be added
					if(AddFactor == 0){AddFactor = 1;} // if resulting rms is too low to give at least 1dac count - assigning 1dac count
					int thdac_raised = thdac + AddFactor;
					if(debug){std::cout<<"CHECK - factor to add is {"<<AddFactor<<"} DAC counts"<<std::endl;} 
					std::cout<<"\nWARNING - "<<feb.getAddress()<<" VMM_"<<i_vmm<<" effective threshold is less than 10 mV - raising THDAC by one noise rms -["<<thdac<<" + "<<AddFactor<<"] = ["<<thdac_raised<<"]"<<std::endl;
			   	for (int channel_id = 0; channel_id < NCH_PER_VMM; channel_id++)
					{
						if(channel_mask.at(channel_id)!=0)
						{
							if(debug){std::cout<<"Channel "<<channel_id<<" masked, so skipping it"<<std::endl;}
							continue;
						}
						fe_samples_tmp = vmm_averages(
								cs,
								feb,
								i_vmm,
								channel_id,
								n_samples,
								TRIM_MID,
								thdac_raised,
								fe_samples_tmp
								);
			    	}
			
			    // find the median eff_thresh value for a given FE, vmm
			    size_t vmm_n = fe_samples_tmp.size() / 2;
			    std::nth_element(fe_samples_tmp.begin(), fe_samples_tmp.begin()+vmm_n, fe_samples_tmp.end());
			    float vmm_median_trim_mid = cm.take_median(fe_samples_tmp);
			    float vmm_eff_thresh = vmm_median_trim_mid - vmm_baseline_med[feb.getAddress()]; //unused variable????
					if(debug){std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<": new V_eff = "<<vmm_eff_thresh<<std::endl;}
		    	thdacs[feb.getAddress()] = thdac_raised; //rewriting new value

				}

				int test = thdacs[feb.getAddress()];
				if(debug){std::cout<<"\nCHECK - threshold value in the thdac map for "<<fe_name<<" is ["<<test<<"] DAC counts\n"<<" [slope - "<<std::get<1>(thdac_constants)<<"]"<<std::endl;}
	//--------------------------------------------------------------------------------------------------------------------------		

		    vmm_mid_eff_thresh[feb.getAddress()] = vmm_eff_thresh;   /// fill the mapo with median value with trimmers enabled at the middle value
				
				if((cm.sample_to_mV(vmm_eff_thresh) >= 40) or (cm.sample_to_mV(vmm_eff_thresh) <= 5)){ //if res. effective threshold is >40 mV or <5 mV inform calibrep
					mtx.lock();
					calibrep<<fe_name<<" VMM_"<<i_vmm<<" high threshold [result:"<<cm.sample_to_mV(vmm_eff_thresh)<<"(mV), expected:"<<expected_eff_thr<<"(mV), noise:"<<cm.sample_to_mV(vmm_stdev)<<"(mV)]\n"<<std::endl;	
					mtx.unlock();
				}
		    //////////////////////////////////
		    //////////////////////////////////
		    // Scanning trimmers
		    int nch_base_above_thresh = 0;
		
		    std::cout << "\nINFO - Checking "<<feb.getAddress()<<" VMM_"<<i_vmm<<" trimmer linearity\n" << std::endl;	
		
		    int good_chs = 0;
		    int tot_chs = NCH_PER_VMM;
	
				std::map<std::pair<std::string, int>,float> rescued_slopes;
		    int thdac2 = thdacs[feb.getAddress()]; //reading THDAC again;
		
				auto lr0 = std::chrono::high_resolution_clock::now();
		    for (int channel_id = 0; channel_id < NCH_PER_VMM; channel_id++){
	
					if(channel_mask.at(channel_id)!=0)
					{
						if(debug){std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" Channel "<<channel_id<<" masked, so skipping it"<<std::endl;}
						continue;
					}	
		      // check if channel has a weird RMS or baseline
		
		      std::pair<std::string,int> feb_ch(fe_name,channel_id);
		      float ch_baseline_rms = channel_baseline_rms[feb_ch];
		      float ch_baseline_med = channel_baseline_med[feb_ch];
		
		      if (!cm.check_channel(ch_baseline_med, ch_baseline_rms, vmm_baseline_med[fe_name])){continue;}
		
		      /////////////////////////////////////
		      float tmp_min_eff_threshold = 0.;
		      float tmp_mid_eff_threshold = 0.;
		      float tmp_max_eff_threshold = 0.;
					float	first_trim_slope = 0;
				
		      std::pair<float,int> slopeAndMax = find_linear_region_slope(cs,
		                            feb,
																cm,
		                            i_vmm,
		                            channel_id,
		                            thdac2,
		                            tpdac,
		                            n_samples,
		                            ch_baseline_med,
		                            ch_baseline_rms,
		                            tmp_min_eff_threshold,
		                            tmp_mid_eff_threshold,
		                            tmp_max_eff_threshold,
		                            nch_base_above_thresh,
																first_trim_slope,
		                    				bad_bl, 
												  			debug,
														    TRIM_HI,
		                            TRIM_MID,
		                            TRIM_LO															
		                            );
		
		      if(slopeAndMax.first==0){
		        tot_chs--;
		        continue;
		      }
		      /////////////////////////////////////////
		
		      channel_eff_thresh_slope[feb_ch] = slopeAndMax.first;
		      channel_trimmer_max[feb_ch]      = slopeAndMax.second;
		      channel_mid_eff_thresh[feb_ch]   = tmp_mid_eff_threshold;
		
		      ch_baseline_rms = channel_baseline_rms[std::make_pair(feb.getAddress(), channel_id)];
		      ch_baseline_med = channel_baseline_med[std::make_pair(feb.getAddress(), channel_id)];
		
		      if (!cm.check_channel(ch_baseline_med, ch_baseline_rms, vmm_baseline_med[feb.getAddress()])){
		        tot_chs--;
		        continue;
		      }
		
		      if (debug) std::cout << "INFO -  [MIN_EF_TH = "
		                            << cm.sample_to_mV(tmp_min_eff_threshold)
		                            << "], [MAX_EF_TH =  "
		                            << cm.sample_to_mV(tmp_max_eff_threshold)
		                            << "], [VMM_EF_TH =  "
		                            << cm.sample_to_mV(vmm_eff_thresh) <<"]" <<std::endl;
	
		///-------------------- trimmer range check -------------------------
		      if ( vmm_eff_thresh < tmp_min_eff_threshold || vmm_eff_thresh > tmp_max_eff_threshold ){
		        if (debug) std::cout << "INFO sad! channel " << channel_id << " can't be equalized!" << std::endl;
				 	}
		      else {
		        good_chs++;
		        if (debug) std::cout << "INFO :) channel " << channel_id << " is okay!" << std::endl;
		      }
		    } // end of channel loop
		
	auto lr1 = std::chrono::high_resolution_clock::now();
			  if (nch_base_above_thresh > nsw::ref_val::ChanThreshCutoff)
				{
		      std::cout<<"\nWARNING! - "<<fe_name<<" VMM_"<<i_vmm<<" has ["<<nch_base_above_thresh<<"] channels with V_eff below baseline at trim value 14 >>> THDAC might be raised later!"<<std::endl;
		    }
		    //////////////////////////////////
		    //////////////////////////////////
		    // Trimmer Analysis
	
			int bad_trim = 64 - good_chs;
			if(debug){std::cout <<"\nINFO - {GOOD NEWS YEREVAN!}, "<<fe_name<<" VMM "<<i_vmm<<" -> "<< good_chs << " out of " << tot_chs << " are okay!\n" << std::endl;}
	   
			std::cout<<"\nINFO - "<< feb.getAddress()<<" VMM_"<<i_vmm<<" -> calculating best trimmer settings\n"<<std::endl;//MAIN DEBUG MSG

			std::map<std::pair<std::string,int>, float> eff_thr_w_best_trim; 
			std::map< std::pair< std::string,int>, int> best_channel_trim;
			std::map<std::pair<std::string, int>, float> channel_trimmed_thr;
			std::map<std::pair<std::string,int>,int> DAC_to_add;// value to add to THDAC is one of the channels with neg. thr. is unmasked
		//--------- check for trimmer performance vector of effective thr. --------		
//			std::vector<float> trim_perf;
	
	auto e0 = std::chrono::high_resolution_clock::now();
			bool recalc = false;
			int thdac_i = thdacs[feb.getAddress()];
			if(debug){std::cout<<"CHECK - thdac value for "<<fe_name<<" is ["<<thdac_i<<"]"<<std::endl;}
	    for (int channel_id = 0; channel_id < NCH_PER_VMM; channel_id++)
			{ // channel loop
	//--------------- trimmer analysis function tryout -------------------------------------------------
					if(channel_mask.at(channel_id)!=0)
					{
						std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" Channel "<<channel_id<<" masked, so skipping it"<<std::endl;
						continue;
					}

		       best_channel_trim = analyse_trimmers(
									cs,
									feb,
									cm,
									i_vmm,
									channel_id,
									n_samples,
									TRIM_MID,
									channel_trimmed_thr,
									thdac_i,	
									channel_baseline_med,
									channel_baseline_rms,
									vmm_baseline_med,
									vmm_mid_eff_thresh,
									channel_mid_eff_thresh,
									channel_eff_thresh_slope,
									channel_trimmer_max,
									eff_thr_w_best_trim,
									channel_mask,
									DAC_to_add,
									best_channel_trim,
//								trim_perf,
									recalc,
									debug
									);
			}
	auto e1 = std::chrono::high_resolution_clock::now();
//	float trim_mean = (std::accumulate(trim_perf.begin(), trim_perf.end(), 0.0)/(trim_perf.size()));	//
//	float trim_rms = cm.take_rms(trim_perf, trim_mean);																									// check for eff. thr. scattering around mean
	if(bad_trim >= 16)
	{
		mtx.lock();
//		calibrep<<fe_name<<" VMM_"<<i_vmm<<": Large effective threshold RMS - ["<<trim_rms<<"], OR many badly trimmed CH - ["<<bad_trim<<"/64](try 1)"<<std::endl;
		calibrep<<fe_name<<" VMM_"<<i_vmm<<": Many badly trimmed CH - ["<<bad_trim<<"/64](try 1)"<<std::endl;
		mtx.unlock();
	}
	//-------- check if there are unmasked channels above threshold, if tis` the case - recalculate values with new thdac ---------------

	std::vector<int> add_dac;	
	for(int i=0; i<NCH_PER_VMM; i++)
	{
		std::pair<std::string,int> feb_ch(fe_name,i);
		float extraDAC = DAC_to_add[feb_ch];
		//int extraDAC = DAC_to_add[feb_ch];
		if(extraDAC > 0 and cm.sample_to_mV(extraDAC) <= 16)/*mV, half of trimmer working range*/
		{		
			add_dac.push_back(extraDAC);
		}
		else if(extraDAC = 0){continue;}
		else{		
			continue;
		}
	}
	if(add_dac.size()>0)
	{
		recalc = true;
	//	trim_perf.clear();
		std::sort(add_dac.begin(),add_dac.end());
		int plus_dac = std::round((add_dac.at(add_dac.size()-1))/std::get<1>(thdac_constants)) + 1; 
		int thdac_new = thdac_i + plus_dac;
		std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" New THDAC value is set to - ( "<<thdac_new<<" )"
													<<" by adding ( "<<plus_dac<<" + 1 )[DAC]  -> recalculating threshold values"<<std::endl;
	
	  for (int channel_id = 0; channel_id < NCH_PER_VMM; channel_id++)
		{ // channel loop
				if(channel_mask.at(channel_id)!=0)
				{
					std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" Channel "<<channel_id<<" masked, so skipping it"<<std::endl;
					continue;
				}
				std::pair<std::string,int> feb_ch(fe_name,channel_id);
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				
				best_channel_trim = analyse_trimmers(
									cs,
									feb,
									cm,
									i_vmm,
									channel_id,
									n_samples,
									TRIM_MID,
									channel_trimmed_thr,
									thdac_new,									//calculating with new thdac
									channel_baseline_med,
									channel_baseline_rms,
									vmm_baseline_med,
									vmm_mid_eff_thresh,
									channel_mid_eff_thresh,
									channel_eff_thresh_slope,
									channel_trimmer_max,
									eff_thr_w_best_trim,
									channel_mask,
									DAC_to_add,
									best_channel_trim,
//									trim_perf,
									recalc,
									debug
									);
		}
		
		thdacs[feb.getAddress()] = thdac_new; //rewriting thdac after its raised for this VMM
//		float trim_mean = (std::accumulate(trim_perf.begin(), trim_perf.end(), 0.0)/(trim_perf.size()));
//		float trim_rms2 = cm.take_rms(trim_perf, trim_mean);
		if(bad_trim >= 16 or recalc)
		{
			mtx.lock();
//			calibrep<<fe_name<<" VMM_"<<i_vmm<<": Large effective threshold RMS - ["<<trim_rms2<<"] ,OR many badly trimmed CH - ["<<bad_trim<<"/64](try 2)"<<std::endl;
			calibrep<<fe_name<<" VMM_"<<i_vmm<<":Threshold raised by {"<<plus_dac<<"} DAC, OR many badly trimmed CH - ["<<bad_trim<<"/64](try 2)"<<std::endl;
			mtx.unlock();
		}
		//std::cout<<"\nINFO - "<<fe_name<<" VMM_"<<i_vmm<<" V_eff value RMS = "<<trim_rms2<<std::endl;
	}	
///-------------------- end of trimmer analysis-----------------------------------------------------------------
		int thdac_to_json = thdacs[feb.getAddress()];
		if(debug){std::cout<<"CHECK - thdac for "<<fe_name<<" at this point is ["<<thdac_to_json<<"]"<<std::endl;}
		out_json_l2.put("sdt_dac", thdac_to_json); //later move this to the place where thdac is altered lats time (it might change later during calibration...)
//--------------- channel calib parameters to file ----------------------------------- 			 

		auto f0 = std::chrono::high_resolution_clock::now();
			for(int channel_id=0;channel_id<NCH_PER_VMM; channel_id++)
			{
				std::pair<std::string,int> feb_ch(feb.getAddress(),channel_id);
				ch_calib_data<<feb.getAddress()<<"\t"
							<<i_vmm<<"\t"
							<<channel_id<<"\t"
							<<cm.sample_to_mV(channel_baseline_med[feb_ch])<<"\t"			
							<<cm.sample_to_mV(channel_baseline_rms[feb_ch])<<"\t"
							<<cm.sample_to_mV(channel_mid_eff_thresh[feb_ch])<<"\t"
							<<channel_eff_thresh_slope[feb_ch]<<"\t"
							<<cm.sample_to_mV(vmm_baseline_med[feb.getAddress()])<<"\t"
							<<cm.sample_to_mV(vmm_baseline_rms[feb.getAddress()])<<"\t"
							<<cm.sample_to_mV(vmm_median_trim_mid)<<"\t"
							<<cm.sample_to_mV(vmm_eff_thresh)<<"\t"
							<<thdacs[feb.getAddress()]<<"\t"
							<<best_channel_trim[feb_ch]<<"\t"
							<<channel_trimmed_thr[feb_ch]<<"\t"
							<<eff_thr_w_best_trim[feb_ch]<<std::endl;

				single_trim.put("",best_channel_trim[feb_ch]);
				trimmer_node.push_back(std::make_pair("",single_trim));			

				single_mask.put("",channel_mask.at(channel_id));
				mask_node.push_back(std::make_pair("",single_mask));
			}

//----------- writing all ptrees to the json -------------------------------			
		
			std::string vmmname="vmm"+std::to_string(i_vmm);
		
	 		out_json_l2.add_child("channel_sd", trimmer_node);
			out_json_l2.add_child("channel_sm", mask_node);
		
			trimmer_node.clear();
			mask_node.clear();
		
			out_json_l1.add_child(vmmname, out_json_l2);
	
			out_json_l2.clear();
	
			std::cout<<"\nINFO - "<< feb.getAddress()<<" VMM_"<<i_vmm<<" data written to file\n"<<std::endl;//MAIN DEBUG MSG// check that it was indeed written !?	
				
			float nr_ch_masked = std::accumulate(channel_mask.begin(), channel_mask.end(), 0.0);
//			std::string which_mchan = "";
//			for(int i = 0; i< channel_mask.size(); i++)
//			{
//				if(channel_mask.at(i)!=0){which_mchan += std::to_string(i)+",";}
//				if()
//				else{continue;}
//			}
			if(nr_ch_masked >=4){
				mtx.lock();
				calibrep<<fe_name<<" VMM_"<<i_vmm<<" Nr of masked channels = ["<<nr_ch_masked<<"/64]\n"<<std::endl;
				mtx.unlock();
			}
	auto f1 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> t_bl = b1 - b0;
	std::chrono::duration<double> t_thdac = t1 - t0;
	std::chrono::duration<double> t_bl_wtrim = tr1 - tr0;
	std::chrono::duration<double> t_trim_lin = lr1 - lr0;
	std::chrono::duration<double> t_ef_thr = e1 - e0;
	std::chrono::duration<double> t_files = f1 - f0;

	if(debug){
		std::cout << "\n baseline reading time: " << t_bl.count() << " s";
		std::cout << "\n thdac read/calc time: " << t_thdac.count() << " s";
		std::cout << "\n baseline w trims reading time: " << t_bl_wtrim.count() << " s";
		std::cout << "\n trim read/lin-calc time: " << t_trim_lin.count() << " s";
		std::cout << "\n effective threshold calc time: " << t_ef_thr.count() << " s";
		std::cout << "\n file writing time: " << t_files.count() << " s";
		}
	}
//------------------end of VMM loop-------------------------
	calibrep.close();
	ch_calib_data.close();
//------------ output filles closed ------------------------------------------					
	std::string json_folder=input_data.get<std::string>("json_data_output");	
	pt::write_json(json_folder+fe_name+"_config_test2.json", out_json_l1);
//------------- elapsed time ---------------------------------------	
	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = finish - start;
	std::cout << "\n Elapsed time: " << elapsed.count() << " s\n";

//--------------------end of board configuration ----------------------------	
}

std::vector<short unsigned int> nsw::CalibrationSca::ch_threshold(nsw::ConfigSender &cs,
																		nsw::FEBConfig &feb,
																		int vmm_id,
																		int channel_id,
																		int n_samples)
{
		 feb.getVmm(vmm_id).setMonitorOutput  (channel_id, nsw::vmm::ChannelMonitor);
     feb.getVmm(vmm_id).setChannelMOMode  (channel_id, nsw::vmm::ChannelTrimmedThreshold);
     auto results = cs.readVmmPdoConsecutiveSamples(feb, vmm_id, n_samples);
		return results;
}

void nsw::CalibrationSca::read_thresholds(std::string config_filename,
											std::vector<nsw::FEBConfig> frontend_configs,
											std::string io_config_path,
											int n_samples,
											int fe_name_sorted,
											bool pFEB,
											bool debug,
											std::string fe_name)
{
//------------------ main business starts here !----------------------
  nsw::ConfigSender cs;
	nsw::CalibrationMath cm;
//------------------------------------------------------------------	
	int n_vmms = 8;
	if(pFEB){n_vmms=4;}
//-------------------------------------------------------------
	namespace pt = boost::property_tree;
	pt::ptree input_data;
	pt::read_json(io_config_path,input_data);
	std::string path2 = input_data.get<std::string>("thr_data_output");
	std::ofstream thr_test(path2+fe_name+"_thresholds.txt");// separate file output per board
//--------------------------------------------------------------------------------------------------------------------------
	std::string cl_file = input_data.get<std::string>("report_log");
	std::mutex mtx;
	std::ofstream calibrep(cl_file, std::ofstream::out | std::ofstream::app);
	calibrep.is_open();
//-----------------------------------------------------------------------------------
		int bad_thr_tot = 0;
		thr_test.is_open();
		auto feb = frontend_configs.at(fe_name_sorted);

    for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) {
 			std::vector<short unsigned int> results;
			int dev_thr = 0;
      for (int channel_id = 0; channel_id < 64; channel_id++) {
						
				for(int itry=0; itry<5; itry++){		
					try{
	
						  results = ch_threshold(cs, feb, vmm_id, channel_id, n_samples);
							break;
							}catch(std::exception &e){
								std::cout<<"\nERROR - Couldn`t read"<<fe_name<<" VMM_"<<vmm_id<<" Channel "<<channel_id<<" threshold, reason: !["<<e.what()<<"]!"<<std::endl;
								std::this_thread::sleep_for(std::chrono::milliseconds(15));
							}
	   	  	  }
						float sum    = std::accumulate(results.begin(), results.end(), 0.0);
    	  	  float mean   = sum / results.size();	

						if(mean < 100)
						{
							std::cout<<"WARNING - "<<feb.getAddress()<<" VMM_"<<vmm_id<<" channel - "<<channel_id<<" -> threshold might be dead = ["<<cm.sample_to_mV(mean)<<" mV]"<<std::endl;
						}
	
//-------------- searching for max and min deviation in samples -----------------------------------------------------------	   
					  float max_dev;
					  float min_dev;
	   	  
						std::sort(results.begin(),results.end());
		 				max_dev = results.at(n_samples-1);
		 				min_dev = results.at(0);
						float thr_dev = max_dev - min_dev;
						if(thr_dev > 50)
						{
							std::cout<<feb.getAddress()<<" VMM_"<<vmm_id<<" channel - "<<channel_id<<" -> high threshold deviation = ["<<cm.sample_to_mV(thr_dev)<<" mV]"<<std::endl;
							dev_thr++;
						}
					
//--------------------- text file output ----------------------------------------------------------------------------------------------
						if(debug){std::cout<<feb.getAddress()<<"\t"<<vmm_id<<"\t"<<channel_id<<"\t"<<cm.sample_to_mV(mean)<<"\t"<<cm.sample_to_mV(max_dev)<<"\t"<<cm.sample_to_mV(min_dev)<<std::endl;}  
						thr_test<<feb.getAddress()<<"\t"<<vmm_id<<"\t"<<channel_id<<"\t"<<cm.sample_to_mV(mean)<<"\t"<<cm.sample_to_mV(max_dev)<<"\t"<<cm.sample_to_mV(min_dev)<<std::endl;  
        
		}//channel loop ends
		bad_thr_tot += dev_thr;
		std::cout<<fe_name<<" VMM_"<<vmm_id<<" thresholds done"<<std::endl;
   }//vmm loop ends
	
	 if(bad_thr_tot > 8)
	 {
			mtx.lock();
			calibrep<<fe_name<<" - Nr. of CH with strongly deviating thresholds = ["<<bad_thr_tot<<"/512]\n"<<std::endl;
			mtx.unlock();
	 }
	
// }//config loop ends
	calibrep.close();
	thr_test.close();
}

void nsw::CalibrationSca::read_baseline_full(std::string config_filename,
											std::vector<nsw::FEBConfig> frontend_configs,
											std::string io_config_path,
											int n_samples,
											int fe_name_sorted,
											bool pFEB,
											std::string fe_name,
											bool conn_check)
{
//------------------ main business starts here !----------------------
  nsw::ConfigSender cs;
	nsw::CalibrationMath cm;
//------------------------------------------------------------------	
	int n_vmms = 8;
	if(pFEB){n_vmms=4;}
//-------------------------------------------------------------
	namespace pt = boost::property_tree;
	pt::ptree input_data;
	pt::read_json(io_config_path,input_data);
	std::string path = input_data.get<std::string>("bl_data_output");
	std::string cl_file = input_data.get<std::string>("report_log");

	std::mutex mtx;
	std::ofstream calibrep(cl_file, std::ofstream::out|std::ofstream::app);

	std::ofstream full_bl(path+fe_name+"_full_bl.txt");// separate file output per board	
	
	calibrep.is_open();
	full_bl.is_open();
	std::vector<int> vmm_fchan={};
	int fault_chan_total=0;

	auto feb = frontend_configs.at(fe_name_sorted);

//	std::cout<<feb.dump()<<std::endl;
	feb.dump();
  	for (int vmm_id = 0; vmm_id < n_vmms; vmm_id++) {

		std::vector<short unsigned int> results;

		auto t0 = std::chrono::high_resolution_clock::now();
		int fault_chan = 0, noisy_channels = 0;

    	for (int channel_id = 0; channel_id < 64; channel_id++) {
							
				feb.getVmm(vmm_id).setMonitorOutput(channel_id, nsw::vmm::ChannelMonitor);
    		feb.getVmm(vmm_id).setChannelMOMode(channel_id, nsw::vmm::ChannelAnalogOutput);
    		auto results = cs.readVmmPdoConsecutiveSamples(feb, vmm_id, n_samples*10);

	//			for(auto & result :results)
	//			{
	//			//	full_bl<<fe_name<<"\t"<<vmm_id<<"\t"<<channel_id<<"\t"<<cm.sample_to_mV(result)<<std::endl;  
	//				full_bl<<fe_name<<"\t"<<vmm_id<<"\t"<<channel_id<<"\t"<<cm.sample_to_mV(result)<<"\t"<<cm.sample_to_mV(rms)<<std::endl;  
	//			}
				float sum    = std::accumulate(results.begin(), results.end(), 0.0);
     	  float mean   = sum / results.size();	
				float median = cm.take_median(results); 
				float rms = cm.take_rms(results,mean); 
	
				for(auto & result :results)
				{
				//	full_bl<<fe_name<<"\t"<<vmm_id<<"\t"<<channel_id<<"\t"<<cm.sample_to_mV(result)<<std::endl;  
					full_bl<<fe_name<<"\t"<<vmm_id<<"\t"<<channel_id<<"\t"<<cm.sample_to_mV(result)<<"\t"<<cm.sample_to_mV(rms)<<std::endl;  
				}
		
				if(cm.sample_to_mV(rms)>30){noisy_channels++;}
				
				if(conn_check)
				{
					if(cm.sample_to_mV(mean)>180)
					{
						fault_chan++;
						printf("\nWARNING - %s - VMM %i - %i high baseline = <<%f mV>>, probably needs to be masked\n", fe_name.c_str(), vmm_id, channel_id, cm.sample_to_mV(mean));
					}
					if(cm.sample_to_mV(mean)<140)
					{
						fault_chan++;
						printf("\nWARNING - %s - VMM %i - %i low baseline = <<%f mV>>, probably needs to be masked\n", fe_name.c_str(), vmm_id, channel_id, cm.sample_to_mV(mean));
					}
				}
//-------------- searching for max and min deviation in samples -----------------------------------------------------------	   
			  float max_dev;
			  float min_dev;
			
				std::sort(results.begin(),results.end());
			 	max_dev = results.at(n_samples-1);
			 	min_dev = results.at(0);
				float sample_dev = max_dev - min_dev;

	   	  if(conn_check)
				{
					std::cout<<"INFO - "<<fe_name<<" VMM_"<<vmm_id<<" CH:"<<channel_id<<" - BL: "<<cm.sample_to_mV(median)<<" - RMS: "<<cm.sample_to_mV(rms)<<", spread: "<<cm.sample_to_mV(sample_dev)<<std::endl;
				//	if(cm.sample_to_mV(sample_dev)>60)
				//	{
				//		printf("this channel %i sample deviation above 60mV (masking...?)\n",channel_id);
				//	}
				}
		}//channel loop ends
		fault_chan_total += fault_chan;
		vmm_fchan.push_back(fault_chan);		
		auto t1 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double>	t_bl = t1 - t0;
		if(conn_check){std::cout<<"VMM_"<<vmm_id<<" done in "<<t_bl.count()<<"[s]"<<std::endl;}

		if(noisy_channels>16){
			std::cout<<"\n[ [WARNING!] - "<<fe_name<<" -> VMM "<<vmm_id<<" has >16 channels w noise above 30mV - "<<noisy_channels<<" ]]\n"<<std::endl;
			mtx.lock();
			calibrep<<fe_name<<" VMM_"<<vmm_id<<" >>"<<fault_chan<<"<< faulty channels\n"<<std::endl;
			mtx.unlock();
		}

		std::cout<<fe_name<<" VMM_"<<vmm_id<<" done"<<std::endl;
  	results.clear(); 
		if(fault_chan>10 and fault_chan<32){
			std::cout<<"\n[ [WARNING!] - "<<fe_name<<" -> VMM "<<vmm_id<<" has >10 faulty channels - "<<fault_chan<<" ]]\n"<<std::endl;
			mtx.lock();
			calibrep<<fe_name<<" VMM_"<<vmm_id<<" >>"<<fault_chan<<"<< faulty channels\n"<<std::endl;
			mtx.unlock();
		}
		if(fault_chan>=32){
			std::cout<<"\n{{ [EPIC_WARNING!] - "<<fe_name<<" -> VMM "<<vmm_id<<" more than _HALF_ of channels are faulty - ["<<fault_chan<<"] }}\n"<<std::endl;
			mtx.lock();	
			calibrep<<fe_name<<" VMM_"<<vmm_id<<" >>"<<fault_chan<<"<< faulty channels\n"<<std::endl;
			mtx.unlock();
		}
	}//vmm loop ends
	if(conn_check){
		mtx.lock();
		calibrep<<fe_name<<" - NR of unusable channels = ["<<fault_chan_total<<"]\n"<<std::endl;
		mtx.unlock();
	}
	
	calibrep.close();	
	full_bl.close();
}

void nsw::CalibrationSca::merge_json(std::string & mod_json, std::string io_config_path)
{
	namespace pt = boost::property_tree;
	pt::ptree input_data;
	pt::read_json(io_config_path,input_data);

	std::string json_dir = input_data.get<std::string>("json_data_output");

	std::cout<<"\nMerging generated and common configuration trees\n"<<std::endl;
	
 	std::string main_path = input_data.get<std::string>("config_dir");
	
	pt::ptree mmfe_conf;
//---------------------------------------------------------------------
	std::vector <std::string> in_files;
//------------ filling file name vector --------------------------	
	DIR *dir=NULL;
	struct dirent *ent=NULL;
	dir = opendir(json_dir.c_str()); // make sure there is a propper path specified to generated MMFE children node files

	std::printf("Reading json file directory, omitting swaps and dots ...\n");	//debug msg
	char s = '.';
	if(dir!=NULL)
	{
		while(ent = readdir(dir))
		{
			std::string file_n = ent->d_name;
			if(file_n.at(0)!=s){in_files.push_back(ent->d_name);}	
		}			
	}		
	// just checking if the files are in teh vector
	std::printf("written file names in to vector\n"); //debug msg

	std::sort(in_files.begin(),in_files.end());

	std::printf("sorted files\n");//debug msg

//	in_files.erase(in_files.begin(),in_files.begin()+2);
		
	int fsize=in_files.size();
	std::printf("directory has [[ %i ]] json files\n",fsize);	

//========== trying to modify already existing json==========================================================
	pt::ptree prev_conf;
	std::string start_configuration = input_data.get<std::string>("configuration_json");
	pt::read_json(main_path+start_configuration, prev_conf);

	for(long unsigned int i=0; i<in_files.size(); i++){
		std::ifstream in_file_check;
		in_file_check.open(json_dir+in_files[i], std::ios::in);
		if(in_file_check.peek() == std::ifstream::traits_type::eof() )
    {
      std::cout<<"WARNING: FILE("<<in_files[i]<<") is EMPTY\n"<<std::endl;
      in_file_check.close();
      continue;
    }
		in_file_check.close();
		pt::read_json(json_dir+in_files[i], mmfe_conf);
		std::string fename = mmfe_conf.get<std::string>("OpcNodeId");

		for(int nth_vmm=0; nth_vmm<8; nth_vmm++)
		{
			std::string vmm = "vmm"+std::to_string(nth_vmm);
			std::string child_name = fename+"."+vmm;
			std::cout<<"\nLooking for node:"<<vmm<<std::endl;
			if(mmfe_conf.count(vmm)==0){std::cout<<"Failed"<<std::endl;continue;}
			else{std::cout<<"Success"<<std::endl;}
			prev_conf.add_child(child_name, mmfe_conf.get_child(vmm));	
			printf("\nadded %s VMM%i node", fename.c_str(), nth_vmm);

		}
	}
	pt::write_json(main_path+mod_json+"_sdsm_appended.json", prev_conf);
//==============================================================
	printf("\n new configuration file is -> %s\n",mod_json.c_str());
//------------here`s the end-------------

}