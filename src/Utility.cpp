#include "NSWCalibration/Utility.h"

#include <ctime>
#include <numeric>

#include <fmt/core.h>
#include <fmt/chrono.h>

std::string nsw::calib::utils::strf_time()
{
  return fmt::format("{:%Y_%m_%d_%Hh%Mm%Ss}", fmt::localtime(std::time(nullptr)));
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
