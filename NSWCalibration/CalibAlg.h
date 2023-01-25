#ifndef CALIBALG_H_
#define CALIBALG_H_

//
// Base class for NSW calib algs
//

#include <cstdint>
#include <functional>
#include <string>
#include <chrono>
#include <filesystem>
#include <vector>

#include <nlohmann/json.hpp>

#include <ipc/partition.h>
#include <is/infodictionary.h>

#include "NSWCalibration/Commands.h"

#include "NSWConfiguration/hw/DeviceManager.h"

class ISInfoDictionary;

using json = nlohmann::json;

namespace nsw {

  class CalibAlg {

  public:
    CalibAlg(std::string calibType, const hw::DeviceManager& deviceManager);
    CalibAlg(std::string, hw::DeviceManager&&) = delete;

    virtual ~CalibAlg() = default;
    CalibAlg(const CalibAlg&) = default;
    CalibAlg(CalibAlg&&) = default;
    CalibAlg& operator=(CalibAlg&&) = default;
    CalibAlg& operator=(const CalibAlg&) = default;

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

    /**
     * \brief Tell orchestrator whether to increase LB between iterations
     *
     * \return true Increase LB
     * \return false Do not increase LB
     */
    [[nodiscard]] virtual bool increaseLbBetweenIterations() const { return false; }

    /*!
     * \brief Implements a method to extract calibration parameters from IS
     *
     * Will be called during the \c reset UserCmd, which is executed
     * prior to starting any calibration loop.
     *
     * \param is_dictionary Reference to the ISInfoDictionary from the RC application
     * \param is_db_name Name of the IS server storing the parameters
     */
    virtual void setCalibParamsFromIS(const ISInfoDictionary& is_dictionary, const std::string& is_db_name);
    virtual void setCalibKeyToIS(const ISInfoDictionary&){};

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

    /*!
     * \brief Obtain the current run number, start of run time, and set the run
     *        string based on the RunParams ISInfoDictionary
     *
     * Sets m_run_string to either the current run number (m_run_number)
     * in the format runXXXXXXXX, or the start of run time (m_run_start)
     * in the format YY_mm_dd_HHhMMmSSs
     */
    void setCurrentRunParameters(const std::pair<std::uint32_t, std::time_t>& params);

  protected:
    [[nodiscard]] const hw::DeviceManager& getDeviceManager() const { return m_deviceManager.get(); }

    /*!
     * \brief Construct the output path for calibration output files for a given run
     *
     * The format will be the base output directory taken from OKS
     * (m_out_path), the calibration type (m_calibType), and the run
     * identifier (m_run_string)
     *
     * \returns std::filesystem::path corresponding to the location
     *          output files will be written for this run
     */
    [[nodiscard]]
    std::filesystem::path getOutputDir() const { return std::filesystem::path(m_out_path) / std::filesystem::path(m_calibType) / std::filesystem::path(m_run_string); }

    /*!
     * \brief Construct the full path for an output file
     *
     * The format will be the base output directory taken from OKS
     * (m_out_path), the calibration type (m_calibType), the run
     * identifier (m_run_string), and finally the filename (fname)
     *
     * \param fname Name of the output file
     *
     * \returns std::filesystem::path corresponding to the output file
     */
    [[nodiscard]]
    std::filesystem::path getOutputPath(const std::string& fname) const { return getOutputDir()/fname;}

    /**
     * \brief Get LBs as a JSON
     *
     * \return JSON objects containing LBs per iteration
     */
    [[nodiscard]] json getLbJson() const;

    /**
     * @brief Get the current LB
     */
    [[nodiscard]] int getLumiBlock() const;

    /**
     * @brief Start a new LB
     */
    void updateLumiBlockMap(int lumiBlockStart, int lumiBlockEnd);

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
    std::uint32_t m_run_number{0};  //!< Calibration run number from partition
    std::time_t m_run_start{};      //!< Calibration run start from partition
    std::string m_run_string;       //!< String representation of the run number or timestamp

  private:
    std::reference_wrapper<const hw::DeviceManager> m_deviceManager;  //!< Device Manager
    std::string m_name;      //!< Calibration application nam
    std::string m_out_path;  //!< Calibration output base path, taken from OKS

    std::chrono::time_point<std::chrono::system_clock> m_time_start;  //!< Calibration start time
    std::chrono::duration<double> m_elapsed_seconds{0};  //!< Duration of the calibration

    IPCPartition m_ipcPartition;
    ISInfoDictionary m_isDict;
    std::map<std::size_t, std::pair<int, int>> m_lbs{};

  };
}

#endif
