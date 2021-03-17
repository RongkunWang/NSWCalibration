#include "NSWCalibration/Utility.h"

#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>

std::string nsw::calib::utils::strf_time()
{
  std::stringstream ss;
  auto              result = std::time(nullptr);
  auto              tm     = *std::localtime(&result);
  ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
  return ss.str();
}
