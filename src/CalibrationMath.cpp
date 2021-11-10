#include "NSWCalibration/CalibrationMath.h"

std::size_t nsw::CalibrationMath::mVtoSample(const float mV_read, const bool stgc)
{
  float val = mV_read * nsw::ref::MV_PER_SAMPLE;
  if (!stgc) {
    val /= nsw::ref::MM_RESISTOR_FACTOR;
  }
  return std::size_t{val};
}


bool nsw::CalibrationMath::checkChannel(const float ch_baseline_rms, const bool stgc)
{
  // FIXME CHECK OVERUSAGE OF RMS_CUTOFF!!!!!!!
  return !(sampleTomV(ch_baseline_rms, stgc) > nsw::ref::RMS_CUTOFF);
}


std::pair<float,float> nsw::CalibrationMath::getSlopes(const float ch_lo,
                                                       const float ch_mid,
                                                       const float ch_hi,
                                                       const int trim_hi,
                                                       const int trim_mid,
                                                       const int trim_lo)
{
  const float m1 = (ch_hi - ch_mid)/(trim_hi-trim_mid);
  const float m2 = (ch_mid - ch_lo)/(trim_mid-trim_lo);
  return std::make_pair(m1,m2);
}


bool nsw::CalibrationMath::checkSlopes(const float m1, const float m2, const float slope_check_val)
{
  // FIXME TODO adapt for both MM and sTGC
  return (std::abs(m1 - m2) < slope_check_val);
}
