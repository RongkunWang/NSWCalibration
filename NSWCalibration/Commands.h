#ifndef NSWCALIBRATION_COMMANDS_H
#define NSWCALIBRATION_COMMANDS_H

#include <array>
#include <string>
#include <vector>

namespace nsw::commands {
  struct Command {
    std::string              command{};
    std::vector<std::string> args{};

    bool operator==(const Command& other) const {
      return command == other.command and args == other.args;
    }
  };
  struct Commands {
    std::vector<Command> before{};
    std::vector<Command> during{};
    std::vector<Command> after{};

    bool operator==(const Commands& other) const {
      return before == other.before and during == other.during and
             after == other.after;
    }

    /*!
     * \brief Get the list of commands to be executed by the
     *        AltiController for each calibration iteration
     *
     * Each calibration iteration consists of three periods where ALTI
     * commands may be executed:
     *  - Before the configure step is executed
     *  - After the configure step (before the acquire) is executed
     *  - After the acquire step (before the unconfigure) is executed
     *
     * \returns An array containing the IS location and commands for
     *          each of the defined periods
     */
    [[nodiscard]]
    std::array<std::pair<std::string, std::vector<Command>>, 3> getCommands() const {
      return {std::make_pair("NswParams.Calib.AltiCommands.Before", before),
              std::make_pair("NswParams.Calib.AltiCommands.During", during),
              std::make_pair("NswParams.Calib.AltiCommands.After", after)};
    }
  };
  namespace internal {
    /*! \namespace nsw::internal
     * \brief Defines commands. Not to be used directly
     */
    const std::string StartPGIfNotBusy     = "StartPatternGeneratorIfNotBusy";
    const std::string StartPG              = "StartPatternGenerator";
    const std::string StopPG               = "StopPatternGenerator";
    const std::string SetPGRepeat          = "SetPatternGeneratorRepeat";
    const std::string SendAsyncShortCmd    = "SendAsyncShortCommand";
    const std::vector<std::string> ArgsBCR = {"0x01"};
    const std::vector<std::string> ArgsECR = {"0x02"};
    const std::vector<std::string> ArgsOCR = {"0x80"};
    const std::vector<std::string> ArgsSR  = {"0x08"};
    const std::vector<std::string> ArgsFalse = {"0x0"};
    const std::vector<std::string> ArgsTrue  = {"0x1"};
  }  // namespace internal

  const auto actionStartPG          = Command{internal::StartPG};
  const auto actionStartPGIfNotBusy = Command{internal::StartPGIfNotBusy};
  const auto actionStopPG           = Command{internal::StopPG};
  const auto actionSetPGRepeat      = Command{internal::SetPGRepeat, internal::ArgsTrue};
  const auto actionUnsetPGRepeat    = Command{internal::SetPGRepeat, internal::ArgsFalse};
  const auto actionBCR              = Command{internal::SendAsyncShortCmd, internal::ArgsBCR};
  const auto actionECR              = Command{internal::SendAsyncShortCmd, internal::ArgsECR};
  const auto actionOCR              = Command{internal::SendAsyncShortCmd, internal::ArgsOCR};
  const auto actionSR               = Command{internal::SendAsyncShortCmd, internal::ArgsSR};

  const std::array availableCommands{actionStartPG,
                                     actionStartPGIfNotBusy,
                                     actionStopPG,
                                     actionSetPGRepeat,
                                     actionUnsetPGRepeat,
                                     actionBCR,
                                     actionECR,
                                     actionOCR,
                                     actionSR};
}  // namespace nsw::commands

#endif
