#include "NSWCalibration/Utility.h"

#include <ctime>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <sstream>

#include <fmt/core.h>

std::string nsw::calib::utils::strf_time()
{
  std::stringstream ss;
  auto              result = std::time(nullptr);
  auto              tm     = *std::localtime(&result);
  ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
  return ss.str();
}

std::string nsw::calib::utils::serializeCommands(
  const std::vector<nsw::commands::Command>& commands) {
  if (commands.empty()) {
    return "";
  }
  return std::accumulate(std::next(std::cbegin(commands)),
                         std::cend(commands),
                         commandToString(commands.at(0)),
                         [](std::string ss, const nsw::commands::Command& command) {
                           return fmt::format("{};{}", std::move(ss), commandToString(command));
                         });
}

std::string nsw::calib::utils::commandToString(
  const nsw::commands::Command& command) {
  return std::accumulate(std::cbegin(command.args),
                         std::cend(command.args),
                         command.command,
                         [](std::string ss, const std::string& s) {
                           return fmt::format("{}:{}", std::move(ss), s);
                         });
}
