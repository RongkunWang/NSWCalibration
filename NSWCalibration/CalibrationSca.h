//Class for SCA path calibration
#ifndef CALIBRATIONSCA_H_
#define CALIBRATIONSCA_H_

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

#include "NSWCalibration/CalibrationMath.h"

namespace nsw{

class CalibrationSca{ 

public:
CalibrationSca();
~CalibrationSca() {};

void read_config(std::string config_filename, 
									std::string fe_name,
									bool full_set,
									std::set<std::string> &frontend_names,
									std::vector<std::string> &fe_names_v,
									std::vector<nsw::FEBConfig> &frontend_configs);

void configure_feb(std::vector<nsw::FEBConfig> frontend_configs, int fe_name_sorted); //might be deleted later, better use dedicated NSWConfiguration exe

int calculate_thdac_value(nsw::ConfigSender & cs,
                          nsw::FEBConfig & feb,
                          nsw::CalibrationMath &cm,
													int vmm_id,
                          int n_samples,
                          int thdac_central_guess,
													std::tuple<std::string, float, float> &thdac_constants,
                          std::vector<int> & thdac_guess_variations,
													bool stgc,
													bool debug);

std::pair<float,int> find_linear_region_slope(nsw::ConfigSender & cs,
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
														);

std::vector<float> read_baseline(nsw::ConfigSender &cs,
												         nsw::FEBConfig &feb,
																 nsw::CalibrationMath &cm,
												         int i_vmm,
												         int channel_id,
												         int n_samples,
												         std::vector<float> & fe_samples_tmp,
												         std::map< std::pair< std::string,int>, float> & channel_baseline_med,
												         std::map< std::pair< std::string,int>, float> & channel_baseline_rms,
												         bool debug,
																 bool stgc,
																 std::vector<float> &n_over_cut,
												         int RMS_CUTOFF);

std::vector<float> vmm_averages(nsw::ConfigSender &cs,
															  nsw::FEBConfig &feb,
															  int i_vmm,
															  int channel_id,
															  int n_samples,
															  int trim_mid,
															  int thdac,
															  std::vector<float> & fe_samples_tmp);


std::map< std::pair< std::string,int>, int>  analyse_trimmers(nsw::ConfigSender &cs,
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
																												     //  std::vector<float> & trim_perf,
																												       bool recalc,
																												       bool stgc,
																												       bool debug
																															);

void sca_calib( std::string config_filename,
           std::vector<nsw::FEBConfig> frontend_configs, // if here i use type "auto" for frontend_configs -> get overloaded function call error from cmake!
				   std::string fe_name,
           std::string io_config_path,
           int fe_name_sorted,
           int n_samples,
//           bool pFEB,
           bool debug,
           int rms_factor);

void read_baseline_full(std::string config_filename,
			                 std::vector<nsw::FEBConfig> frontend_configs,
                       std::string io_config_path,
                       int n_samples,
                       int fe_name_sorted,
  //                     bool pFEB,
                       std::string fe_name,
                       bool conn_check);

std::vector<short unsigned int> ch_threshold(nsw::ConfigSender &cs,
                                     nsw::FEBConfig &feb,
                                     int vmm_id,
                                     int channel_id,
                                     int n_samples);

void read_thresholds(std::string config_filename,
		                   std::vector<nsw::FEBConfig> frontend_configs,
                       std::string io_config_path,
                       int n_samples,
                       int fe_name_sorted,
   //                    bool pFEB,
                       bool debug,
                       std::string fe_name);

void calib_pulserDAC( std::vector<nsw::FEBConfig> frontend_configs,
		                  std::string io_config_path,
                  		std::string fe_name,
									     int n_samples,
                       int fe_name_sorted,
                       bool debug);

void merge_json(std::string &mod_json, std::string io_config_path, std::string config_filename, int rms, bool split_config);

void send_pulses(std::vector<nsw::FEBConfig>, int fe_name_sorted, int thdac, int tpdac_i, bool debug);

void turn_off_pulser(std::vector<nsw::FEBConfig>, int fe_name_sorted, bool debug);

	};
}
#endif
