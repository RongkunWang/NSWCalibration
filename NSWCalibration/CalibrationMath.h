//class for mathematical operations

#ifndef CALIBRATIONMATH_H_
#define CALIBRATIONMATH_H_

#include <vector>

namespace nsw {

  class CalibrationMath {

  public:
    CalibrationMath() = default;
    ~CalibrationMath() = default;

    static float take_median(std::vector<short unsigned int> &v);

    static float take_mode(std::vector<short unsigned int> &v);

    static float take_median(std::vector<float> &v);

    static float take_rms(std::vector<short unsigned int> &v, float mean);

    static float take_rms(std::vector<float> &v, float mean);

    static float sample_to_mV(float sample, bool stgc);

    static float sample_to_mV(short unsigned int sample, bool stgc);

    static float mV_to_sample(float mV_read, bool stgc);

    static bool check_channel(float ch_baseline_med, float ch_baseline_rms, float vmm_baseline_med, bool stgc);

    static bool check_slopes(float m1, float m2, float slope_check_val);

    static std::pair<float,float> get_slopes(float ch_lo,
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
