#include "NSWCalibration/CalibrationMath.h"

std::size_t nsw::CalibrationMath::mVtoSample(const float mV_read, const bool stgc)
{
  auto val = mV_read * nsw::ref::MV_PER_SAMPLE;
  if (!stgc) {
    val /= nsw::ref::MM_RESISTOR_FACTOR;
  }
  return static_cast<std::size_t>(val);
}


bool nsw::CalibrationMath::checkChannel(const float ch_baseline_rms, const bool stgc)
{
  // FIXME CHECK OVERUSAGE OF RMS_CUTOFF!!!!!!!
  return !(sampleTomV(ch_baseline_rms, stgc) > nsw::ref::RMS_CUTOFF);
}


bool nsw::CalibrationMath::checkSlopes(const float m1, const float m2, const float slope_check_val)
{
  // FIXME TODO adapt for both MM and sTGC
  return (std::abs(m1 - m2) < slope_check_val);
}

std::pair<float, float> nsw::CalibrationMath::getSlopes(
  const std::pair<float, int>& points_trim_low,
  const std::pair<float, int>& points_trim_mid,
  const std::pair<float, int>& points_trim_hi)
{
  const auto m1 =
    (points_trim_hi.first - points_trim_mid.first) /
    (static_cast<float>(points_trim_hi.second) - static_cast<float>(points_trim_mid.second));
  const auto m2 =
    (points_trim_mid.first - points_trim_low.first) /
    (static_cast<float>(points_trim_mid.second) - static_cast<float>(points_trim_low.second));
  return std::make_pair(m1, m2);
}
