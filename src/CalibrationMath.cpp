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

//#include "include/CalibrationSca.h"
//#include "include/CalibrationMath.h"

#include "NSWCalibration/CalibrationSca.h"
#include "NSWCalibration/CalibrationMath.h"

nsw::CalibrationMath::CalibrationMath(){
}

float nsw::CalibrationMath::take_median(std::vector<float> &v){
  size_t n = v.size() / 2;
  std::nth_element(v.begin(), v.begin()+n, v.end());
  float median = v[n];
  return median;
}

float nsw::CalibrationMath::take_median(std::vector<short unsigned int> &v) {
  size_t n = v.size() / 2;
  std::nth_element(v.begin(), v.begin()+n, v.end());
  float median = v[n];
  return median;
}

float nsw::CalibrationMath::sample_to_mV(float sample, bool stgc){
	float val;
  if(stgc){val = sample * 1000. / 4095.0;} //stgc doesn`t have input resistor
  else{val = sample * 1000. *1.5/ 4095.0;} //1,5 is due to a resistor
	return val;
}
//############################################################################
float nsw::CalibrationMath::take_mode(std::vector<short unsigned int> &v) {

	std::sort(v.begin(),v.end());
//	std::vector<short unsigned int> n_modes;
	int counter = 0;
	int mode_count_max = 0;
	short unsigned int mode = 0;
	for(short unsigned int i = 0; i< v.size(); i++)
	{
		counter++;
///		std::cout<<v[i]<<" - "<<counter<<std::endl;
		if(i==0){continue;}
		if(v[i]!=v[i+1]){
			if(mode_count_max < counter){
//				std::cout<<"max count"<<mode_count_max<<std::endl;
				mode_count_max = counter;
				mode=v[i];
//				std::cout<<"max count"<<mode_count_max<<" - "<<counter<<" - "<<std::endl;
//				if(i==v.size()-1){n_modes.push_back(mode);}
				counter=0;
			}
			else if(mode_count_max == counter){
				mode = v[i];//prev_sample_val;
//				n_modes.push_back(mode);
				
//				std::cout<<"max count"<<mode_count_max<<" - "<<counter<<" - "<<std::endl;
				counter=0;
			}
			else{counter=0;continue;}
		}
		else{continue;}			
	}
//	int nr_of_modes = n_modes.size();
//	if(nr_of_modes>1){
//		std::cout<<" found few modes ["<<nr_of_modes<<"]"<<std::endl;
//		for(auto &m: n_modes){std::cout<<"-"<<m<<"-";}
//	}
//	std::cout<<" << MODE is"<<mode<<" >> with <<"<<mode_count_max<<" counts >>"<<std::endl;

////--------------------------------------------------------------------------
//	int nr = v[0];
//  float mode = nr;
//  short unsigned int n = v.size() / 2;
//	std::sort(v.begin(),v.end());
//	int count = 1;
//	int mode_count = 1;
//	for(int i=0; i<n; i++)
//	{
//		if(v[i]==nr){count++;}
//		else
//		{
//			if(count>mode_count)
//			{
//				mode_count = count;
//				mode = nr;
//			}
//			count=1;
//			nr = v[i];
//		}	
//	}
////-----------------------------------------------------------------------------
  return mode;
}
//############################################################################

float nsw::CalibrationMath::sample_to_mV(short unsigned int sample, bool stgc){
	float val;
  if(stgc){val = sample * 1000. / 4095.0;} //no resistor
  else{val = sample * 1000. *1.5 / 4095.0;} //1.5 is due to a resistor
	return val;
}

float nsw::CalibrationMath::mV_to_sample(float mV_read, bool stgc){
	float val;
  if(stgc){val = mV_read / 1000. * 4095.0;} //1.5 is due to a resistor
  else{val = mV_read / 1000. /1.5 * 4095.0;} //1.5 is due to a resistor
	return val;
}

float nsw::CalibrationMath::take_rms(std::vector<float> &v, float mean) {
  float sq_sum = std::inner_product(v.begin(), v.end(), v.begin(), 0.0);
  float stdev = std::sqrt(sq_sum / v.size() - mean * mean);
  return stdev;
}

float nsw::CalibrationMath::take_rms(std::vector<short unsigned int> &v, float mean) {
  float sq_sum = std::inner_product(v.begin(), v.end(), v.begin(), 0.0);
  float stdev = std::sqrt(sq_sum / v.size() - mean * mean);
  return stdev;
}

bool nsw::CalibrationMath::check_channel(float ch_baseline_med, float ch_baseline_rms, float vmm_baseline_med, bool stgc){
  if (sample_to_mV(ch_baseline_rms, stgc) > nsw::ref_val::RmsCutoff/*RMS_CUTOFF*/)
    return false;
  return true;
}

std::pair<float,float> nsw::CalibrationMath::get_slopes(float ch_lo,
                                  float ch_mid,
                                  float ch_hi,
                                //  int trim_hi  = TRIM_HI,
                                 /* int trim_hi  = nsw::ref_val::TrimHi,
                                  int trim_mid = nsw::ref_val::TrimMid,
                                  int trim_lo  = nsw::ref_val::TrimLo*/
																	int trim_hi,
																	int trim_mid,
																	int trim_lo){
  float m1 = (ch_hi - ch_mid)/(trim_hi-trim_mid);
  float m2 = (ch_mid - ch_lo)/(trim_mid-trim_lo);
  return std::make_pair(m1,m2);
}

bool nsw::CalibrationMath::check_slopes(float m1, float m2, float slope_check_val){
  if ( fabs(m1 - m2) > slope_check_val/*SLOPE_CHECK*/ )
    return false;
  return true;
}


