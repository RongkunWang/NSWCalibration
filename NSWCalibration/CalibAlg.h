#ifndef CALIBALG_H_
#define CALIBALG_H_

//
// Base class for NSW calib algs
//

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

#include <RunControl/Common/RunControlCommands.h>

#include "NSWCalibration/Commands.h"

namespace nsw {

  class CalibAlg {

  public:
    explicit CalibAlg(std::string calibType) : m_calibType(std::move(calibType)) {};
    virtual ~CalibAlg() = default;

    /*!
     * \brief Defines the steps required to prepare the calibration
     *
     * Pure virtual, must be overridden in the derived class
     *
     * \param db Configuration source object for the electronics (JSON
     *        or Oracle)
     */
    virtual void setup(const std::string& db) = 0;

    /*!
     * \brief Defines the steps required to set up the calibration
     *        iteration
     *
     * Pure virtual, must be overridden in the derived class
     */
    virtual void configure() = 0;

    /*!
     * \brief Defines any steps taken during the acquisition period
     *        (most likely empty)
     */
    virtual void acquire() {};

    /*!
     * \brief Defines the steps required to move to the next
     *        calibration iteration
     *
     * \c next() is called after this function
     */
    virtual void unconfigure() {};

    /*!
     * \brief Returns the set of commands and arguments that will be
     *        published to IS for use in the NSWOrchestrator
     *
     * Called in the \c NSWCalibRc::publish function
     *
     * \returns an instance of \c nsw::commands::Commands, defining
     * the requested ALTI interactions for this calibration
     */
    [[nodiscard]]
    virtual nsw::commands::Commands getAltiSequences() const {return {};};

    /*!
     * \brief Increments the iteration counter and checks if there are
     *        more iterations to do
     *
     * Will be called after \c unconfigure
     */
    void next() { ++m_counter; };

    /*!
     * \brief Prints the overall calibration progress
     *
     * Will be called before \c configure
     */
    void progressbar();

    std::size_t counter() const {return m_counter;};
    std::size_t total() const {return m_total;};
    bool wait4swrod() const {return m_wait4swrod;}
    bool simulation() const {return m_simulation;}
    std::string applicationName() const {return m_name;}
    std::uint32_t runNumber() const {return m_run_number;}

    void setCounter(const std::size_t ctr) {m_counter = ctr;}
    void setTotal(const std::size_t tot) {m_total = tot;}
    void setWait4swROD(const bool wait) {m_wait4swrod = wait;}
    void setSimulation(const bool sim) {m_simulation = sim;}
    void setApplicationName(const std::string& name) {m_name = name;}
    void setRunNumber(const std::uint32_t val) {m_run_number = val;}

  private:
    // "progress bar"
    void setStartTime() {m_time_start = std::chrono::system_clock::now();}
    void setElapsedSeconds() {m_elapsed_seconds = std::chrono::system_clock::now() - m_time_start;}
    double elapsedSeconds() const {return m_elapsed_seconds.count();}
    double rate() const {return elapsedSeconds() > 0 ? static_cast<double>(m_counter) / elapsedSeconds() : -1;}
    double remainingSeconds() const {return static_cast<double>(m_total-m_counter)/rate();}

  protected:
    std::string m_calibType;   //!< Calibration type
    std::size_t m_counter{0};  //!< Current iteration counter
    std::size_t m_total{1};    //!< Total number of iterations in the calibration loop
    bool m_wait4swrod{false};  //!< Calibration interacts with SwRod for processing
    bool m_simulation{false};  //!< Calibration is simulated or not

  private:
    std::uint32_t m_run_number{0};  //!< Calibration run number
    std::string m_name;             //!< Calibration application name

    std::chrono::time_point<std::chrono::system_clock> m_time_start;  //!< Calibration start time
    std::chrono::duration<double> m_elapsed_seconds{0};  //!< Duration of the calibration
  };
}

#endif
