#ifndef NSWCALIBRATION_UTILITY_H_
#define NSWCALIBRATION_UTILITY_H_

#include <string>

#include "NSWCalibration/Commands.h"

namespace nsw {
  namespace calib {
    namespace utils {

      std::string strf_time();

      /**
       * \brief Serialize a vector of commands to a semicolon-delimited string
       *
       * The commands and their possible arguments will be serialzed
       * using ';' as the command delimiter and ':' as the argument
       * delimiter.
       *
       * E.g. to send a BCR and then a StartPG command, the serialized
       * result of the input commands would be:
       *   - \tt "SendAsyncShortCommand:0x01;StartPatternGenerator"
       *
       * \param commands is a vector of commands, in the example given:
       *        - \tt {{"SendAsyncShortCommand","0x01"},{"StartPatternGenerator"}}
       *
       * \returns std::string containing the serialized commands
       */
      std::string serializeCommands(const std::vector<nsw::commands::Command>& commands);

      /**
       * \brief Converts an \c nsw::commands::Command to a string of
       *        the form \tt CommandName:CommandArg1:CommandArg2:CommandArg3
       *
       * \param command Object containing the command name and any
       *        arguments to that command
       *
       * \returns std::string containing the command name and any
       *          arguments, delimited by ':'
       */
      std::string commandToString(const nsw::commands::Command& command);

    }  // namespace utils
  }    // namespace calib
}  // namespace nsw

#endif
