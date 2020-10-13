//class for mathematical operations

#ifndef CALIBRATIONMATH_H_
#define CALIBRATIONMATH_H_

#include <iostream> 
#include <sys/types.h>
#include <string>
#include <cstring>
#include <vector>
#include <iomanip>
#include <fstream>
#include <numeric>
#include <set>
#include <map>
#include <stdio.h>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

namespace nsw{

class CalibrationMath{

public:
CalibrationMath();
~CalibrationMath() {};

float take_median(std::vector<short unsigned int> &v);

float take_mode(std::vector<short unsigned int> &v);

float take_median(std::vector<float> &v);

float take_rms(std::vector<short unsigned int> &v, float mean);

float take_rms(std::vector<float> &v, float mean);

float sample_to_mV(float sample, bool stgc);

float sample_to_mV(short unsigned int sample, bool stgc);

float mV_to_sample(float mV_read, bool stgc);

bool check_channel(float ch_baseline_med, float ch_baseline_rms, float vmm_baseline_med, bool stgc);

bool check_slopes(float m1, float m2, float slope_check_val);

std::pair<float,float> get_slopes(float ch_lo,
																	float ch_mid,
																	float ch_hi,
																	int trim_hi, /*= TrimHiTRIM_HI*/
																	int trim_mid, /*= TrimMidTRIM_MID*/
																	int trim_lo/* = TrimLoTRIM_LO*/);

};

namespace ref_val{

	const int NchPerVmm = 64;
	const float RmsCutoff = 30;
	const	float BaselineCutoff = 10;//20//original;
	const	float SlopeCheck = 1/1.5/1000.0*4095.0;
	const	int TrimMid = 14;
	const	int TrimLo = 0;
	const	int	TrimHi = 31;
	const	int ChanThreshCutoff = 1;

}

}
#endif
