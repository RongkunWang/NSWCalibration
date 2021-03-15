//class for mathematical operations

#ifndef CALIBRATIONMATH_H_
#define CALIBRATIONMATH_H_

#include <vector>

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

    constexpr float SAMPLES_PER_MV = 1000./4095.; ///< 12-bit ADC with 1 V max range
    constexpr float MV_PER_SAMPLE  = 1./SAMPLES_PER_MV; ///< 12-bit ADC with 1 V max range
    constexpr float MM_RESISTOR_FACTOR = 1.5; ///< 1.5 is due to a resistor on MMs
    constexpr int NchPerVmm = 64;
    constexpr float RmsCutoff = 30;
    constexpr float BaselineCutoff = 10;//20//original;
    constexpr float SlopeCheck = 1/1.5/1000.0*4095.0;
    constexpr int TrimMid = 14;
    constexpr int TrimLo = 0;
    constexpr int TrimHi = 31;
    constexpr int ChanThreshCutoff = 1;

  }

}
#endif
