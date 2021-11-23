#ifndef NSWCALIBRATION_ISSUES_H
#define NSWCALIBRATION_ISSUES_H

#include <fmt/format.h>

#include <ers/Issue.h>

namespace nsw {
    /**
     * \brief Declares generic issue
     *
     * \param condition is the condition triggering the issue
     * \param reason is the reason the issue was triggered
     */
    ERS_DECLARE_ISSUE(calib,
                      Issue,
                      fmt::format("{}\n{}", condition, reason),
                      ((std::string)condition)
                      ((std::string)reason)
                      )

    /**
     * \brief Used to declare that a connection with a front-end board has been lost
     *
     * \param fe is name of the front-end board
     * \param err is the exception message caught indicating the failure
     */
    ERS_DECLARE_ISSUE(calib,
                      LostConnection,
                      fmt::format("Lost connection to {} because {} exception.", fe, err),
                      ((std::string)fe)
                      ((std::string)err)
    )

    /**
     * \brief Declares an issue with extracting a parameter from IS
     *
     * \param paramName name of the parameter to find in IS
     * \param setCommand example is_write command to print in the log
     */
    ERS_DECLARE_ISSUE(calib,
                      IsParameterNotFound,
                      fmt::format("'{}' not found in IS. Perhaps you forgot: {}", paramName, setCommand),
                      ((std::string)paramName)
                      ((std::string)setCommand)
                      )

    /**
     * \brief Declares an issue with extracting a parameter from IS, but with a default value set
     *
     * \param paramName name of the parameter to find in IS
     * \param setCommand example is_write command to print in the log
     * \param paramDefault is the default value of the parameter that will be used
     */
    ERS_DECLARE_ISSUE(calib,
                      IsParameterNotFoundUseDefault,
                      fmt::format("'{}' not found in IS. Perhaps you forgot: {}. Defaulting to: {}", paramName, setCommand, paramDefault),
                      ((std::string)paramName)
                      ((std::string)setCommand)
                      ((std::string)paramDefault)
                      )
}

#endif
